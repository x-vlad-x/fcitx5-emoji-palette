#include "emoji_palette/catalog.hpp"

#include "emoji_palette/utf8.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace emoji_palette {
namespace {

struct GeneratedEmoji {
    std::string_view sequence;
    Category category;
    std::string_view subgroup;
    std::string_view englishName;
    std::string_view englishKeywords;
    std::string_view germanName;
    std::string_view germanKeywords;
    std::string_view russianName;
    std::string_view russianKeywords;
    std::string_view baseSequence;
};

#include "data/generated/emoji_data.inc"

std::vector<std::string> split(std::string_view value, char delimiter) {
    std::vector<std::string> result;
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const auto end = value.find(delimiter, begin);
        const auto part = value.substr(begin, end == std::string_view::npos ? end : end - begin);
        if (!part.empty()) {
            result.emplace_back(part);
        }
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return result;
}

std::vector<std::string> queryTokens(std::string_view query) {
    return split(normalizeForSearch(query), ' ');
}

bool isSubsequence(std::string_view needle, std::string_view haystack) {
    std::size_t position = 0;
    for (const char value : haystack) {
        if (position < needle.size() && value == needle[position]) {
            ++position;
        }
    }
    return position == needle.size();
}

int textScore(std::string_view token, std::string_view text, int exact) {
    if (text.empty()) {
        return -1;
    }
    if (text == token) {
        return exact;
    }
    if (text.starts_with(token)) {
        return exact - 150;
    }
    const std::string wordPrefix = " " + std::string(token);
    if (text.find(wordPrefix) != std::string_view::npos) {
        return exact - 250;
    }
    if (text.find(token) != std::string_view::npos) {
        return exact - 400;
    }
    if (token.size() >= 3 && isSubsequence(token, text)) {
        return exact - 750;
    }
    return -1;
}

template <typename Document>
int documentScore(std::string_view token, const Document& document, std::size_t locale) {
    int best = textScore(token, document.names[locale], 1000);
    for (const auto& keyword : document.keywords[locale]) {
        best = std::max(best, textScore(token, keyword, 700));
    }
    if (locale != 0) {
        int fallback = textScore(token, document.names[0], 900);
        for (const auto& keyword : document.keywords[0]) {
            fallback = std::max(fallback, textScore(token, keyword, 600));
        }
        best = std::max(best, fallback);
    }
    return best;
}

} // namespace

EmojiCatalog::EmojiCatalog() {
    records_.reserve(kGeneratedEmojiCount);
    searchDocuments_.reserve(kGeneratedEmojiCount);
    for (const auto& generated : kGeneratedEmoji) {
        EmojiRecord record{
            .sequence = std::string(generated.sequence),
            .category = generated.category,
            .subgroup = std::string(generated.subgroup),
            .annotations =
                {
                    LocalizedAnnotation{
                        std::string(generated.englishName),
                        split(generated.englishKeywords, '|'),
                    },
                    LocalizedAnnotation{
                        std::string(generated.germanName),
                        split(generated.germanKeywords, '|'),
                    },
                    LocalizedAnnotation{
                        std::string(generated.russianName),
                        split(generated.russianKeywords, '|'),
                    },
                },
            .baseSequence = std::string(generated.baseSequence),
        };
        SearchDocument document;
        for (std::size_t locale = 0; locale < record.annotations.size(); ++locale) {
            document.names[locale] = normalizeForSearch(record.annotations[locale].name);
            document.keywords[locale].reserve(record.annotations[locale].keywords.size());
            for (const auto& keyword : record.annotations[locale].keywords) {
                document.keywords[locale].push_back(normalizeForSearch(keyword));
            }
        }
        records_.push_back(std::move(record));
        searchDocuments_.push_back(std::move(document));
    }
}

const std::vector<EmojiRecord>& EmojiCatalog::records() const { return records_; }

const EmojiRecord* EmojiCatalog::find(std::string_view sequence) const {
    const auto iterator = std::find_if(records_.begin(), records_.end(), [&](const auto& record) {
        return record.sequence == sequence;
    });
    return iterator == records_.end() ? nullptr : &*iterator;
}

bool EmojiCatalog::contains(std::string_view sequence) const { return find(sequence) != nullptr; }

std::vector<const EmojiRecord*> EmojiCatalog::variantsFor(std::string_view base) const {
    std::vector<const EmojiRecord*> result;
    if (const auto* record = find(base)) {
        result.push_back(record);
    }
    for (const auto& record : records_) {
        if (record.baseSequence == base) {
            result.push_back(&record);
        }
    }
    return result;
}

std::vector<SearchResult> EmojiCatalog::search(std::string_view query, Locale locale,
                                               std::size_t limit) const {
    const auto normalized = normalizeForSearch(query);
    if (normalized.empty() || limit == 0) {
        return {};
    }
    if (const auto* exact = find(query)) {
        return {{exact, 10000}};
    }
    const auto tokens = queryTokens(normalized);
    if (tokens.empty()) {
        return {};
    }
    const auto localeIndex = static_cast<std::size_t>(locale);
    struct Candidate {
        std::size_t index;
        int score;
    };
    std::vector<Candidate> candidates;
    for (std::size_t index = 0; index < records_.size(); ++index) {
        int total = 0;
        bool matches = true;
        for (const auto& token : tokens) {
            const int score = documentScore(token, searchDocuments_[index], localeIndex);
            if (score < 0) {
                matches = false;
                break;
            }
            total += score;
        }
        if (matches) {
            candidates.push_back({index, total});
        }
    }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const auto& left, const auto& right) { return left.score > right.score; });
    if (candidates.size() > limit) {
        candidates.resize(limit);
    }
    std::vector<SearchResult> result;
    result.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        result.push_back({&records_[candidate.index], candidate.score});
    }
    return result;
}

std::string_view categoryName(Category category) {
    switch (category) {
    case Category::RecentlyUsed:
        return "Recently Used";
    case Category::Favorites:
        return "Favorites";
    case Category::SmileysEmotion:
        return "Smileys & Emotion";
    case Category::PeopleBody:
        return "People & Body";
    case Category::AnimalsNature:
        return "Animals & Nature";
    case Category::FoodDrink:
        return "Food & Drink";
    case Category::TravelPlaces:
        return "Travel & Places";
    case Category::Activities:
        return "Activities";
    case Category::Objects:
        return "Objects";
    case Category::Symbols:
        return "Symbols";
    case Category::Flags:
        return "Flags";
    case Category::Kaomoji:
        return "Kaomoji";
    }
    return {};
}

std::array<Category, 9> unicodeCategories() {
    return {
        Category::SmileysEmotion, Category::PeopleBody,   Category::AnimalsNature,
        Category::FoodDrink,      Category::TravelPlaces, Category::Activities,
        Category::Objects,        Category::Symbols,      Category::Flags,
    };
}

} // namespace emoji_palette
