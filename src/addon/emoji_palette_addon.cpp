#include "emoji_palette/catalog.hpp"
#include "emoji_palette/ipc/protocol.hpp"
#include "emoji_palette/ipc/session.hpp"
#include "emoji_palette/selection_controller.hpp"
#include "emoji_palette/utf8.hpp"

#include <algorithm>
#include <array>
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
#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
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
                                                    fcitx::KeyListConstrain()};);

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
            fcitx::EventType::InputContextKeyEvent, fcitx::EventWatcherPhase::Default,
            [this](fcitx::Event& event) {
                auto& keyEvent = static_cast<fcitx::KeyEvent&>(event);
                if (!keyEvent.isRelease() && keyEvent.key().checkKeyList(*config_.triggerKey)) {
                    show(*keyEvent.inputContext());
                    keyEvent.filterAndAccept();
                }
            }));

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
        eventHandlers_.emplace_back(instance_->watchEvent(
            fcitx::EventType::InputContextKeyEvent, fcitx::EventWatcherPhase::PreInputMethod,
            [this](fcitx::Event& event) {
                handleActiveKey(static_cast<fcitx::KeyEvent&>(event));
            }));

        reloadConfig();
    }

    void reloadConfig() override { fcitx::readAsIni(config_, "conf/emoji-palette.conf"); }

    const fcitx::Configuration* getConfig() const override { return &config_; }

    void setConfig(const fcitx::RawConfig& config) override {
        config_.load(config, true);
        fcitx::safeSaveAsIni(config_, "conf/emoji-palette.conf");
    }

  private:
    void show(fcitx::InputContext& context) {
        cancel(CancelReason::User, true);
        origin_ = context.watch();
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
        const auto scale = std::clamp(static_cast<int>(context.scaleFactor() * 100.0), 50, 400);
        const Envelope envelope{emoji_palette::ipc::protocolVersion,
                                Show{transaction,
                                     {cursor.left(), cursor.top(), cursor.width(), cursor.height()},
                                     {0, 0, 0, 0},
                                     currentLocale(),
                                     static_cast<std::uint16_t>(scale),
                                     true}};
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
                origin_.unwatch();
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
        event.filter();
        if (event.isRelease()) {
            return;
        }

        const auto& key = event.key();
        std::optional<CommandKind> command;
        if (key.check(FcitxKey_Escape)) {
            cancel(CancelReason::User, true);
            event.accept();
            return;
        }
        if (key.check(FcitxKey_Return) || key.check(FcitxKey_KP_Enter)) {
            command = CommandKind::Select;
        } else if (key.check(FcitxKey_Left)) {
            command = CommandKind::Left;
        } else if (key.check(FcitxKey_Right)) {
            command = CommandKind::Right;
        } else if (key.check(FcitxKey_Up)) {
            command = CommandKind::Up;
        } else if (key.check(FcitxKey_Down)) {
            command = CommandKind::Down;
        } else if (key.check(FcitxKey_Home)) {
            command = CommandKind::Home;
        } else if (key.check(FcitxKey_End)) {
            command = CommandKind::End;
        } else if (key.check(FcitxKey_Page_Up)) {
            command = CommandKind::PageUp;
        } else if (key.check(FcitxKey_Page_Down)) {
            command = CommandKind::PageDown;
        } else if (key.check(FcitxKey_Tab)) {
            command = key.states().test(fcitx::KeyState::Shift) ? CommandKind::PreviousCategory
                                                                : CommandKind::NextCategory;
        } else if (key.check(FcitxKey_BackSpace)) {
            if (auto codepoints = emoji_palette::decodeUtf8(search_);
                codepoints && !codepoints->empty()) {
                codepoints->pop_back();
                search_ = emoji_palette::encodeUtf8(*codepoints);
            }
            sendCommand(CommandKind::SearchText, search_);
            event.accept();
            return;
        } else if (key.isSimple()) {
            const auto text = fcitx::Key::keySymToUTF8(key.sym());
            if (!text.empty() &&
                search_.size() + text.size() <= emoji_palette::ipc::maximumSearchSize) {
                search_ += text;
                sendCommand(CommandKind::SearchText, search_);
                event.accept();
                return;
            }
        }

        if (command) {
            sendCommand(*command);
            event.accept();
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
    emoji_palette::ipc::PeerSession session_;
    fcitx::TrackableObjectReference<fcitx::InputContext> origin_;
    std::uint32_t commandSequence_ = 0;
    std::string search_;
};

class EmojiPaletteAddonFactory final : public fcitx::AddonFactory {
  public:
    fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
        return new EmojiPaletteAddon(manager->instance());
    }
};

}

FCITX_ADDON_FACTORY_V2_BACKWARDS(emoji_palette, EmojiPaletteAddonFactory);
