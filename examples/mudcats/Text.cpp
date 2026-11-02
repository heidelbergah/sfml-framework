/**
 * Text.cpp  -  Mud Cats
 */

#include "Text.hpp"

#include "../../include/core/Random.hpp"
#include "../../third_party/nlohmann/json.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <unordered_map>

namespace mudcats
{

namespace
{
    // The file, flattened: every string under its dotted key, every list of strings likewise.
    struct Table
    {
        std::unordered_map<std::string, std::string> strings;
        std::unordered_map<std::string, std::vector<std::string>> lists;
        bool loaded{false};
    };

    Table& table()
    {
        static Table t;
        return t;
    }

    void flatten(const nlohmann::json& node, const std::string& key, Table& t)
    {
        if(node.is_string())
        {
            t.strings[key] = node.get<std::string>();
            return;
        }
        if(node.is_array())
        {
            std::vector<std::string> items;
            for(std::size_t i = 0; i < node.size(); ++i)
            {
                const auto& item = node[i];
                if(item.is_string()) items.push_back(item.get<std::string>());
                // Lists of objects or lists are reachable by index, as key.0, key.1, ...
                flatten(item, key + "." + std::to_string(i), t);
            }
            if(!items.empty()) t.lists[key] = std::move(items);
            return;
        }
        if(node.is_object())
            for(const auto& [name, child] : node.items())
                flatten(child, key.empty() ? name : key + "." + name, t);
    }

    void ensure_loaded()
    {
        if(!table().loaded) load_text("en");
    }

    std::string missing(std::string_view key)
    {
        static std::set<std::string, std::less<>> reported;
        static bool const check = std::getenv("MUDCATS_TEXT_CHECK") != nullptr;
        if(check && reported.insert(std::string(key)).second)
            std::fprintf(stderr, "text: missing key %.*s\n", static_cast<int>(key.size()), key.data());
        return "[" + std::string(key) + "]";
    }
}

void load_text(const std::string& language)
{
    Table& t = table();
    t = Table{};
    t.loaded = true;
    std::ifstream in("assets/mudcats/text/" + language + ".json");
    if(!in)
    {
        std::fprintf(stderr, "text: can't open assets/mudcats/text/%s.json\n", language.c_str());
        return;
    }
    // Comments are allowed in the file, so it can explain itself.
    nlohmann::json const root = nlohmann::json::parse(in, nullptr, false, true);
    if(root.is_discarded())
    {
        std::fprintf(stderr, "text: assets/mudcats/text/%s.json isn't valid JSON\n", language.c_str());
        return;
    }
    flatten(root, "", t);
}

bool has_text(std::string_view key)
{
    ensure_loaded();
    std::string const k(key);
    return table().strings.count(k) > 0 || table().lists.count(k) > 0;
}

std::string fill(std::string text, TextArgs args)
{
    for(const auto& [name, value] : args)
    {
        std::string const mark = "{" + std::string(name) + "}";
        for(std::size_t at = text.find(mark); at != std::string::npos; at = text.find(mark, at + value.size()))
            text.replace(at, mark.size(), value);
    }
    return text;
}

std::string tr(std::string_view key)
{
    ensure_loaded();
    auto it = table().strings.find(std::string(key));
    return it != table().strings.end() ? it->second : missing(key);
}

std::string tr(std::string_view key, TextArgs args)
{
    return fill(tr(key), args);
}

const std::vector<std::string>& list(std::string_view key)
{
    ensure_loaded();
    static const std::vector<std::string> none;
    auto it = table().lists.find(std::string(key));
    if(it == table().lists.end())
    {
        (void)missing(key);
        return none;
    }
    return it->second;
}

std::string item(std::string_view key, std::size_t index)
{
    const auto& items = list(key);
    return items.empty() ? "[" + std::string(key) + "]" : items[index % items.size()];
}

std::string pick(std::string_view key)
{
    ensure_loaded();
    auto it = table().lists.find(std::string(key));
    if(it != table().lists.end() && !it->second.empty())
        return it->second[static_cast<std::size_t>(Random::integer(0, static_cast<int>(it->second.size()) - 1))];
    return tr(key);
}

std::string pick(std::string_view key, TextArgs args)
{
    return fill(pick(key), args);
}

}
