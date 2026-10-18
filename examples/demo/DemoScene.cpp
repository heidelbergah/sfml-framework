/**
 * DemoScene.cpp
 *
 * Written by James Bouchat
 */

#include "DemoScene.hpp"

#include <cstdio>

namespace
{
    const std::string FONT_PATH  = "assets/fonts/default.ttf";
    const std::string BUTTON_TEX = "assets/textures/simple_button.png";

    // 12px was the original size and it reads correctly at 2x upscale.
    constexpr unsigned TEXT_SIZE = 12;

    constexpr float BLOOM_MAX     = 2.0f;
    constexpr float BLOOM_DEFAULT = 0.45f;

    const std::vector<sf::Color> EXPLOSION_COLORS = {
        sf::Color(203, 53, 61),
        sf::Color(249, 182, 78),
        sf::Color(237, 98, 64),
        sf::Color(86, 61, 67),
        sf::Color(106, 74, 87),
        sf::Color(255, 255, 255)
    };
}

DemoScene::DemoScene()
    : m_camera(sf::Vector2f{static_cast<float>(WINDOW_WIDTH),
                            static_cast<float>(WINDOW_HEIGHT)},
               sf::Vector2f{static_cast<float>(WINDOW_WIDTH) / 2.0f,
                            static_cast<float>(WINDOW_HEIGHT) / 2.0f})
{
    m_camera.set_transition(TransitionFunction::EaseOutQuad);
    m_camera.set_transition_duration(0.18f);

    build_widgets();
    build_particles();
}

void DemoScene::build_widgets()
{
    /*
     * Assets::font() rather than fonts().get() -- it disables glyph
     * smoothing, which is what keeps small text sharp. sf::Font smooths
     * its atlas by default, and at 12px on a target that is then scaled
     * up it turns everything to mush.
     */
    const sf::Font* font = Assets::font(FONT_PATH);
    if(!font)
    {
        std::cerr << "[DemoScene] no font at " << FONT_PATH
                  << ", running without text" << std::endl;
        return;
    }

    const sf::Texture* button_texture = Assets::texture(BUTTON_TEX);

    m_button = std::make_shared<Button>(sf::Vector2f{96.0f, 48.0f}, sf::Color(120, 170, 230));
    m_button->set_position({32.0f, 32.0f}, true);
    m_button->add_text("Click Me!", *font, sf::Color::Black);
    m_button->set_character_size(TEXT_SIZE);
    m_button->set_transition(TransitionFunction::EaseOutElastic);
    m_button->set_transition_duration(0.6f);
    if(button_texture) m_button->set_background_texture(*button_texture);

    // The callback fires exactly once per completed click now, rather
    // than every frame the mouse was held down.
    m_button->set_callback([this]() { spawn_confetti(m_button->get_position()); });
    m_widgets.add_widget("button", m_button);

    m_frame = std::make_shared<Frame>(sf::Vector2f{184.0f, 140.0f},
                                      sf::Color(40, 44, 70), *font);
    m_frame->set_position({420.0f, 90.0f}, true);
    m_frame->set_outline(sf::Color::Black, 2.0f);
    m_frame->add_taskbar(16.0f, sf::Color(200, 200, 215), sf::Color::Black, 1.0f,
                         "Drag Me");
    m_frame->set_z_value(10);
    m_frame->set_transition(TransitionFunction::EaseOutBack);
    m_frame->set_transition_duration(0.5f);

    auto frame_label = std::make_shared<TextBox>(
        sf::Vector2f{164.0f, 84.0f}, sf::Color(28, 30, 50),
        "Children sit in the\nframe's local space.", *font, sf::Color(210, 210, 230));
    frame_label->set_position({10.0f, 24.0f}, true);
    frame_label->set_character_size(TEXT_SIZE);
    m_frame->add_widget("label", frame_label);
    m_widgets.add_widget("frame", m_frame);

    m_info = std::make_shared<TextBox>(
        sf::Vector2f{300.0f, 110.0f}, sf::Color(24, 26, 42),
        "WASD pan    RMB explode    Space fire\n"
        "Enter taskbar    P pause    B bloom\n"
        "U: toggle whether the HUD shakes",
        *font, sf::Color(220, 220, 235));
    m_info->set_position({32.0f, 330.0f}, true);
    m_info->set_character_size(TEXT_SIZE);
    m_info->set_outline(sf::Color(90, 90, 120), 2.0f);
    m_info->set_z_value(20);
    m_info->add_text_scroll(28.0f);
    m_widgets.add_widget("info", m_info);

    m_bloom_label = std::make_shared<TextBox>(
        sf::Vector2f{150.0f, 20.0f}, sf::Color::Transparent,
        "bloom", *font, sf::Color(200, 200, 215));
    m_bloom_label->set_position({360.0f, 372.0f}, true);
    m_bloom_label->set_character_size(TEXT_SIZE);
    m_widgets.add_widget("bloom_label", m_bloom_label);

    m_slider = std::make_shared<Slider>(sf::Vector2f{150.0f, 14.0f}, sf::Color(240, 200, 90));
    m_slider->set_position({360.0f, 396.0f}, true);
    m_slider->set_callback([this]() { apply_bloom_intensity(); });
    m_slider->set_percentage(BLOOM_DEFAULT / BLOOM_MAX);
    m_widgets.add_widget("slider", m_slider);

    m_gauge = std::make_shared<Gauge>(sf::Vector2f{72.0f, 72.0f}, sf::Color(120, 230, 170),
                                      sf::Vector2f{0.0f, 100.0f},
                                      sf::Vector2f{-120.0f, 120.0f});
    m_gauge->set_position({580.0f, 366.0f}, true);
    m_gauge->set_dial_color(sf::Color(30, 34, 52));
    m_gauge->set_arm_transition(TransitionFunction::EaseOutBack);
    m_gauge->set_arm_transition_duration(0.35f);
    m_widgets.add_widget("gauge", m_gauge);
}

