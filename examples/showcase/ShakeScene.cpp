/**
 * ShakeScene.cpp  -  Showcase
 */

#include "ShakeScene.hpp"

#include <cstdio>

namespace showcase
{

namespace
{
    constexpr float GROUND_Y  = 444.0f;
    constexpr float MARGIN    = 48.0f;      // world extends past the screen, so shake never shows an edge

    constexpr float PANEL_W   = 280.0f;
    constexpr float PANEL_H   = 226.0f;

    const sf::Vector2f GRAPH_POS{92.0f, 32.0f};
    const sf::Vector2f GRAPH_SIZE{176.0f, 64.0f};
    constexpr std::size_t HISTORY = 176;    // one sample per pixel column

    constexpr float RUMBLE_MIN = 0.10f;
    constexpr float RUMBLE_MAX = 0.60f;

    const sf::Color SKY_TOP {26, 23, 34};
    const sf::Color FAR     {36, 33, 46};
    const sf::Color NEAR    {48, 43, 58};
    const sf::Color GROUND  {30, 27, 36};
}

ShakeScene::ShakeScene()
    : DemoScene("Shake lab", "1 2 3  impulses   hold space  rumble"),
      m_camera({VIEW_W, VIEW_H}, {VIEW_W / 2.0f, VIEW_H / 2.0f})
{
    m_sparks = std::make_shared<ParticleSystem>();
    m_sparks->set_lifespan(sf::seconds(0.9f));
    m_sparks->set_gravity(sf::Vector2f{0.0f, 114.0f});
    m_sparks->set_gravity_enabled(true);
    m_sparks->set_drag(0.25f);
    m_sparks->set_fade(true);
    m_particles.add_particle_system("sparks", m_sparks);

    build_world();
    build_panel();
}

void ShakeScene::build_world()
{
    m_world.clear();
    m_roofs.clear();

    float const left  = -MARGIN;
    float const right = VIEW_W + MARGIN;

    // Night sky, a shade lighter near the horizon.
    push_rect(m_world, {{left, -MARGIN}, {right - left, GROUND_Y + MARGIN}}, SKY_TOP);
    push_rect(m_world, {{left, GROUND_Y - 120.0f}, {right - left, 120.0f}},
              mix(SKY_TOP, FAR, 0.35f));

    // A private, fixed-seed generator: the skyline comes out the same
    // every run, and the shared Random:: sequence is left alone.
    std::mt19937 rng{7};
    auto real = [&rng](float lo, float hi)
    {
        return std::uniform_real_distribution<float>{lo, hi}(rng);
    };
    auto chance = [&real](float p) { return real(0.0f, 1.0f) < p; };

    // A handful of stars.
    for(int i = 0; i < 70; ++i)
    {
        sf::Vector2f const at{std::floor(real(left, right)),
                              std::floor(real(-MARGIN, GROUND_Y - 160.0f))};
        push_rect(m_world, {at, {1.0f, 1.0f}}, fade(TEXT, real(0.15f, 0.5f)));
    }

    // Two rows of buildings, the far row taller and darker.
    auto row = [&](sf::Color body, float min_h, float max_h, float min_w, float max_w,
                   float window_chance, bool record_roofs)
    {
        float x = left + real(-20.0f, 0.0f);
        while(x < right)
        {
            float const w = std::floor(real(min_w, max_w));
            float const h = std::floor(real(min_h, max_h));
            float const top = GROUND_Y - h;

            push_rect(m_world, {{x, top}, {w, h}}, body);

            // Windows on a 7 x 9 grid, a few of them lit.
            for(float wy = top + 8.0f; wy < GROUND_Y - 10.0f; wy += 9.0f)
            {
                for(float wx = x + 5.0f; wx < x + w - 6.0f; wx += 7.0f)
                {
                    if(!chance(window_chance)) continue;
                    sf::Color const lit = chance(0.75f) ? AMBER : SKY;
                    push_rect(m_world, {{wx, wy}, {3.0f, 4.0f}},
                              fade(lit, real(0.35f, 0.8f)));
                }
            }

            if(record_roofs) m_roofs.push_back({x, x + w, top});
            x += w + std::floor(real(2.0f, 14.0f));
        }
    };

    row(FAR,  140.0f, 250.0f, 34.0f, 70.0f, 0.06f, false);
    row(NEAR,  60.0f, 160.0f, 40.0f, 84.0f, 0.12f, true);

    push_rect(m_world, {{left, GROUND_Y}, {right - left, VIEW_H + MARGIN - GROUND_Y}}, GROUND);
    push_rect(m_world, {{left, GROUND_Y}, {right - left, 1.0f}}, LINE);

    m_moon.setRadius(16.0f);
    m_moon.setPointCount(24);
    m_moon.setFillColor(mix(TEXT, INK, 0.25f));
    m_moon.setPosition({120.0f, 88.0f});
}

void ShakeScene::build_panel()
{
    m_panel = std::make_shared<Frame>(sf::Vector2f{PANEL_W, PANEL_H}, PANEL, *font());
    m_panel->set_outline(LINE, 1.0f);
    m_panel->set_transition(TransitionFunction::EaseOutBack);
    m_panel->set_transition_duration(0.5f);
    m_widgets.add_widget("panel", m_panel);

    m_gauge_value = make_label({64.0f, 20.0f}, "0.00", TEXT_DIM, Alignment::CENTER);
    m_gauge_value->set_position({12.0f, 8.0f}, true);
    m_panel->add_widget("gauge_value", m_gauge_value);

    m_gauge = std::make_shared<Gauge>(sf::Vector2f{64.0f, 64.0f}, AMBER,
                                      sf::Vector2f{0.0f, 1.0f},
                                      sf::Vector2f{-120.0f, 120.0f});
    m_gauge->set_dial_color(PANEL_HI);
    m_gauge->set_arm_transition(TransitionFunction::EaseOutQuad);
    m_gauge->set_arm_transition_duration(0.08f);
    m_gauge->set_transition(TransitionFunction::None);
    m_gauge->set_position({12.0f, 32.0f}, true);
    m_gauge->set_value(0.0f, true);
    m_panel->add_widget("gauge", m_gauge);

    auto legend_a = make_label({80.0f, 20.0f}, "trauma", AMBER);
    legend_a->set_position({GRAPH_POS.x, 8.0f}, true);
    m_panel->add_widget("legend_a", legend_a);

    auto legend_b = make_label({80.0f, 20.0f}, "floor", SKY, Alignment::RIGHT);
    legend_b->set_position({GRAPH_POS.x + GRAPH_SIZE.x - 80.0f, 8.0f}, true);
    m_panel->add_widget("legend_b", legend_b);

    struct Impulse { const char* name; float amount; int sparks; };
    Impulse const impulses[] = {{"tap", 0.2f, 0}, {"hit", 0.5f, 40}, {"boom", 1.0f, 160}};

    for(std::size_t i = 0; i < std::size(impulses); ++i)
    {
        Impulse const imp = impulses[i];
        auto button = make_button({80.0f, 24.0f}, imp.name);
        button->set_position({12.0f + 88.0f * static_cast<float>(i), 110.0f}, true);
        button->set_callback([this, imp]() { impulse(imp.amount, imp.sparks); });
        m_panel->add_widget(std::string("impulse_") + imp.name, button);
    }

    m_rumble = make_button({80.0f, 24.0f}, "rumble");
    m_rumble->set_position({12.0f, 144.0f}, true);
    m_panel->add_widget("rumble", m_rumble);

    m_rumble_strength = std::make_shared<Slider>(sf::Vector2f{116.0f, 12.0f}, SKY);
    m_rumble_strength->set_body_color(LINE);
    m_rumble_strength->set_transition(TransitionFunction::None);
    m_rumble_strength->set_position({102.0f, 150.0f}, true);
    m_rumble_strength->set_percentage(0.35f);
    m_panel->add_widget("rumble_strength", m_rumble_strength);

    m_rumble_label = make_label({44.0f, 24.0f}, "", TEXT, Alignment::RIGHT);
    m_rumble_label->set_position({PANEL_W - 56.0f, 144.0f}, true);
    m_panel->add_widget("rumble_label", m_rumble_label);

    m_hud_toggle = make_button({124.0f, 24.0f}, "");
    m_hud_toggle->set_position({12.0f, 186.0f}, true);
    m_hud_toggle->set_callback([this]() { set_hud_shakes(!m_hud_shakes); });
    m_panel->add_widget("hud_toggle", m_hud_toggle);

    m_roll_toggle = make_button({124.0f, 24.0f}, "");
    m_roll_toggle->set_position({PANEL_W - 136.0f, 186.0f}, true);
    m_roll_toggle->set_callback([this]() { set_roll(!m_roll); });
    m_panel->add_widget("roll_toggle", m_roll_toggle);

    set_hud_shakes(true);
    set_roll(false);
}

float ShakeScene::rumble_amount() const
{
    return m_rumble_strength->get_value(RUMBLE_MIN, RUMBLE_MAX);
}

void ShakeScene::impulse(float amount, int sparks)
{
    m_camera.shake(amount);

    if(sparks <= 0 || m_roofs.empty()) return;

    // Something on the skyline takes the hit.
    Roof const& roof = Random::pick(m_roofs);
    sf::Vector2f const at{Random::real(roof.left + 6.0f, roof.right - 6.0f), roof.y};

    m_sparks->set_position(at);
    for(int i = 0; i < sparks; ++i)
    {
        sf::Color const color = Random::chance(0.7f) ? AMBER : CORAL;
        m_sparks->add_particles_varied(1, color, Vector{90.0f + amount * 140.0f,
                                       sf::degrees(90.0f)}, 70.0f, 60.0f);
    }
}

void ShakeScene::set_hud_shakes(bool on)
{
    m_hud_shakes = on;
    m_hud_toggle->add_text(on ? "hud  shakes" : "hud  steady", *font(), TEXT);
}

void ShakeScene::set_roll(bool on)
{
    m_roll = on;
    m_camera.set_shake_roll_enabled(on);
    m_roll_toggle->add_text(on ? "roll  on" : "roll  off", *font(), on ? TEXT : TEXT_DIM);
}

void ShakeScene::on_enter()
{
    DemoScene::on_enter();

    m_camera.clear_shake();
    m_sparks->clear();
    m_trauma_history.assign(HISTORY, 0.0f);
    m_floor_history.assign(HISTORY, 0.0f);
    m_space_held = false;

    sf::Vector2f const home{VIEW_W - PANEL_W - 24.0f, 20.0f};
    m_panel->set_position(home + sf::Vector2f{PANEL_W + 40.0f, 0.0f}, true);
    m_panel->set_position(home);
}

void ShakeScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    DemoScene::handle_event(event, mouse_pos);

