/**
 * Includes.hpp
 *
 * Umbrella header. Pulls in SFML plus the core framework utilities.
 *
 * Every standard header used anywhere in the framework is included
 * explicitly here. Previously the code relied on SFML pulling in
 * <vector>, <string>, <memory> and friends transitively, which happens
 * to work today but is not guaranteed, an SFML point release that
 * tidies its own includes would break the build.
 */

#ifndef INCLUDES_HPP
#define INCLUDES_HPP

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/AssetCache.hpp"
#include "core/Config.hpp"
#include "core/Easing.hpp"
#include "core/GameTime.hpp"
#include "core/Interpolated.hpp"
#include "core/Letterbox.hpp"
#include "core/Math.hpp"
#include "core/Random.hpp"

#endif
