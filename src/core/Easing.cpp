/**
 * Easing.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/core/Easing.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float PI = 3.14159265358979323846f;

    // Overshoot constants, from the standard easing definitions.
    constexpr float BACK_C1 = 1.70158f;
    constexpr float BACK_C3 = BACK_C1 + 1.0f;
    constexpr float BACK_C2 = BACK_C1 * 1.525f;
}

float ease_linear(float t)
{
    return t;
}

float ease_in_out_exponential(float t)
{
    if(t <= 0.0f) return 0.0f;
    if(t >= 1.0f) return 1.0f;
    if(t < 0.5f)  return std::pow(2.0f, 20.0f * t - 10.0f) / 2.0f;
    return (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) / 2.0f;
}

float ease_out_back(float t)
{
    float const u = t - 1.0f;
    return 1.0f + BACK_C3 * u * u * u + BACK_C1 * u * u;
}

float ease_in_back(float t)
{
    return BACK_C3 * t * t * t - BACK_C1 * t * t;
}

float ease_in_out_back(float t)
{
    if(t < 0.5f)
    {
        float const u = 2.0f * t;
        return (u * u * ((BACK_C2 + 1.0f) * u - BACK_C2)) / 2.0f;
    }

    float const u = 2.0f * t - 2.0f;
    return (u * u * ((BACK_C2 + 1.0f) * u + BACK_C2) + 2.0f) / 2.0f;
}

float ease_out_elastic(float t)
{
    constexpr float c4 = (2.0f * PI) / 3.0f;

    if(t <= 0.0f) return 0.0f;
    if(t >= 1.0f) return 1.0f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
}

float ease_in_out_elastic(float t)
{
    constexpr float c5 = (2.0f * PI) / 4.5f;

    if(t <= 0.0f) return 0.0f;
    if(t >= 1.0f) return 1.0f;
    if(t < 0.5f)
        return -(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f;
    return (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f + 1.0f;
}

float ease_in_quad(float t)
{
    return t * t;
}

float ease_out_quad(float t)
{
    return 1.0f - (1.0f - t) * (1.0f - t);
}

float ease_in_out_quad(float t)
{
    if(t < 0.5f) return 2.0f * t * t;
    float const u = -2.0f * t + 2.0f;
    return 1.0f - (u * u) / 2.0f;
}

float ease_out_bounce(float t)
{
    constexpr float n1 = 7.5625f;
    constexpr float d1 = 2.75f;

    if(t < 1.0f / d1)        return n1 * t * t;
    if(t < 2.0f / d1)      { t -= 1.5f  / d1; return n1 * t * t + 0.75f; }
    if(t < 2.5f / d1)      { t -= 2.25f / d1; return n1 * t * t + 0.9375f; }
    t -= 2.625f / d1;
    return n1 * t * t + 0.984375f;
}

float get_ratio(float t, TransitionFunction transition)
{
    // None means "no interpolation", jump straight to the end value.
    if(transition == TransitionFunction::None) return 1.0f;

    t = std::clamp(t, 0.0f, 1.0f);

    switch(transition)
    {
        case TransitionFunction::Linear:               return ease_linear(t);
        case TransitionFunction::EaseInOutExponential: return ease_in_out_exponential(t);
        case TransitionFunction::EaseOutBack:          return ease_out_back(t);
        case TransitionFunction::EaseInBack:           return ease_in_back(t);
        case TransitionFunction::EaseInOutBack:        return ease_in_out_back(t);
        case TransitionFunction::EaseOutElastic:       return ease_out_elastic(t);
        case TransitionFunction::EaseInOutElastic:     return ease_in_out_elastic(t);
        case TransitionFunction::EaseInQuad:           return ease_in_quad(t);
        case TransitionFunction::EaseOutQuad:          return ease_out_quad(t);
        case TransitionFunction::EaseInOutQuad:        return ease_in_out_quad(t);
        case TransitionFunction::EaseOutBounce:        return ease_out_bounce(t);
        case TransitionFunction::None:                 return 1.0f;
    }

    return t;
}
