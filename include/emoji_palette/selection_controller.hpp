#pragma once

#include "emoji_palette/catalog.hpp"
#include "emoji_palette/ipc/protocol.hpp"

#include <memory>
#include <optional>
#include <string_view>

namespace emoji_palette {

class CommitTarget {
  public:
    virtual ~CommitTarget() = default;

    virtual bool available() const = 0;
    virtual bool focused() const = 0;
    virtual void commit(std::string_view sequence) = 0;
};

enum class SelectionResult {
    Committed,
    Inactive,
    TransactionMismatch,
    TargetUnavailable,
    FocusLost,
    InvalidSelection,
};

class SelectionController {
  public:
    explicit SelectionController(const EmojiCatalog& catalog);

    void begin(ipc::TransactionId transaction, std::unique_ptr<CommitTarget> target);
    void cancel(ipc::CancelReason reason);
    SelectionResult select(const ipc::Selected& selected);

    bool active() const;
    std::optional<ipc::TransactionId> transaction() const;
    std::optional<ipc::CancelReason> lastCancelReason() const;

  private:
    const EmojiCatalog& catalog_;
    std::optional<ipc::TransactionId> transaction_;
    std::unique_ptr<CommitTarget> target_;
    std::optional<ipc::CancelReason> lastCancelReason_;
};

}
