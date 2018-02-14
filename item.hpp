#ifndef ITEMS_HPP_INCLUDED
#define ITEMS_HPP_INCLUDED

#include <string>
#include <vector>

namespace item_types
{
    enum item_type
    {
        SCRIPT, ///expose access logs
        LOCK,
        CHAR_COUNT,
        SCRIPT_SLOT,
        PUBLIC_SCRIPT_SLOT,
        MISC,
        ERR,
    };

    static std::vector<std::string> quick_names
    {
        "Script",
        "Lock",
        "Char Count",
        "Script Slot",
        "Public Script Slot",
        "Misc",
        "Error not found",
    };
}

struct item
{
    /*int64_t id = -1;
    int32_t rarity = 0;

    item_types::item_type type = item_types::ERR;

    ///Hmm. Not normalising descriptions is going to cost a lot of space
    ///It allows for very custom one off items
    ///and potentially even custom player items... which is actually necessary for scripts, so we can't just use a predefined table
    ///would have to do db stuff to fix this
    std::string name;
    std::string description;*/

    std::map<std::string, std::string> item_properties;

    std::string get_prop(const std::string& str)
    {
        return item_properties[str];
    }

    int64_t get_prop_as_integer(const std::string& str)
    {
        std::string prop = item_properties[str];

        long long val = atoll(prop.c_str());

        return val;
    }
};

namespace item_types
{
    //item get_default_of(item)
}

#endif // ITEMS_HPP_INCLUDED
