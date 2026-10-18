/**
 * Config.hpp
 *
 * Resolution and window constants.
 *
 * These are constexpr rather than mutable globals. Previously
 * SCALED_WIDTH was computed once at static-init time from WINDOW_WIDTH,
 * so assigning a new WINDOW_WIDTH at runtime left the scaled values
 * silently stale. Deriving them at compile time makes that impossible.
 *
 * The game renders at WINDOW_WIDTH x WINDOW_HEIGHT and is blitted to a
 * window SCALE times larger. Keeping SCALE a whole number is what keeps
 * pixel art crisp, fractional scaling resamples and blurs it.
 */

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <SFML/System/Vector2.hpp>

inline constexpr unsigned WINDOW_WIDTH  = 960;
inline constexpr unsigned WINDOW_HEIGHT = 540;
inline constexpr unsigned SCALE         = 2;

inline constexpr unsigned SCALED_WIDTH  = WINDOW_WIDTH  * SCALE;
inline constexpr unsigned SCALED_HEIGHT = WINDOW_HEIGHT * SCALE;

inline constexpr sf::Vector2u BASE_RESOLUTION{WINDOW_WIDTH, WINDOW_HEIGHT};
inline constexpr sf::Vector2f RESOLUTION_SCALE{static_cast<float>(SCALE),
                                               static_cast<float>(SCALE)};

#endif
