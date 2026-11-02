/**
 * Text.hpp  -  Mud Cats
 *
 * Every word the game shows or says lives in one file per language,
 * assets/mudcats/text/<language>.json, and is looked up by key:
 *
 *   tr("title.menu.play")                        "PLAY"
 *   tr("hud.notice.takes_section", {{"name", who}, {"section", what}})
 *                                                "{name} takes {section}", filled in
 *   pick("barks.monteran.contact")               one of a list, at random
 *   item("orders.words", 2)                      one of a list, by position
 *   list("names.rechivii.men")                   the whole list
 *
 * Keys are paths through the JSON's objects, joined with dots. A string
 * can hold {placeholders}; anything not given is left as it is. A key the
 * file doesn't have comes back as "[key]", so a missing line shows up on
 * screen instead of crashing anything, and with MUDCATS_TEXT_CHECK set
 * each missing key is reported once on stderr. tools/text/check_text.py
 * compares the keys the code asks for with the ones the file has.
 */

#ifndef MUDCATS_TEXT_HPP
#define MUDCATS_TEXT_HPP

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mudcats
{

using TextArgs = std::initializer_list<std::pair<std::string_view, std::string>>;

/*
 * Reads assets/mudcats/text/<language>.json. Called once at start; any
 * lookup before then loads English.
 */
void load_text(const std::string& language = "en");

[[nodiscard]] bool has_text(std::string_view key);
[[nodiscard]] std::string tr(std::string_view key);
[[nodiscard]] std::string tr(std::string_view key, TextArgs args);

// One entry of a list at random (or the string itself, if it isn't a list).
[[nodiscard]] std::string pick(std::string_view key);
[[nodiscard]] std::string pick(std::string_view key, TextArgs args);

// One entry of a list by position (wrapping round past the end).
[[nodiscard]] std::string item(std::string_view key, std::size_t index);

// A whole list; empty if there's no such list.
[[nodiscard]] const std::vector<std::string>& list(std::string_view key);

// Fills {placeholders} in any string.
[[nodiscard]] std::string fill(std::string text, TextArgs args);

}

#endif
