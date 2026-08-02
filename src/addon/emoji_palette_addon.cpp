#include "emoji_palette/catalog.hpp"
#include "emoji_palette/geometry.hpp"
#include "emoji_palette/ipc/protocol.hpp"
#include "emoji_palette/ipc/session.hpp"
#include "emoji_palette/selection_controller.hpp"
#include "key_router.hpp"
#include "shortcut_matcher.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <dbus_public.h>
#include <fcitx-config/configuration.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/dbus/bus.h>
#include <fcitx-utils/dbus/matchrule.h>
#include <fcitx-utils/dbus/message.h>
#include <fcitx-utils/dbus/servicewatcher.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/instance.h>

namespace {

using emoji_palette::ipc::CancelReason;
using emoji_palette::ipc::Command;
using emoji_palette::ipc::CommandKind;
using emoji_palette::ipc::Envelope;
using emoji_palette::ipc::Hello;
using emoji_palette::ipc::Hide;
using emoji_palette::ipc::Selected;
using emoji_palette::ipc::Show;
using emoji_palette::ipc::TransactionId;
using emoji_palette::ipc::Welcome;

FCITX_DEFINE_LOG_CATEGORY(paletteLogCategory, "emojipalette")

// Geometry and frontend identity only. Search text and selected sequences
// are never written to the log.
#define EMOJI_PALETTE_PLACEMENT() FCITX_LOGC(paletteLogCategory, Debug)

constexpr char serviceName[] = "org.fcitx.Fcitx5.EmojiPalette1";
constexpr char objectPath[] = "/org/fcitx/Fcitx5/EmojiPalette1";
constexpr char interfaceName[] = "org.fcitx.Fcitx5.EmojiPalette1";
constexpr std::uint32_t addonCapabilities =
    static_cast<std::uint32_t>(emoji_palette::ipc::Capability::PointerSelection) |
    static_cast<std::uint32_t>(emoji_palette::ipc::Capability::LocalizedSearch);

emoji_palette::Locale currentLocale() {
    const char* value = std::getenv("LC_ALL");
    if (value == nullptr || *value == '\0') {
        value = std::getenv("LC_MESSAGES");
    }
    if (value == nullptr || *value == '\0') {
        value = std::getenv("LANG");
    }
    const std::string_view locale = value == nullptr ? std::string_view{} : value;
    if (locale.starts_with("de")) {
        return emoji_palette::Locale::German;
    }
    if (locale.starts_with("ru")) {
        return emoji_palette::Locale::Russian;
    }
    return emoji_palette::Locale::English;
}

std::uint16_t scalePercent(double scaleFactor) {
    if (!std::isfinite(scaleFactor) || scaleFactor <= 0.0) {
        return 100;
    }
    const auto percent = static_cast<int>(std::lround(scaleFactor * 100.0));
    return static_cast<std::uint16_t>(
        std::clamp(percent, static_cast<int>(emoji_palette::minimumScalePercent),
                   static_cast<int>(emoji_palette::maximumScalePercent)));
}

TransactionId createTransaction() {
    std::random_device source;
    TransactionId result;
    do {
        for (auto& byte : result.bytes) {
            byte = static_cast<std::uint8_t>(source());
        }
    } while (std::all_of(result.bytes.begin(), result.bytes.end(),
                         [](std::uint8_t byte) { return byte == 0; }));
    return result;
}

std::uint64_t createNonce() {
    std::random_device source;
    std::uint64_t result = 0;
    do {
        result =
            (static_cast<std::uint64_t>(source()) << 32U) | static_cast<std::uint64_t>(source());
    } while (result == 0);
    return result;
}

class FcitxCommitTarget final : public emoji_palette::CommitTarget {
  public:
    explicit FcitxCommitTarget(fcitx::InputContext& context) : context_(context.watch()) {}

    bool available() const override { return context_.isValid(); }

    bool focused() const override {
        const auto* context = context_.get();
        return context != nullptr && context->hasFocus();
    }

    void commit(std::string_view sequence) override {
        if (auto* context = context_.get()) {
            context->commitString(std::string(sequence));
        }
    }