void DemoScene::build_particles()
{
    ParticleSystem confetti({32.0f, 32.0f});
    confetti.set_fade(true);
    confetti.set_drag(0.35f);            // per second, not per frame
    confetti.set_lifespan(sf::seconds(2.5f));
    confetti.set_gravity(sf::Vector2f{0.0f, 260.0f});
    confetti.set_gravity_enabled(true);
    m_particles.add_particle_system("confetti", std::move(confetti));

    ParticleSystem bullets({static_cast<float>(WINDOW_WIDTH) - 40.0f,
                            static_cast<float>(WINDOW_HEIGHT) / 2.0f});
    bullets.set_lifespan(sf::seconds(1.5f));
    bullets.set_drag(1.0f);              // frictionless
    m_particles.add_particle_system("bullets", std::move(bullets));
}

void DemoScene::on_enter()
{
    // Scoped, so it unsubscribes when the scene dies. The old Game
    // subscribed with a lambda capturing `this` and never unsubscribed.
    m_pause_subscription = subscribe_scoped(EventType::TOGGLE_PAUSE,
        [](const Event& e)
        {
            std::cout << "[DemoScene] pause event: " << e.m_data << std::endl;
        });

    if(m_manager) m_manager->set_bloom_enabled(m_bloom_on);

    // set_percentage() deliberately does not invoke the callback, so the
    // starting value has to be pushed through once by hand.
    apply_bloom_intensity();
}

void DemoScene::on_exit()
{
    m_pause_subscription.reset();
}

void DemoScene::spawn_confetti(sf::Vector2f where)
{
    auto system = m_particles.get_particle_system("confetti");
    if(!system) return;

    system->set_position(where);
    m_camera.shake(0.25f);

    for(const sf::Color& color : EXPLOSION_COLORS)
        system->add_particles_varied(18, color, Vector{140.0f, sf::degrees(90.0f)},
                                     180.0f, 40.0f);
}

void DemoScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    /*
     * Mapped through a view built from the camera's SETTLED centre, not
     * the live shaking one. Using the shaking view means that during a
     * screen shake a click lands wherever the camera happened to be
     * displaced to that frame, which is not where the player aimed.
     */
    if(m_manager)
    {
        sf::View aim_view = m_camera.get_view();
        aim_view.setCenter(m_camera.get_target_center());
        aim_view.setRotation(sf::Angle::Zero);
        m_mouse_world = m_manager->map_to_world(mouse_pos, aim_view);
    }
    else
    {
        m_mouse_world = mouse_pos;
    }

    // Widgets are positioned in screen space here, so they get the
    // unmapped position.
    m_widgets.handle_event(event, mouse_pos);

    if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        // Reads the key from the event rather than polling live keyboard
        // state. The old code did `if(event is KeyPressed)` and then
        // asked isKeyPressed(Enter), so holding Enter and tapping any
        // other key triggered the Enter branch.
        switch(key->code)
        {
            case sf::Keyboard::Key::Escape:
                EventBus::publish(Event(EventType::QUIT));
                break;

            case sf::Keyboard::Key::Enter:
                if(m_frame) m_frame->toggle_moveability();
                EventBus::publish(Event(EventType::TOGGLE_PAUSE, "toggled from demo"));
                break;

            case sf::Keyboard::Key::P:
                GameTime::is_paused() ? GameTime::resume() : GameTime::pause();
                break;

            case sf::Keyboard::Key::B:
                m_bloom_on = !m_bloom_on;
                if(m_manager) m_manager->set_bloom_enabled(m_bloom_on);
                break;

            case sf::Keyboard::Key::U:
                // Whether the HUD is welded to the screen or jolts with
                // the world. Both are legitimate; a tank cannon wants
                // the second one.
                m_hud_shakes = !m_hud_shakes;
                break;

            case sf::Keyboard::Key::Num1:
                m_particles.set_particle_system_position("confetti", {256.0f, 128.0f});
                if(m_button) m_button->set_position({256.0f, 128.0f});
                break;

            case sf::Keyboard::Key::Num2:
                m_particles.set_particle_system_position("confetti", {700.0f, 300.0f});
                if(m_button) m_button->set_position({700.0f, 300.0f});
                break;

            case sf::Keyboard::Key::Space:
                if(auto bullets = m_particles.get_particle_system("bullets"))
                {
                    m_camera.shake(0.06f);   // small, and it stacks
                    bullets->add_particles(1, sf::Color(255, 240, 200),
                                           Vector{620.0f, sf::degrees(180.0f)}, 3.0f);
                }
                break;

            default:
                break;
        }
    }

    if(const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if(pressed->button == sf::Mouse::Button::Right)
        {
            auto system = m_particles.get_particle_system("confetti");
            if(system)
            {
                system->set_position(m_mouse_world);
                m_camera.shake(0.85f);

                for(int i = 0; i < 220; ++i)
                {
                    system->add_particles_varied(
                        1, Random::pick(EXPLOSION_COLORS),
                        Vector{Random::real(60.0f, 300.0f), Random::angle()},
                        0.0f, 60.0f);
                }
            }
        }
    }
}

void DemoScene::update(sf::Time delta_time)
{
    // Held-key movement belongs in update, not in the event loop --
    // polling it per event meant the camera moved once per queued event
    // rather than once per frame.
    sf::Vector2f pan{0.0f, 0.0f};
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) pan.x -= 1.0f;
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) pan.x += 1.0f;
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) pan.y -= 1.0f;
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) pan.y += 1.0f;

    if(pan != sf::Vector2f{0.0f, 0.0f})
        m_camera.move(normalized(pan) * 220.0f * delta_time.asSeconds());

    m_widgets.update(delta_time);
    m_particles.update(delta_time);
    m_camera.update(delta_time);

    // Sweeps the gauge so its tween is visible.
    m_gauge_phase += delta_time.asSeconds();
    if(m_gauge)
        m_gauge->set_value(50.0f + 50.0f * std::sin(m_gauge_phase * 0.8f));
}

void DemoScene::render(sf::RenderTarget& target)
{
    // Particles live in world space and are drawn through the camera
    // view, so they pan and shake with it.
    target.setView(m_camera.get_view());
    target.draw(m_particles);

    /*
     * The HUD gets one of two views.
     *
     * get_ui_view() is screen space plus the shake offset: the widgets
     * stay pinned where you placed them and do not pan with the camera,
     * but the whole display jolts when something shakes it.
     *
     * getDefaultView() is screen space with no shake at all -- a HUD
     * welded to the glass.
     */
    target.setView(m_hud_shakes ? m_camera.get_ui_view()
                                : target.getDefaultView());
    target.draw(m_widgets);
}

void DemoScene::apply_bloom_intensity()
{
    if(!m_manager || !m_slider) return;

    float const intensity = m_slider->get_value(0.0f, BLOOM_MAX);
    m_manager->get_bloom().set_intensity(intensity);

    if(m_bloom_label)
    {
        char buffer[48];
        std::snprintf(buffer, sizeof(buffer), "bloom intensity  %.2f", intensity);
        m_bloom_label->set_string(buffer);
    }
}

sf::Color DemoScene::get_clear_color() const
{
    return sf::Color(18, 18, 26);
}
