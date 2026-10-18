/**
 * ParticleScene.cpp  -  Showcase
 */

#include "ParticleScene.hpp"

#include <cstdio>

namespace showcase
{

namespace
{
    // Order matters: setting(index) reads these by position.
    enum : std::size_t { RATE, SPEED, SPREAD, LIFE, GRAVITY, DRAG };

    constexpr float PANEL_W   = 248.0f;
    constexpr float PANEL_H   = 232.0f;
    constexpr float BAR_H     = 22.0f;
    constexpr float ROW_TOP   = 34.0f;
    constexpr float ROW_STEP  = 26.0f;

    const sf::Vector2f FOUNTAIN{VIEW_W / 2.0f, VIEW_H - 70.0f};
}

ParticleScene::ParticleScene()
    : DemoScene("Particle sandbox", "hold left mouse to paint   right click  settings")
{
    m_brush = std::make_shared<ParticleSystem>(FOUNTAIN);
    m_brush->set_max_particles(12000);
    m_particles.add_particle_system("brush", m_brush);

    // Two colours per palette; each particle picks a point between them.
    // Kept below full brightness so a lone particle stays soft and only
    // overlapping ones add up to a glow.
    m_palettes = {
        {{255, 168,  70, 225}, {226,  84,  64, 225}},   // ember
        {{100, 178, 236, 225}, {120, 220, 162, 225}},   // tide
        {{232, 120, 186, 225}, {255, 186,  96, 225}},   // dusk
        {{226, 214, 192, 210}, {130, 120, 130, 210}},   // ash
    };

    m_count = make_label({200.0f, 40.0f}, "", TEXT_DIM, Alignment::RIGHT);
    m_count->set_position({VIEW_W - 224.0f, 16.0f}, true);
    m_widgets.add_widget("count", m_count);

    build_panel();
    select_palette(0);
    set_fade(true);
    apply_settings();
}

void ParticleScene::build_panel()
{
    m_panel = std::make_shared<Frame>(sf::Vector2f{PANEL_W, PANEL_H}, PANEL, *font());
    m_panel->set_outline(LINE, 1.0f);
    m_panel->set_z_value(50);
    m_panel->set_visible(false);
    m_panel->set_transition(TransitionFunction::EaseOutBack);
    m_panel->set_transition_duration(0.28f);
    m_widgets.add_widget("panel", m_panel);

    m_panel->add_taskbar(BAR_H, "  emitter", taskbar_style());

    struct Spec { const char* name; float min; float max; float start; const char* format; };
    Spec const specs[] = {
        {"rate",    20.0f, 1500.0f, 0.46f, "%.0f"},
        {"speed",   20.0f,  400.0f, 0.62f, "%.0f"},
        {"spread",   0.0f,  180.0f, 0.08f, "%.0f"},
        {"life",     0.3f,    4.0f, 0.46f, "%.1fs"},
        {"gravity",  0.0f,  600.0f, 0.22f, "%.0f"},
        {"drag",     0.05f,   1.0f, 0.60f, "%.2f"},
    };

    for(std::size_t i = 0; i < std::size(specs); ++i)
    {
        Spec const& spec = specs[i];
        float const y    = ROW_TOP + ROW_STEP * static_cast<float>(i);

        auto label = make_label({64.0f, 20.0f}, spec.name, TEXT_DIM);
        label->set_position({10.0f, y}, true);
        m_panel->add_widget(std::string("label_") + spec.name, label);

        auto slider = std::make_shared<Slider>(sf::Vector2f{116.0f, 12.0f}, CORAL);
        slider->set_body_color(LINE);
        slider->set_transition(TransitionFunction::None);
        slider->set_position({78.0f, y + 4.0f}, true);
        slider->set_percentage(spec.start);
        m_panel->add_widget(std::string("slider_") + spec.name, slider);

        auto value = make_label({44.0f, 20.0f}, "", TEXT, Alignment::RIGHT);
        value->set_position({PANEL_W - 54.0f, y}, true);
        m_panel->add_widget(std::string("value_") + spec.name, value);

        m_settings.push_back({spec.name, spec.min, spec.max, spec.format, slider, value});
    }

    float const bottom_y = ROW_TOP + ROW_STEP * static_cast<float>(std::size(specs)) + 6.0f;

    for(std::size_t i = 0; i < m_palettes.size(); ++i)
    {
        sf::Color swatch = mix(m_palettes[i].a, m_palettes[i].b, 0.35f);
        swatch.a = 255;

        auto button = make_button({20.0f, 20.0f}, "", swatch);
        button->set_position({10.0f + 26.0f * static_cast<float>(i), bottom_y}, true);
        button->set_callback([this, i]() { select_palette(i); });
        m_panel->add_widget("swatch_" + std::to_string(i), button);
        m_swatches.push_back(button);
    }

    m_fade_button = make_button({66.0f, 20.0f}, "");
    m_fade_button->set_position({PANEL_W - 130.0f, bottom_y}, true);
    m_fade_button->set_callback([this]() { set_fade(!m_fade); });
    m_panel->add_widget("fade", m_fade_button);

    auto clear = make_button({52.0f, 20.0f}, "clear");
    clear->set_position({PANEL_W - 60.0f, bottom_y}, true);
    clear->set_callback([this]() { m_brush->clear(); });
    m_panel->add_widget("clear", clear);
}

void ParticleScene::open_panel(sf::Vector2f at)
{
    // Open beside the cursor, but never off screen.
    sf::Vector2f const target{
        std::clamp(at.x + 10.0f, 8.0f, VIEW_W - PANEL_W - 8.0f),
        std::clamp(at.y + 10.0f, 8.0f, VIEW_H - PANEL_H - 8.0f)};

    if(!m_panel->is_visible())
    {
        m_panel->set_position(target + sf::Vector2f{0.0f, 14.0f}, true);
        m_panel->set_visible(true);
    }
    m_panel->set_position(target);
}

void ParticleScene::close_panel()
{
    m_panel->set_visible(false);
}

void ParticleScene::select_palette(std::size_t index)
{
    m_palette = index;

    // A thin light border marks the active swatch.
    for(std::size_t i = 0; i < m_swatches.size(); ++i)
        m_swatches[i]->set_outline(i == index ? TEXT : sf::Color::Transparent,
                                   i == index ? 2.0f : 0.0f);
}

void ParticleScene::set_fade(bool fade)
{
    m_fade = fade;
    m_brush->set_fade(m_fade);
    m_fade_button->add_text(m_fade ? "fade on" : "fade off", *font(),
                            m_fade ? TEXT : TEXT_DIM);
}

float ParticleScene::setting(std::size_t index) const
{
    return m_settings[index].get();
}

void ParticleScene::apply_settings()
{
    m_brush->set_lifespan(sf::seconds(setting(LIFE)));
    m_brush->set_drag(setting(DRAG));

    float const gravity = setting(GRAVITY);
    m_brush->set_gravity(sf::Vector2f{0.0f, gravity});
    m_brush->set_gravity_enabled(gravity > 0.5f);

    for(Setting& s : m_settings)
    {
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), s.format, static_cast<double>(s.get()));
        s.value->set_string(buffer);
    }
}

