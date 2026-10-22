/**
 * Content.hpp  -  Desk
 *
 * The papers on the desk, each built as a stack of Frames full of
 * Paragraphs and Pictures. All the in-world text is placeholder, written
 * around the Mud Cats setting.
 */

#ifndef DESK_CONTENT_HPP
#define DESK_CONTENT_HPP

#include "../../include/gui/Frame.hpp"

namespace desk
{

const sf::Font& font();

std::vector<std::shared_ptr<Frame>> early_edition();
std::vector<std::shared_ptr<Frame>> late_edition();
std::vector<std::shared_ptr<Frame>> letter();
std::vector<std::shared_ptr<Frame>> photograph();
std::vector<std::shared_ptr<Frame>> sticky_note();

inline const sf::Vector2f NEWSPAPER_SIZE{300.0f, 404.0f};
inline const sf::Vector2f LETTER_SIZE{220.0f, 292.0f};
inline const sf::Vector2f PHOTO_SIZE{150.0f, 124.0f};
inline const sf::Vector2f NOTE_SIZE{160.0f, 112.0f};

}

#endif