    if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        switch(key->code)
        {
            case sf::Keyboard::Key::Num1:  impulse(0.2f, 0);   break;
            case sf::Keyboard::Key::Num2:  impulse(0.5f, 40);  break;
            case sf::Keyboard::Key::Num3:  impulse(1.0f, 160); break;
            case sf::Keyboard::Key::Space: m_space_held = true; break;
            default: break;
        }
    }
    else if(const auto* released = event.getIf<sf::Event::KeyReleased>())
    {
        if(released->code == sf::Keyboard::Key::Space) m_space_held = false;
    }
}

void ShakeScene::update(sf::Time delta_time)
{
    DemoScene::update(delta_time);

    // A sustained floor only holds while it keeps being asked for.
    if(m_rumble->is_pressed() || m_space_held)
        m_camera.set_sustained_shake(rumble_amount());

    m_camera.update(delta_time);
    m_particles.update(delta_time);

    float const trauma = std::min(1.0f, m_camera.get_trauma() + m_camera.get_sustained_shake());
    m_gauge->set_value(trauma);

    m_trauma_history.push_back(trauma);
    m_floor_history.push_back(m_camera.get_sustained_shake());
    while(m_trauma_history.size() > HISTORY) m_trauma_history.pop_front();
    while(m_floor_history.size()  > HISTORY) m_floor_history.pop_front();
    rebuild_graph();

    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%.2f", static_cast<double>(rumble_amount()));
    m_rumble_label->set_string(buffer);

    std::snprintf(buffer, sizeof(buffer), "%.2f", static_cast<double>(trauma));
    m_gauge_value->set_string(buffer);
    m_gauge_value->set_text_color(trauma > 0.005f ? AMBER : TEXT_DIM);
}

