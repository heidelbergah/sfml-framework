/**
 * Pad.hpp  -  Mud Cats
 *
 * The game controller. Buttons and sticks are named the way an Xbox pad
 * names them, which is also how an 8BitDo pad in its X-input mode presents
 * itself:
 *
 *   A  B  X  Y          the face buttons, bottom, right, left, top
 *   LB RB  LT RT        the bumpers, and the triggers
 *   BACK  START         select and start
 *   LS  RS              clicking a stick in
 *
 * Which number each of those is depends on the system. Linux (and so the
 * Raspberry Pi) reads pads through its joystick driver, which numbers an
 * Xbox-style pad's buttons A 0, B 1, X 2, Y 3, LB 4, RB 5, BACK 6, START 7,
 * GUIDE 8, LS 9, RS 10, with the triggers as two axes of their own.
 * Windows numbers them the same up to START, then LS 8, RS 9, with both
 * triggers on one axis. Those two are what's written here.
 *
 * The game also remembers which you touched last, keys and mouse or the
 * pad, so the hints on screen name the right buttons (see Text.hpp: with
 * the pad in use, a line "x" is shown as "x_pad" where the file has one).
 *
 * For testing without one: MUDCATS_FAKE_PAD=<file> makes up a pad that's
 * always connected, whose state is whatever the file says, one line of
 * words: the buttons held (A B X Y LB RB BACK START LS RS), and
 * LX= LY= RX= RY= LT= RT= DX= DY= for the sticks, triggers and d-pad,
 * -1 to 1. A script can rewrite it as the game runs. Only the battle reads
 * it: the menus go by events, which a file can't make.
 */

#ifndef MUDCATS_PAD_HPP
#define MUDCATS_PAD_HPP

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>

#include <optional>

namespace mudcats::pad
{

enum class Button { A, B, X, Y, LB, RB, BACK, START, LS, RS, COUNT };

// The controller in use: the first one connected. Nothing if there isn't one.
[[nodiscard]] std::optional<unsigned> id();
[[nodiscard]] bool connected();

// Whether the game insists on a controller: a Raspberry Pi console does,
// a computer with a keyboard doesn't (Settings::require_pad).
[[nodiscard]] bool required();
// Nothing can be done until one is plugged in.
[[nodiscard]] inline bool missing() { return required() && !connected(); }

// The pad's state now. refresh() once a frame, before reading it.
void refresh();
[[nodiscard]] bool held(Button b);
[[nodiscard]] sf::Vector2f left_stick();       // -1..1 each way, with the dead middle taken out
[[nodiscard]] sf::Vector2f right_stick();
[[nodiscard]] float left_trigger();            // 0..1
[[nodiscard]] float right_trigger();
[[nodiscard]] sf::Vector2i dpad();             // -1, 0 or 1 each way; down is +1

// Which button an event is about, if it's one of this pad's.
[[nodiscard]] std::optional<Button> pressed(const sf::Event& e);
[[nodiscard]] std::optional<Button> released(const sf::Event& e);

// Keys and mouse, or the pad: whichever was touched last. Every event goes
// through here first, so the hints follow along.
void notice(const sf::Event& e);
[[nodiscard]] bool in_use();

// For the menus: the pad as the keys they already understand. The d-pad
// and the left stick are the arrows, A is enter, B is escape, START is
// enter. Anything else, nothing: a menu can add its own.
[[nodiscard]] std::optional<sf::Keyboard::Scancode> menu_key(const sf::Event& e);

// A key press, made up, for handing a pad button to code that reads keys.
[[nodiscard]] sf::Event key_event(sf::Keyboard::Scancode code);

}

#endif