  private:
    fcitx::TrackableObjectReference<fcitx::InputContext> context_;
};

FCITX_CONFIGURATION(EmojiPaletteConfig,
                    fcitx::KeyListOption triggerKey{this,
                                                    "TriggerKey",
                                                    _("Trigger Key"),
                                                    {fcitx::Key("Super+period")},
                                                    fcitx::KeyListConstrain()};
                    fcitx::Option<bool> closeAfterSelection{this, "CloseAfterSelection",
                                                            _("Close after selection"), true};);

class EmojiPaletteAddon final : public fcitx::AddonInstance {
  public:
    explicit EmojiPaletteAddon(fcitx::Instance* instance)
        : instance_(instance), dbus_(instance_->addonManager().addon("dbus")),
          bus_(dbus_->call<fcitx::IDBusModule::bus>()), watcher_(*bus_), controller_(catalog_) {
        frameMatch_ =
            bus_->addMatch(fcitx::dbus::MatchRule(serviceName, objectPath, interfaceName, "Frame"),
                           [this](fcitx::dbus::Message& message) { return receiveFrame(message); });
        watcherEntry_ = watcher_.watchService(
            serviceName,
            [this](const std::string&, const std::string& oldOwner, const std::string& newOwner) {
                if (!oldOwner.empty() && session_.accepts(oldOwner)) {
                    session_.ownerLost(oldOwner);
                    cancel(CancelReason::HelperDisconnected, false);
                }
                if (newOwner.empty()) {
                    pendingCall_.reset();
                }
            });

        eventHandlers_.emplace_back(instance_->watchEvent(
            fcitx::EventType::InputContextKeyEvent, fcitx::EventWatcherPhase::PreInputMethod,
            [this](fcitx::Event& event) {
                auto& keyEvent = static_cast<fcitx::KeyEvent&>(event);
                if (controller_.active()) {
                    handleActiveKey(keyEvent);
                    return;
                }
                if (!keyEvent.isRelease() &&
                    shortcutMatcher_.matches(keyEvent.key(), keyEvent.origKey())) {
                    show(*keyEvent.inputContext());
                    keyEvent.filterAndAccept();
                }
            }));
        eventHandlers_.emplace_back(instance_->watchEvent(
            fcitx::EventType::InputMethodGroupChanged, fcitx::EventWatcherPhase::Default,
            [this](fcitx::Event&) { reloadLayout(); }));

        const auto reset = [this](CancelReason reason, fcitx::Event& event) {
            auto& contextEvent = static_cast<fcitx::InputContextEvent&>(event);
            if (contextEvent.inputContext() == origin_.get()) {
                cancel(reason, true);
            }
        };
        eventHandlers_.emplace_back(instance_->watchEvent(
            fcitx::EventType::InputContextFocusOut, fcitx::EventWatcherPhase::Default,
            [reset](fcitx::Event& event) { reset(CancelReason::FocusLost, event); }));
        eventHandlers_.emplace_back(instance_->watchEvent(
            fcitx::EventType::InputContextReset, fcitx::EventWatcherPhase::Default,
            [reset](fcitx::Event& event) { reset(CancelReason::ContextReset, event); }));
        eventHandlers_.emplace_back(instance_->watchEvent(
            fcitx::EventType::InputContextSwitchInputMethod, fcitx::EventWatcherPhase::Default,
            [reset](fcitx::Event& event) { reset(CancelReason::InputMethodChanged, event); }));
        eventHandlers_.emplace_back(instance_->watchEvent(
            fcitx::EventType::InputContextDestroyed, fcitx::EventWatcherPhase::Default,
            [reset](fcitx::Event& event) { reset(CancelReason::ContextDestroyed, event); }));

        reloadConfig();
    }

    void reloadConfig() override {
        fcitx::readAsIni(config_, "conf/emojipalette.conf");
        reloadShortcuts();
    }

    const fcitx::Configuration* getConfig() const override { return &config_; }

    void setConfig(const fcitx::RawConfig& config) override {
        config_.load(config, true);
        reloadShortcuts();
        fcitx::safeSaveAsIni(config_, "conf/emojipalette.conf");
    }

  private:
    void reloadShortcuts() {
        shortcutMatcher_.setShortcuts(*config_.triggerKey);
        reloadLayout();
    }

    void reloadLayout() {
        const auto& manager = instance_->inputMethodManager();
        if (manager.groupCount() == 0) {
            return;
        }
        shortcutMatcher_.setLayout(manager.currentGroup().defaultLayout());
        keyRouter_.setLayout(manager.currentGroup().defaultLayout());
    }

