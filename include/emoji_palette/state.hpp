#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace emoji_palette {

struct RecentEntry {
    std::string sequence;
    std::uint64_t useCount;
    std::int64_t lastUsed;
};

struct PersistentState {
    std::vector<std::string> favorites;
    std::vector<RecentEntry> recents;
};

struct StateLoadResult {
    PersistentState state;
    bool recoveredFromCorruption;
};

class StateStore {
  public:
    explicit StateStore(std::filesystem::path path);

    StateLoadResult load() const;
    bool save(const PersistentState& state) const;

  private:
    std::filesystem::path path_;
};

void toggleFavorite(PersistentState& state, std::string_view sequence);
void recordUse(PersistentState& state, std::string_view sequence, std::int64_t timestamp,
               std::size_t maximumEntries = 100);

} // namespace emoji_palette
