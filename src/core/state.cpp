#include "emoji_palette/state.hpp"

#include "emoji_palette/utf8.hpp"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <fcntl.h>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace emoji_palette {
namespace {

constexpr std::size_t maximumStateSize = std::size_t{1024} * 1024;
constexpr std::size_t maximumSequenceSize = 128;
constexpr std::size_t maximumFavorites = 500;
constexpr std::size_t maximumRecents = 1000;

std::vector<std::string_view> fields(std::string_view value) {
    std::vector<std::string_view> result;
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const auto end = value.find('\t', begin);
        result.push_back(value.substr(begin, end == std::string_view::npos ? end : end - begin));
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return result;
}

template <typename Value> bool parseNumber(std::string_view text, Value& value) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

bool validSequence(std::string_view sequence) {
    return !sequence.empty() && sequence.size() <= maximumSequenceSize && isValidUtf8(sequence);
}

std::optional<std::string> readFile(const std::filesystem::path& path, bool& exists) {
    exists = false;
    const int descriptor = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) {
        return std::nullopt;
    }
    exists = true;
    struct stat status{};
    if (fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size < 0 ||
        static_cast<std::uint64_t>(status.st_size) > maximumStateSize) {
        close(descriptor);
        return std::nullopt;
    }
    std::string content(static_cast<std::size_t>(status.st_size), '\0');
    std::size_t position = 0;
    while (position < content.size()) {
        const auto count = read(descriptor, content.data() + position, content.size() - position);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            close(descriptor);
            return std::nullopt;
        }
        position += static_cast<std::size_t>(count);
    }
    if (close(descriptor) != 0) {
        return std::nullopt;
    }
    return content;
}

std::optional<PersistentState> parseState(std::string_view content) {
    PersistentState state;
    std::set<std::string> favorites;
    std::set<std::string> recents;
    std::istringstream stream{std::string(content)};
    std::string line;
    if (!std::getline(stream, line) || line != "emoji-palette-state\t1") {
        return std::nullopt;
    }
    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }
        const auto values = fields(line);
        if (values.size() == 2 && values[0] == "favorite") {
            const auto sequence = hexDecode(values[1]);
            if (!sequence || !validSequence(*sequence) || !favorites.insert(*sequence).second ||
                favorites.size() > maximumFavorites) {
                return std::nullopt;
            }
            state.favorites.push_back(*sequence);
            continue;
        }
        if (values.size() == 4 && values[0] == "recent") {
            std::uint64_t count = 0;
            std::int64_t timestamp = 0;
            const auto sequence = hexDecode(values[3]);
            if (!parseNumber(values[1], count) || count == 0 ||
                !parseNumber(values[2], timestamp) || !sequence || !validSequence(*sequence) ||
                !recents.insert(*sequence).second || recents.size() > maximumRecents) {
                return std::nullopt;
            }
            state.recents.push_back({*sequence, count, timestamp});
            continue;
        }
        return std::nullopt;
    }
    return state;
}

std::optional<std::string> serialize(const PersistentState& state) {
    if (state.favorites.size() > maximumFavorites || state.recents.size() > maximumRecents) {
        return std::nullopt;
    }
    std::set<std::string> favorites;
    std::set<std::string> recents;
    std::ostringstream output;
    output << "emoji-palette-state\t1\n";
    for (const auto& sequence : state.favorites) {
        if (!validSequence(sequence) || !favorites.insert(sequence).second) {
            return std::nullopt;
        }
        output << "favorite\t" << hexEncode(sequence) << '\n';
    }
    for (const auto& recent : state.recents) {
        if (!validSequence(recent.sequence) || recent.useCount == 0 ||
            !recents.insert(recent.sequence).second) {
            return std::nullopt;
        }
        output << "recent\t" << recent.useCount << '\t' << recent.lastUsed << '\t'
               << hexEncode(recent.sequence) << '\n';
    }
    auto value = output.str();
    if (value.size() > maximumStateSize) {
        return std::nullopt;
    }
    return value;
}

bool writeAll(int descriptor, std::string_view value) {
    std::size_t position = 0;
    while (position < value.size()) {
        const auto count = write(descriptor, value.data() + position, value.size() - position);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        position += static_cast<std::size_t>(count);
    }
    return true;
}

}

StateStore::StateStore(std::filesystem::path path) : path_(std::move(path)) {}

StateLoadResult StateStore::load() const {
    bool exists = false;
    const auto content = readFile(path_, exists);
    if (!content) {
        return {{}, exists};
    }
    const auto state = parseState(*content);
    if (!state) {
        return {{}, true};
    }
    return {*state, false};
}

bool StateStore::save(const PersistentState& state) const {
    const auto content = serialize(state);
    if (!content) {
        return false;
    }
    std::error_code error;
    const auto directory =
        path_.parent_path().empty() ? std::filesystem::path(".") : path_.parent_path();
    std::filesystem::create_directories(directory, error);
    if (error) {
        return false;
    }

    std::string pattern = (directory / (path_.filename().string() + ".tmp.XXXXXX")).string();
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    const int descriptor = mkstemp(buffer.data());
    if (descriptor < 0) {
        return false;
    }
    const std::filesystem::path temporary(buffer.data());
    bool success = fchmod(descriptor, S_IRUSR | S_IWUSR) == 0 && writeAll(descriptor, *content) &&
                   fsync(descriptor) == 0;
    if (close(descriptor) != 0) {
        success = false;
    }
    if (success && rename(temporary.c_str(), path_.c_str()) != 0) {
        success = false;
    }
    if (!success) {
        unlink(temporary.c_str());
        return false;
    }
    const int directoryDescriptor = open(directory.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY);
    if (directoryDescriptor < 0) {
        return false;
    }
    const bool synced = fsync(directoryDescriptor) == 0;
    close(directoryDescriptor);
    return synced;
}

void toggleFavorite(PersistentState& state, std::string_view sequence) {
    const auto iterator =
        std::find(state.favorites.begin(), state.favorites.end(), std::string(sequence));
    if (iterator == state.favorites.end()) {
        state.favorites.emplace_back(sequence);
    } else {
        state.favorites.erase(iterator);
    }
}

void recordUse(PersistentState& state, std::string_view sequence, std::int64_t timestamp,
               std::size_t maximumEntries) {
    auto iterator = std::find_if(state.recents.begin(), state.recents.end(),
                                 [&](const auto& entry) { return entry.sequence == sequence; });
    if (iterator == state.recents.end()) {
        state.recents.push_back({std::string(sequence), 1, timestamp});
    } else {
        ++iterator->useCount;
        iterator->lastUsed = timestamp;
    }
    std::stable_sort(state.recents.begin(), state.recents.end(),
                     [](const auto& left, const auto& right) {
                         if (left.lastUsed != right.lastUsed) {
                             return left.lastUsed > right.lastUsed;
                         }
                         if (left.useCount != right.useCount) {
                             return left.useCount > right.useCount;
                         }
                         return left.sequence < right.sequence;
                     });
    if (state.recents.size() > maximumEntries) {
        state.recents.resize(maximumEntries);
    }
}

}
