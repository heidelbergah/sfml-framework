/**
 * Screens.cpp  -  Cinder
 */

#include "Screens.hpp"

#include "Palette.hpp"
#include "Run.hpp"
#include "Sound.hpp"

#include "../../include/core/Random.hpp"

#include <cmath>
#include <cstdio>

namespace cinder
{

// ---------------------------------------------------------------- Meadow

Meadow::Meadow()
{
    m_def.name  = "meadow";
    m_def.size  = FRAME_SIZE_F;
    m_def.start = FRAME_SIZE_F * 0.5f;
    m_def.seed  = 7;
    m_def.walls = {{{60, 200}, {70, 10}}, {{380, 60}, {10, 40}}};
    m_def.water = {{{330, 190}, {90, 36}}};
    m_world.build(m_def);
    m_spark = m_last = m_def.start;
}

void Meadow::initialize(ShaderManager& shaders)
{
    m_reveal.initialize(shaders);
}

void Meadow::update(float dt, bool wander)
{
    m_time += dt;

    if(wander)
    {
        // A slow figure of eight around the title, with a little wobble.
        float const t = m_time * 0.22f;
        m_spark = {240.0f + std::sin(t) * 190.0f + std::sin(m_time * 1.7f) * 6.0f,
                   140.0f + std::sin(t * 2.0f) * 80.0f + std::cos(m_time * 1.3f) * 5.0f};

        if(std::hypot(m_spark.x - m_last.x, m_spark.y - m_last.y) > 5.0f)
        {
            m_last = m_spark;
            m_trail.push_back({m_spark, 6.0f});
        }

        m_flame += dt;
        while(m_flame > 1.0f / 50.0f)
        {
            m_flame -= 1.0f / 50.0f;
            m_motes.flame(m_spark, 2.0f, 2.5f, 34.0f);
        }
    }
    else if(Random::chance(dt * 25.0f))
    {
        // At the end, embers drift up from everywhere.
        m_motes.flame({Random::real(0.0f, FRAME_SIZE_F.x), FRAME_SIZE_F.y + 2.0f}, 0.0f, 2.0f, 40.0f);
    }

    for(auto& p : m_trail) p.life -= dt;
    std::erase_if(m_trail, [](const Trail& p) { return p.life <= 0.0f; });
    m_motes.update(dt);
}

void Meadow::render(sf::RenderTarget& target, float warmth)
{
    if(!m_reveal.ready()) return;

    sf::View const view(FRAME_SIZE_F * 0.5f, FRAME_SIZE_F);
    sf::FloatRect const visible({0.0f, 0.0f}, FRAME_SIZE_F);

    sf::RenderTexture& world = m_reveal.world(view, m_world.ground_colour());
    m_world.draw_living(world, visible);

    if(warmth < 1.0f)
    {
        m_reveal.warm(m_spark, 70.0f);
        for(const auto& p : m_trail) m_reveal.warm(p.pos, 30.0f * std::sqrt(p.life / 6.0f), std::min(1.0f, p.life / 3.0f));
    }
    if(warmth > 0.0f) m_reveal.warm(FRAME_SIZE_F * 0.5f, 600.0f * warmth);

    float const flicker = 0.5f + 0.5f * std::sin(m_time * 11.0f);
    sf::RenderTexture& frame = m_reveal.composite(view, m_time, flicker);
    m_world.draw_cold(frame, visible, m_time);
    m_motes.draw(frame);

    if(warmth < 1.0f)
    {
        sf::RectangleShape core({3.0f, 3.0f});
        core.setPosition({std::round(m_spark.x) - 1.0f, std::round(m_spark.y) - 1.0f});
        core.setFillColor(YELLOW);
        frame.draw(core);
    }

    m_reveal.present(target);
}

// ---------------------------------------------------------------- MenuScene

sf::Color MenuScene::get_clear_color() const { return BACKGROUND; }

void MenuScene::on_enter()
{
    m_meadow.initialize(m_manager->get_shaders());
    m_manager->set_bloom_enabled(true);
    m_leaving = false;
    SoundBank::get().stop_loops();
    SoundBank::get().set_music(true);
}

void MenuScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    (void)mouse_pos;
    const auto* key = event.getIf<sf::Event::KeyPressed>();
    if(!key || m_leaving) return;

    switch(key->scancode)
    {
        case sf::Keyboard::Scancode::Space:
        case sf::Keyboard::Scancode::Enter:
            m_leaving = true;
            SoundBank::get().play("ignite", 0.7f);
            m_manager->request_scene_switch("play", SceneTransitionType::FADE);
            break;
        case sf::Keyboard::Scancode::M:
            SoundBank::get().toggle_music_mute();
            break;
        case sf::Keyboard::Scancode::Escape:
            EventBus::publish(Event(EventType::QUIT));
            break;
        default:
            break;
    }
}

void MenuScene::update_realtime(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();
    m_time += dt;
    m_meadow.update(dt, true);
    SoundBank::get().set_loop("fire_loop", 0.18f);
    SoundBank::get().update(dt);
}

