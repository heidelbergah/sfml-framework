/**
 * main.cpp  -  Showcase
 *
 * Five small demos behind one menu, each a Scene:
 *
 *   Easing playground   Interpolated and every easing curve
 *   Particle sandbox    ParticleSystem, a Frame of Sliders, bloom
 *   Shake lab           Camera trauma, sustained shake, Gauge, UI views
 *   Name forge          Markov, TextBox typewriter text
 *   Shader windows      ShaderManager, draggable Frames, five fragment shaders
 *
 *     make EXAMPLE=showcase run
 */

#include "../../include/scenes/SceneManager.hpp"

#include "EasingScene.hpp"
#include "HubScene.hpp"
#include "NameScene.hpp"
#include "ParticleScene.hpp"
#include "ShaderScene.hpp"
#include "ShakeScene.hpp"

int main()
{
    SceneManager sm("sfml-framework showcase");

    // Bloom is loaded once here but only switched on by the scenes that
    // want it, so text elsewhere never picks up a glow.
    sm.enable_bloom("shaders");
    sm.set_bloom_enabled(false);
    sm.set_fade_duration(0.25f);

    sm.add_scene("hub",       std::make_shared<showcase::HubScene>());
    sm.add_scene("easing",    std::make_shared<showcase::EasingScene>());
    sm.add_scene("particles", std::make_shared<showcase::ParticleScene>());
    sm.add_scene("shake",     std::make_shared<showcase::ShakeScene>());
    sm.add_scene("names",     std::make_shared<showcase::NameScene>());
    sm.add_scene("shaders",   std::make_shared<showcase::ShaderScene>());

    sm.swap_scene("hub", SceneTransitionType::INSTANT);
    sm.run(60);
    return 0;
}