    void show(fcitx::InputContext& context) {
        cancel(CancelReason::User, true);
        origin_ = context.watch();
        closeAfterSelection_ = *config_.closeAfterSelection;
        const auto transaction = createTransaction();
        controller_.begin(transaction, std::make_unique<FcitxCommitTarget>(context));
        search_.clear();
        commandSequence_ = 0;
        if (session_.state() == emoji_palette::ipc::SessionState::Ready) {
            sendShow(context, transaction);
        } else {
            negotiate(context, transaction);
        }
    }

    void negotiate(fcitx::InputContext& context, TransactionId transaction) {
        const auto nonce = createNonce();
        const Envelope hello{emoji_palette::ipc::protocolVersion,
                             Hello{emoji_palette::ipc::protocolVersion,
                                   emoji_palette::ipc::protocolVersion, nonce, addonCapabilities}};
        callExchange(hello, [this, contextReference = context.watch(), transaction,
                             nonce](fcitx::dbus::Message& reply) {
            if (!controller_.active() || controller_.transaction() != transaction) {
                return;
            }
            if (reply.isError()) {
                cancel(CancelReason::HelperDisconnected, false);
                return;
            }
            std::vector<std::uint8_t> frame;
            if (!(reply >> frame)) {
                cancel(CancelReason::ProtocolError, true);
                return;
            }
            const auto parsed = emoji_palette::ipc::parse(frame);
            const auto* welcome =
                parsed.envelope ? std::get_if<Welcome>(&parsed.envelope->payload) : nullptr;
            if (welcome == nullptr) {
                cancel(CancelReason::ProtocolError, true);
                return;
            }
            session_.begin(reply.sender(), nonce, addonCapabilities);
            if (!session_.acceptWelcome(reply.sender(), *welcome)) {
                cancel(CancelReason::ProtocolError, true);
                return;
            }
            auto* current = contextReference.get();
            if (current == nullptr || !current->hasFocus()) {
                cancel(CancelReason::FocusLost, true);
                return;
            }
            sendShow(*current, transaction);
        });
    }

    void sendShow(fcitx::InputContext& context, TransactionId transaction) {
        const auto& cursor = context.cursorRect();
        // A client that sets RelativeRect reports the caret relative to its own
        // window. Only the compositor knows where that window is, so such a
        // rectangle is not a usable absolute position for the helper.
        const bool relative = context.capabilityFlags().test(fcitx::CapabilityFlag::RelativeRect);
        const auto caret = relative
                               ? emoji_palette::absentCaret
                               : emoji_palette::sanitizedCaret({cursor.left(), cursor.top(),
                                                                cursor.width(), cursor.height()});
        const auto scale = scalePercent(context.scaleFactor());
        EMOJI_PALETTE_PLACEMENT() << "Show frontend=" << std::string(context.frontendName())
                                  << " rawCaret=" << cursor.left() << "," << cursor.top() << ","
                                  << cursor.width() << "," << cursor.height()
                                  << " relativeRect=" << relative << " clientScalePercent=" << scale
                                  << " caretUsable=" << (caret != emoji_palette::absentCaret);
        // Show::screen is reserved: Fcitx5 exposes no output geometry, so the
        // helper resolves the output from the caret or from the active output.
        const Envelope envelope{emoji_palette::ipc::protocolVersion,
                                Show{transaction, caret, emoji_palette::absentCaret,
                                     currentLocale(), scale, closeAfterSelection_}};
        sendEnvelope(envelope);
    }

    void sendCommand(CommandKind kind, std::string text = {}) {
        const auto transaction = controller_.transaction();
        if (!transaction) {
            return;
        }
        ++commandSequence_;
        const Envelope envelope{emoji_palette::ipc::protocolVersion,
                                Command{*transaction, commandSequence_, kind, std::move(text)}};
        sendEnvelope(envelope);
    }

    void callExchange(const Envelope& envelope,
                      std::function<void(fcitx::dbus::Message&)> callback) {
        const auto frame = emoji_palette::ipc::serialize(envelope);
        if (!frame) {
            cancel(CancelReason::ProtocolError, false);
            return;
        }
        auto message = bus_->createMethodCall(serviceName, objectPath, interfaceName, "Exchange");
        message << *frame;
        pendingCall_ = message.callAsync(
            5000000, [callback = std::move(callback)](fcitx::dbus::Message& reply) {
                callback(reply);
                return true;
            });
    }

    void sendEnvelope(const Envelope& envelope) {
        const auto frame = emoji_palette::ipc::serialize(envelope);
        if (!frame) {
            cancel(CancelReason::ProtocolError, false);
            return;
        }
        auto message = bus_->createMethodCall(serviceName, objectPath, interfaceName, "Exchange");
        message << *frame;
        if (!message.send()) {
            cancel(CancelReason::HelperDisconnected, false);
        }
    }

