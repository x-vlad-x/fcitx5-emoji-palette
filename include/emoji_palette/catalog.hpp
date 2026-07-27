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

struct LocalizedAnnotation {
    std::string name;
    std::vector<std::string> keywords;
};

struct EmojiRecord {
    std::string sequence;
    Category category;
    std::string subgroup;
    std::array<LocalizedAnnotation, 3> annotations;
    std::string baseSequence;
};

struct SearchResult {
    const EmojiRecord* emoji;
    int score;
};

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
    struct SearchDocument {
        std::array<std::string, 3> names;
        std::array<std::vector<std::string>, 3> keywords;
    };

    std::vector<EmojiRecord> records_;
    std::vector<SearchDocument> searchDocuments_;
};

std::string_view categoryName(Category category);
std::array<Category, 9> unicodeCategories();

}
