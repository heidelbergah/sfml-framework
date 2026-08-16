/**
 * Letterbox.hpp
 *
 * Works out how a fixed-resolution render target maps onto a window of
 * arbitrary size, and converts coordinates in both directions.
 *
 * Pulled out as a pure function so it can be tested without opening a
 * window. Getting this wrong is subtle: the failure mode is a cursor
 * that tracks correctly at the top of the screen and drifts further off
 * the lower you click, which reads like a physics bug rather than a
 * coordinate one.
 */

#ifndef LETTERBOX_HPP
#define LETTERBOX_HPP

#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <cmath>

enum class ScalingMode
{
    /*
     * Fill the window completely, allowing the two axes to scale by
     * different amounts. No bars ever. Distortion equals the difference
     * between the window's aspect ratio and the game's -- for a 16:9
     * game in a maximised window that is typically 2-4%, which nobody
     * can see. On a very differently shaped window (ultrawide, portrait)
     * it becomes obvious, so prefer FIT there.
     *
     * This is the default: stable UI coordinates and no black bars.
     */
    STRETCH,

    /*
     * Preserve the aspect ratio exactly and centre the result, leaving
     * bars on whichever axis has room left over. Nothing is ever
     * distorted; you pay for it with the bars.
     */
    FIT,

    /*
     * FIT, but snapped to a whole-number scale. Gives a perfectly even
     * pixel grid at the cost of much larger bars -- a 1920x1000 window
     * drops from 1.85x to 1x, using a quarter of the area.
     */
    INTEGER
};

struct Letterbox
{
    sf::Vector2f scale{1.0f, 1.0f};
    sf::Vector2f offset;              // top-left of the content in the window

    static Letterbox compute(sf::Vector2u window, sf::Vector2u base,
                             ScalingMode mode = ScalingMode::STRETCH)
    {
        Letterbox result;

        if(window.x == 0 || window.y == 0 || base.x == 0 || base.y == 0)
            return result;

        float const fit_x = static_cast<float>(window.x) / static_cast<float>(base.x);
        float const fit_y = static_cast<float>(window.y) / static_cast<float>(base.y);

        if(mode == ScalingMode::STRETCH)
        {
            result.scale  = {fit_x, fit_y};
            result.offset = {0.0f, 0.0f};
            return result;
        }

        float uniform = std::min(fit_x, fit_y);

        // Below 1x there is no integer option that fits, so fall through
        // to fractional rather than clamping up and cropping.
        if(mode == ScalingMode::INTEGER && uniform >= 1.0f)
            uniform = std::floor(uniform);

        result.scale = {uniform, uniform};

        float const content_w = static_cast<float>(base.x) * uniform;
        float const content_h = static_cast<float>(base.y) * uniform;

        result.offset = {
            std::floor((static_cast<float>(window.x) - content_w) / 2.0f),
            std::floor((static_cast<float>(window.y) - content_h) / 2.0f)
        };

        return result;
    }

    /*
     * Window pixel -> base-resolution coordinate. Points inside the
     * bars map outside [0, base], which is how you detect a click that
     * missed the content.
     */
    [[nodiscard]]
    sf::Vector2f to_base(sf::Vector2f window_pixel) const
    {
        return { (scale.x > 0.0f) ? (window_pixel.x - offset.x) / scale.x : 0.0f,
                 (scale.y > 0.0f) ? (window_pixel.y - offset.y) / scale.y : 0.0f };
    }

    /*
     * Base-resolution coordinate -> window pixel. Exact inverse.
     */
    [[nodiscard]]
    sf::Vector2f to_window(sf::Vector2f base_pos) const
    {
        return { base_pos.x * scale.x + offset.x,
                 base_pos.y * scale.y + offset.y };
    }

    [[nodiscard]]
    bool contains(sf::Vector2f window_pixel, sf::Vector2u base) const
    {
        sf::Vector2f const p = to_base(window_pixel);
        return p.x >= 0.0f && p.y >= 0.0f &&
               p.x <= static_cast<float>(base.x) &&
               p.y <= static_cast<float>(base.y);
    }

    /*
     * True when bars are present on either axis.
     */
    [[nodiscard]]
    bool has_bars() const
    {
        return offset.x > 0.5f || offset.y > 0.5f;
    }
};

#endif