    bool receiveFrame(fcitx::dbus::Message& message) {
        if (!session_.accepts(message.sender())) {
            return true;
        }
        std::vector<std::uint8_t> frame;
        if (!(message >> frame)) {
            cancel(CancelReason::ProtocolError, true);
            return true;
        }
        const auto parsed = emoji_palette::ipc::parse(frame);
        if (!parsed.envelope) {
            cancel(CancelReason::ProtocolError, true);
            return true;
        }
        if (const auto* selected = std::get_if<Selected>(&parsed.envelope->payload)) {
            const auto result = controller_.select(*selected);
            if (result == emoji_palette::SelectionResult::Committed) {
                if (closeAfterSelection_) {
                    origin_.unwatch();
                } else if (auto* origin = origin_.get(); origin != nullptr && origin->hasFocus()) {
                    const auto transaction = createTransaction();
                    controller_.begin(transaction, std::make_unique<FcitxCommitTarget>(*origin));
                    commandSequence_ = 0;
                    sendShow(*origin, transaction);
                } else {
                    cancel(CancelReason::FocusLost, true);
                }
            } else if (result != emoji_palette::SelectionResult::TransactionMismatch &&
                       result != emoji_palette::SelectionResult::Inactive) {
                cancel(CancelReason::ProtocolError, true);
            }
        } else if (const auto* cancelled =
                       std::get_if<emoji_palette::ipc::Cancelled>(&parsed.envelope->payload)) {
            if (controller_.transaction() == cancelled->transaction) {
                cancel(cancelled->reason, false);
            }
        }
        return true;
    }

    void cancel(CancelReason reason, bool notifyHelper) {
        const auto transaction = controller_.transaction();
        controller_.cancel(reason);
        origin_.unwatch();
        search_.clear();
        commandSequence_ = 0;
        if (notifyHelper && transaction &&
            session_.state() == emoji_palette::ipc::SessionState::Ready) {
            const Envelope envelope{emoji_palette::ipc::protocolVersion,
                                    Hide{*transaction, reason}};
            sendEnvelope(envelope);
        }
    }

    void handleActiveKey(fcitx::KeyEvent& event) {
        if (!controller_.active() || event.inputContext() != origin_.get()) {
            return;
        }
        // Full input isolation: while the transaction is active, no key event
        // on the source context may reach the input method or the client.
        event.filterAndAccept();
        const auto route =
            keyRouter_.route(event.key(), event.origKey(), event.isRelease(), search_);
        switch (route.action) {
        case emoji_palette::addon::KeyRoute::Action::Ignore:
            break;
        case emoji_palette::addon::KeyRoute::Action::Command:
            sendCommand(route.command);
            break;
        case emoji_palette::addon::KeyRoute::Action::Search:
            search_ = route.searchText;
            sendCommand(CommandKind::SearchText, search_);
            break;
        }
    }

    EmojiPaletteConfig config_;
    fcitx::Instance* instance_;
    fcitx::AddonInstance* dbus_;
    fcitx::dbus::Bus* bus_;
    fcitx::dbus::ServiceWatcher watcher_;
    std::unique_ptr<fcitx::dbus::ServiceWatcherEntry> watcherEntry_;
    std::unique_ptr<fcitx::dbus::Slot> frameMatch_;
    std::unique_ptr<fcitx::dbus::Slot> pendingCall_;
    std::vector<std::unique_ptr<fcitx::HandlerTableEntry<fcitx::EventHandler>>> eventHandlers_;
    emoji_palette::EmojiCatalog catalog_;
    emoji_palette::SelectionController controller_;
    emoji_palette::addon::ShortcutMatcher shortcutMatcher_;
    emoji_palette::addon::ActiveKeyRouter keyRouter_;
    emoji_palette::ipc::PeerSession session_;
    fcitx::TrackableObjectReference<fcitx::InputContext> origin_;
    std::uint32_t commandSequence_ = 0;
    std::string search_;
    bool closeAfterSelection_ = true;
};

class EmojiPaletteAddonFactory final : public fcitx::AddonFactory {
  public:
    fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
        return new EmojiPaletteAddon(manager->instance());
    }
};

}

FCITX_ADDON_FACTORY_V2(emojipalette, EmojiPaletteAddonFactory);
