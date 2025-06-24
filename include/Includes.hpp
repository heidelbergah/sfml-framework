/**
 * Includes.hpp
 */

#ifndef INCLUDES_HPP
#define INCLUDES_HPP

#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>
//#include <SFML/Audio.hpp>
//#include <SFML/Network.hpp>

#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <functional>
#include <random>

struct Vector
{
    float magnitude;
    sf::Angle direction;
};

static Vector add_vectors(Vector v1, Vector v2)
{
    float r1 = v1.magnitude, r2 = v2.magnitude;
    float p1 = v1.direction.asRadians();
    float p2 = v2.direction.asRadians();

    float p_diff = p2 - p1;
    
    float r = sqrt((r1 * r1) + (r2 * r2) + (2 * r1 * r2 * cos(p_diff)));

    sf::Angle p;
    float y = r2 * sin(p_diff);
    float x = r1 + (r2 * cos(p_diff));

    p = sf::radians(p1 + atan2(y, x));

    Vector vector;
    vector.magnitude = r;
    vector.direction = p;
    return vector;
}

#endif