void ShakeScene::rebuild_graph()
{
    m_graph.clear();

    sf::Vector2f const origin = floor_vector(m_panel->get_position()) + GRAPH_POS;
    push_rect(m_graph, {origin, GRAPH_SIZE}, INK);

    // Gridlines at a quarter, half and three quarters.
    for(float q : {0.25f, 0.5f, 0.75f})
    {
        float const y = std::floor(origin.y + GRAPH_SIZE.y * (1.0f - q));
        push_rect(m_graph, {{origin.x, y}, {GRAPH_SIZE.x, 1.0f}}, fade(LINE, 0.5f));
    }

    // One column per sample: the floor as a filled band, trauma as a
    // 2px line above it.
    for(std::size_t i = 0; i < m_trauma_history.size(); ++i)
    {
        float const x  = origin.x + static_cast<float>(i);
        float const fh = std::round(m_floor_history[i] * (GRAPH_SIZE.y - 2.0f));
        float const th = std::round(m_trauma_history[i] * (GRAPH_SIZE.y - 2.0f));
        float const bottom = origin.y + GRAPH_SIZE.y;

        if(fh > 0.0f)
            push_rect(m_graph, {{x, bottom - fh}, {1.0f, fh}}, fade(SKY, 0.45f));

        push_rect(m_graph, {{x, bottom - th - 2.0f}, {1.0f, 2.0f}}, AMBER);
    }
}

void ShakeScene::render(sf::RenderTarget& target)
{
    // World: pans and shakes.
    target.setView(m_camera.get_view());
    target.draw(m_world);
    target.draw(m_moon);

    sf::RenderStates additive;
    additive.blendMode = sf::BlendAdd;
    target.draw(m_particles, additive);

    // Panel: either joins the shake or ignores it entirely.
    target.setView(m_hud_shakes ? m_camera.get_ui_view() : target.getDefaultView());
    target.draw(m_widgets);
    target.draw(m_graph);
}

}