bool ParticleScene::over_panel(sf::Vector2f point) const
{
    return m_panel->is_visible() && m_panel->contains(point);
}

void ParticleScene::on_enter()
{
    DemoScene::on_enter();

    if(m_manager) m_manager->set_bloom_enabled(true);

    m_brush->clear();
    m_brush->set_position(FOUNTAIN);
    m_demo_mode = true;
    m_painting  = false;
    close_panel();
}

void ParticleScene::on_exit()
{
    if(m_manager) m_manager->set_bloom_enabled(false);
}

void ParticleScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    // Escape closes the panel first, and only leaves on a second press.
    if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        if(key->code == sf::Keyboard::Key::Escape && m_panel->is_visible())
        {
            close_panel();
            return;
        }
    }

    DemoScene::handle_event(event, mouse_pos);

    if(const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if(pressed->button == sf::Mouse::Button::Right)
        {
            open_panel(mouse_pos);
        }
        else if(pressed->button == sf::Mouse::Button::Left && !over_panel(mouse_pos))
        {
            m_painting  = true;
            m_demo_mode = false;
            m_mouse     = mouse_pos;
        }
    }
    else if(const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if(released->button == sf::Mouse::Button::Left) m_painting = false;
    }
    else if(event.is<sf::Event::MouseMoved>())
    {
        m_mouse = mouse_pos;
    }
}

void ParticleScene::update(sf::Time delta_time)
{
    DemoScene::update(delta_time);

    float const dt = delta_time.asSeconds();
    m_time += dt;

    if(m_panel->close_requested()) close_panel();

    // Dragging by the title bar is free-form; keep at least the bar on
    // screen so the panel can always be grabbed again.
    if(m_panel->is_being_dragged())
    {
        sf::Vector2f const pos = m_panel->get_position();
        sf::Vector2f const kept{std::clamp(pos.x, 40.0f - PANEL_W, VIEW_W - 40.0f),
                                std::clamp(pos.y, 0.0f, VIEW_H - BAR_H)};
        if(kept != pos) m_panel->set_position(kept, true);
    }

    bool const any_changed = std::any_of(m_settings.begin(), m_settings.end(),
        [](const Setting& s) { return s.slider->value_changed(); });
    if(any_changed) apply_settings();

    // Emit at the configured rate, carrying the remainder between frames
    // so low rates still come out evenly.
    bool const emitting = m_painting || m_demo_mode;
    if(emitting)
    {
        sf::Vector2f const at = m_demo_mode ? FOUNTAIN : m_mouse;
        m_brush->set_position(at);

        m_emit_accumulator += setting(RATE) * dt;
        int const count = static_cast<int>(m_emit_accumulator);
        m_emit_accumulator -= static_cast<float>(count);

        // The fountain sways a little so it never looks static.
        float const sway = m_demo_mode ? std::sin(m_time * 0.9f) * 8.0f : 0.0f;

        Palette const& palette = m_palettes[m_palette];
        for(int i = 0; i < count; ++i)
        {
            sf::Color const color = mix(palette.a, palette.b, Random::real(0.0f, 1.0f));
            m_brush->add_particles_varied(1, color,
                Vector{setting(SPEED), sf::degrees(90.0f + sway)},
                setting(SPREAD), setting(SPEED) * 0.18f);
        }
    }
    else
    {
        m_emit_accumulator = 0.0f;
    }

    m_particles.update(delta_time);

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%zu particles", m_particles.total_particle_count());
    m_count->set_string(buffer);
}

void ParticleScene::render(sf::RenderTarget& target)
{
    target.setView(target.getDefaultView());

    sf::RenderStates additive;
    additive.blendMode = sf::BlendAdd;
    target.draw(m_particles, additive);

    target.draw(m_widgets);
}

}
