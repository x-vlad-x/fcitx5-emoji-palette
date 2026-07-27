#include "emoji_palette/selection_controller.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

emoji_palette::ipc::TransactionId transaction(std::uint8_t marker) {
    emoji_palette::ipc::TransactionId value;
    value.bytes.front() = marker;
    return value;
}

struct TargetState {
    bool available = true;
    bool focused = true;
    int commits = 0;
    std::string sequence;
};

class FakeTarget final : public emoji_palette::CommitTarget {
  public:
    explicit FakeTarget(TargetState& state) : state_(state) {}

    bool available() const override { return state_.available; }
    bool focused() const override { return state_.focused; }
    void commit(std::string_view sequence) override {
        ++state_.commits;
        state_.sequence = sequence;
    }

  private:
    TargetState& state_;
};

void commitsExactlyOnce() {
    emoji_palette::EmojiCatalog catalog;
    emoji_palette::SelectionController controller(catalog);
    TargetState target;
    const auto id = transaction(1);
    controller.begin(id, std::make_unique<FakeTarget>(target));

    const emoji_palette::ipc::Selected selected{id, "😀"};
    require(controller.select(selected) == emoji_palette::SelectionResult::Committed,
            "valid selection was not committed");
    require(controller.select(selected) == emoji_palette::SelectionResult::Inactive,
            "duplicate selection remained active");
    require(target.commits == 1 && target.sequence == "😀", "selection committed more than once");
}

void rejectsAfterFocusLoss() {
    emoji_palette::EmojiCatalog catalog;
    emoji_palette::SelectionController controller(catalog);
    TargetState target;
    target.focused = false;
    const auto id = transaction(2);
    controller.begin(id, std::make_unique<FakeTarget>(target));

    require(controller.select({id, "😀"}) == emoji_palette::SelectionResult::FocusLost,
            "unfocused target was accepted");
    require(target.commits == 0 && !controller.active(), "selection committed after focus loss");
    require(controller.lastCancelReason() == emoji_palette::ipc::CancelReason::FocusLost,
            "focus-loss cancellation reason was not retained");
}

void rejectsDestroyedAndUnknownTargets() {
    emoji_palette::EmojiCatalog catalog;
    emoji_palette::SelectionController controller(catalog);
    TargetState target;
    target.available = false;
    const auto id = transaction(3);
    controller.begin(id, std::make_unique<FakeTarget>(target));
    require(controller.select({id, "😀"}) == emoji_palette::SelectionResult::TargetUnavailable,
            "destroyed target was accepted");
    require(target.commits == 0, "selection committed to a destroyed target");

    TargetState second;
    controller.begin(id, std::make_unique<FakeTarget>(second));
    require(controller.select({id, "not-an-emoji"}) ==
                emoji_palette::SelectionResult::InvalidSelection,
            "unknown selection was accepted");
    require(second.commits == 0, "unknown selection was committed");
}

void rejectsCrossTransactionSelection() {
    emoji_palette::EmojiCatalog catalog;
    emoji_palette::SelectionController controller(catalog);
    TargetState target;
    controller.begin(transaction(4), std::make_unique<FakeTarget>(target));
    require(controller.select({transaction(5), "😀"}) ==
                emoji_palette::SelectionResult::TransactionMismatch,
            "cross-transaction selection was accepted");
    require(controller.active() && target.commits == 0,
            "cross-transaction selection altered the active transaction");
}

}

int main() {
    commitsExactlyOnce();
    rejectsAfterFocusLoss();
    rejectsDestroyedAndUnknownTargets();
    rejectsCrossTransactionSelection();
}
