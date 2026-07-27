#include "emoji_palette/selection_controller.hpp"

#include "emoji_palette/utf8.hpp"

#include <utility>

namespace emoji_palette {

SelectionController::SelectionController(const EmojiCatalog& catalog) : catalog_(catalog) {}

void SelectionController::begin(ipc::TransactionId transaction,
                                std::unique_ptr<CommitTarget> target) {
    transaction_ = transaction;
    target_ = std::move(target);
    lastCancelReason_.reset();
}

void SelectionController::cancel(ipc::CancelReason reason) {
    transaction_.reset();
    target_.reset();
    lastCancelReason_ = reason;
}

SelectionResult SelectionController::select(const ipc::Selected& selected) {
    if (!transaction_ || !target_) {
        return SelectionResult::Inactive;
    }
    if (selected.transaction != *transaction_) {
        return SelectionResult::TransactionMismatch;
    }
    if (!target_->available()) {
        cancel(ipc::CancelReason::ContextDestroyed);
        return SelectionResult::TargetUnavailable;
    }
    if (!target_->focused()) {
        cancel(ipc::CancelReason::FocusLost);
        return SelectionResult::FocusLost;
    }
    if (selected.sequence.empty() || selected.sequence.size() > ipc::maximumSelectionSize ||
        !isValidUtf8(selected.sequence) || !catalog_.contains(selected.sequence)) {
        cancel(ipc::CancelReason::ProtocolError);
        return SelectionResult::InvalidSelection;
    }

    auto target = std::move(target_);
    transaction_.reset();
    lastCancelReason_.reset();
    target->commit(selected.sequence);
    return SelectionResult::Committed;
}

bool SelectionController::active() const { return transaction_.has_value(); }

std::optional<ipc::TransactionId> SelectionController::transaction() const { return transaction_; }

std::optional<ipc::CancelReason> SelectionController::lastCancelReason() const {
    return lastCancelReason_;
}

}
