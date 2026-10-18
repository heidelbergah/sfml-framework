/**
 * Easing.hpp
 *
 * Easing curves for Interpolated<T>. Each takes t in [0, 1] and returns
 * a ratio, which may overshoot outside [0, 1] for the "back" and
 * "elastic" families, that overshoot is the point.
 */

#ifndef EASING_HPP
#define EASING_HPP

float ease_linear(float t);
float ease_in_out_exponential(float t);
float ease_out_back(float t);
float ease_in_back(float t);
float ease_in_out_back(float t);
float ease_out_elastic(float t);
float ease_in_out_elastic(float t);
float ease_in_quad(float t);
float ease_out_quad(float t);
float ease_in_out_quad(float t);
float ease_out_bounce(float t);

enum class TransitionFunction
{
    None,
    Linear,
    EaseInOutExponential,
    EaseOutBack,
    EaseInBack,
    EaseInOutBack,
    EaseOutElastic,
    EaseInOutElastic,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseOutBounce
};

/*
 * Maps t through the chosen curve. t is expected in [0, 1]; values
 * outside that range are clamped before the curve is applied.
 */
float get_ratio(float t, TransitionFunction transition);

#endif
