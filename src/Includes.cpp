/**
 * Includes.cpp
 *
 * Written by James Bouchat
 */

#include "../include/Includes.hpp"

float ease_in_out_exponential(float t)
{
    if(t == 0) return 0;
    if(t == 1) return 1;
    if(t < 0.5) return pow(2, 20 * t - 10) / 2;
    else return (2 - pow(2, -20 * t + 10)) / 2;
}

float ease_out_back(float t)
{
    const float c1 = 1.70158;
    const float c3 = c1 + 1;

    return 1 + c3 * pow(t - 1, 3) + c1 * pow(t - 1, 2);
}

float ease_in_back(float t)
{
    const float c1 = 1.70158;
    const float c3 = c1 + 1;

    return c3 * t * t * t - c1 * t * t;
}

float ease_in_out_back(float t)
{
    const float c1 = 1.70158;
    const float c2 = c1 * 1.525;

    if(t < 0.5) return (pow(2 * t, 2) * ((c2 + 1) * 2 * t - c2)) / 2;
    else return (pow(2 * t - 2, 2) * ((c2 + 1) * (t * 2 - 2) + c2) + 2) / 2;
}

float ease_out_elastic(float t)
{
    const float c4 = (2 * 3.14159) / 3;

    if(t == 0) return 0;
    if(t == 1) return 1;
    else return pow(2, -10 * t) * sin((t * 10 - 0.75) * c4) + 1;
}

float ease_in_out_elastic(float t)
{
    const float c5 = (2 * 3.14159) / 4.5;

    if(t == 0) return 0;
    if(t == 1) return 1;
    if(t < 0.5) return -(pow(2, 20 * t - 10) * sin((20 * t - 11.125) * c5)) / 2;
    else return (pow(2, -20 * t + 10) * sin((20 * t - 11.125) * c5)) / 2 + 1;
}

float get_ratio(float t, TransitionFunction transition)
{
    switch(transition)
    {
        default:
            return t;
        case TransitionFunction::None:
            return 1.0f;
        case TransitionFunction::Linear:
            return t;
        case TransitionFunction::EaseInOutExponential:
            return ease_in_out_exponential(t);
        case TransitionFunction::EaseOutBack:
            return ease_out_back(t);
        case TransitionFunction::EaseInBack:
            return ease_in_back(t);
        case TransitionFunction::EaseInOutBack:
            return ease_in_out_back(t);
        case TransitionFunction::EaseOutElastic:
            return ease_out_elastic(t);
        case TransitionFunction::EaseInOutElastic:
            return ease_in_out_elastic(t);
    }
}

