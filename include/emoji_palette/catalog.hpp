#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace emoji_palette {

enum class Category {
    RecentlyUsed,
    Favorites,
    SmileysEmotion,
    PeopleBody,
    AnimalsNature,
    FoodDrink,
    TravelPlaces,
    Activities,
    Objects,
    Symbols,
    Flags,
    Kaomoji,
};

enum class Locale {
    English = 0,
    German = 1,
    Russian = 2,
};

inline constexpr std::size_t localeCount = 3;

struct LocalizedAnnotation {
    std::string name;
    std::vector<std::string> keywords;
};

struct EmojiRecord {
    std::string sequence;
    Category category;
    std::string subgroup;
    std::array<LocalizedAnnotation, localeCount> annotations;
    std::string baseSequence;
};

struct SearchResult {
    const EmojiRecord* emoji;
    int score;
};

struct AnnotationIndex {
    std::string name;
    std::vector<std::string> keywords;
};

// Every bundled locale is indexed for every emoji, so a query is never limited
// to the language the picker was opened with.
struct SearchDocument {
    std::array<AnnotationIndex, localeCount> locales;
};

SearchDocument buildSearchDocument(const std::array<LocalizedAnnotation, localeCount>& annotations);

// Scores an already normalized token against every bundled locale and returns a
// negative value when none of them matches. The requested locale only ranks
// matches ahead of the other languages; it never hides them.
int scoreSearchToken(std::string_view token, const SearchDocument& document, Locale requested);

class EmojiCatalog {
  public:
    EmojiCatalog();

    const std::vector<EmojiRecord>& records() const;
    const EmojiRecord* find(std::string_view sequence) const;
    bool contains(std::string_view sequence) const;
    std::vector<const EmojiRecord*> variantsFor(std::string_view base) const;
    std::vector<SearchResult> search(std::string_view query, Locale locale,
                                     std::size_t limit = 100) const;

  private:
    std::vector<EmojiRecord> records_;
    std::vector<SearchDocument> searchDocuments_;
};

std::string_view categoryName(Category category);
std::array<Category, 9> unicodeCategories();

}