void MenuScene::render(sf::RenderTarget& target)
{
    m_meadow.render(target, 0.0f);
    target.setView(target.getDefaultView());

    const sf::Font* f = font();
    if(!f) return;

    // Darken behind the words so they read over the meadow.
    sf::RectangleShape band({static_cast<float>(BASE_RESOLUTION.x), 150.0f});
    band.setPosition({0.0f, 150.0f});
    band.setFillColor(with_alpha(BACKGROUND, 0.55f));
    target.draw(band);

    sf::RectangleShape foot({static_cast<float>(BASE_RESOLUTION.x), 64.0f});
    foot.setPosition({0.0f, static_cast<float>(BASE_RESOLUTION.y) - 80.0f});
    foot.setFillColor(with_alpha(BACKGROUND, 0.6f));
    target.draw(foot);

    float const cx = BASE_RESOLUTION.x * 0.5f;
    float const flicker = 0.5f + 0.5f * std::sin(m_time * 9.0f) * std::sin(m_time * 2.3f);

    target.draw(centred_text(*f, "C I N D E R", 64, {cx + 2.0f, 202.0f}, RED_ASHEN));
    target.draw(centred_text(*f, "C I N D E R", 64, {cx, 200.0f}, lerp(ORANGE_GLOW, GOLDEN, flicker)));
    target.draw(centred_text(*f, "you are the last spark", 16, {cx, 252.0f}, G4));

    float const blink = std::fmod(m_time, 1.4f) < 1.0f ? 1.0f : 0.35f;
    target.draw(centred_text(*f, "space to begin", 16, {cx, 282.0f}, with_alpha(G2, blink)));

    float const y = static_cast<float>(BASE_RESOLUTION.y) - 56.0f;
    target.draw(centred_text(*f, "wasd / arrows  move      space  dash      p  pause      m  music      esc  quit",
                             16, {cx, y}, G6));
    target.draw(centred_text(*f, "colours from ashen by ficd", 16, {cx, y + 24.0f}, G7));
}

// ---------------------------------------------------------------- EndScene

sf::Color EndScene::get_clear_color() const { return BACKGROUND; }

void EndScene::on_enter()
{
    m_meadow.initialize(m_manager->get_shaders());
    m_manager->set_bloom_enabled(true);
    m_time = 0.0f;
    SoundBank::get().stop_loops();
    SoundBank::get().set_music(true);
}

void EndScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    (void)mouse_pos;
    const auto* key = event.getIf<sf::Event::KeyPressed>();
    if(!key || m_time < 1.5f) return;

    if(key->scancode == sf::Keyboard::Scancode::Space || key->scancode == sf::Keyboard::Scancode::Enter
       || key->scancode == sf::Keyboard::Scancode::Escape)
    {
        SoundBank::get().play("ui_select", 0.6f);
        m_manager->request_scene_switch("menu", SceneTransitionType::FADE);
        m_time = -100.0f;       // ignore further keys while fading
    }
    else if(key->scancode == sf::Keyboard::Scancode::M)
    {
        SoundBank::get().toggle_music_mute();
    }
}

void EndScene::update_realtime(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();
    m_time += dt;
    m_meadow.update(dt, false);
    SoundBank::get().set_loop("fire_loop", 0.3f);
    SoundBank::get().update(dt);
}

void EndScene::render(sf::RenderTarget& target)
{
    m_meadow.render(target, 1.0f);
    target.setView(target.getDefaultView());

    const sf::Font* f = font();
    if(!f) return;

    sf::RectangleShape band({static_cast<float>(BASE_RESOLUTION.x), 230.0f});
    band.setPosition({0.0f, 140.0f});
    band.setFillColor(with_alpha(BACKGROUND, 0.6f));
    target.draw(band);

    float const cx = BASE_RESOLUTION.x * 0.5f;
    auto fade = [this](float at) { return std::clamp((m_time - at) / 0.8f, 0.0f, 1.0f); };

    const Run& run = Run::current();
    int const seconds = static_cast<int>(run.time);
    char clock[16];
    std::snprintf(clock, sizeof clock, "%d:%02d", seconds / 60, seconds % 60);

    target.draw(centred_text(*f, "the world is warm again", 32, {cx, 190.0f}, with_alpha(GOLDEN, fade(0.3f))));
    target.draw(centred_text(*f, std::string("time  ") + clock, 16, {cx, 240.0f}, with_alpha(G3, fade(1.0f))));
    target.draw(centred_text(*f, "embers gathered  " + std::to_string(run.embers), 16, {cx, 262.0f}, with_alpha(G3, fade(1.2f))));
    target.draw(centred_text(*f, "times gone out  " + std::to_string(run.deaths), 16, {cx, 284.0f}, with_alpha(G3, fade(1.4f))));
    target.draw(centred_text(*f, "space to return", 16, {cx, 330.0f}, with_alpha(G5, fade(2.0f))));

    target.draw(centred_text(*f, "colours from ashen by ficd  -  made with sfml-framework", 16,
                             {cx, static_cast<float>(BASE_RESOLUTION.y) - 32.0f}, with_alpha(G6, fade(2.4f))));
}

}
