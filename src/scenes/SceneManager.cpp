/**
 * SceneManager.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/scenes/SceneManager.hpp"

SceneManager::SceneManager(const std::string& title, bool fullscreen)
    : m_render_texture(BASE_RESOLUTION),
      m_render_sprite(m_render_texture.getTexture()),
      m_title(title)
{
    /*
     * Pick the largest whole-number upscale that actually fits on this
     * display, rather than always demanding WINDOW * SCALE.
     *
     * Asking for 1920x1080 on a 1920x1080 desktop does not fail --
     * the window manager quietly shrinks the window to make room for
     * the title bar, and everything downstream inherits a size nobody
     * asked for. Better to ask for something that fits.
     */
    sf::Vector2u window_size{SCALED_WIDTH, SCALED_HEIGHT};

    if(!fullscreen)
    {
        sf::VideoMode const desktop = sf::VideoMode::getDesktopMode();

        // Leave room for the title bar, taskbar and window borders.
        unsigned const usable_w = desktop.size.x;
        unsigned const usable_h = (desktop.size.y > 120u) ? desktop.size.y - 120u
                                                          : desktop.size.y;

        unsigned scale = SCALE;
        while(scale > 1u &&
              (WINDOW_WIDTH * scale > usable_w || WINDOW_HEIGHT * scale > usable_h))
        {
            --scale;
        }

        window_size = {WINDOW_WIDTH * scale, WINDOW_HEIGHT * scale};
    }

    m_window.create(sf::VideoMode(window_size), m_title,
                    sf::Style::Default,
                    fullscreen ? sf::State::Fullscreen : sf::State::Windowed);

    m_window.setVerticalSyncEnabled(true);

    // Held keys should not machine-gun KeyPressed events at the OS
    // repeat rate -- anything that accumulates per press runs away.
    m_window.setKeyRepeatEnabled(false);

    m_render_texture.setSmooth(false);

    m_fade_overlay.setSize({static_cast<float>(WINDOW_WIDTH),
                            static_cast<float>(WINDOW_HEIGHT)});

    update_presentation();

    setup_event_listeners();
}

SceneManager::~SceneManager()
{
    // Subscriptions unsubscribe themselves. The old destructor had to
    // remember to unsubscribe each handle by hand, and the handle names
    // it used had already drifted out of sync with the header.
    if(m_current_scene) m_current_scene->on_exit();
}

void SceneManager::setup_event_listeners()
{
    auto bind = [this](EventType type, std::string key, SceneTransitionType transition)
    {
        m_subscriptions.push_back(subscribe_scoped(type,
            [this, key = std::move(key), transition](const Event&)
            {
                request_scene_switch(key, transition);
            }));
    };

    bind(EventType::SWITCH_TO_MAIN_MENU,   "menu",     SceneTransitionType::FADE);
    bind(EventType::SWITCH_TO_GAME,        "game",     SceneTransitionType::FADE);
    bind(EventType::SWITCH_TO_SETTINGS,    "settings", SceneTransitionType::FADE);
    bind(EventType::SWITCH_TO_GAME_OVER,   "gameover", SceneTransitionType::FADE);
    bind(EventType::SWITCH_TO_PERK_SELECT, "perks",    SceneTransitionType::FADE);

    m_subscriptions.push_back(subscribe_scoped(EventType::QUIT,
        [this](const Event&) { quit(); }));

    m_subscriptions.push_back(subscribe_scoped(EventType::TOGGLE_PAUSE,
        [](const Event&)
        {
            GameTime::is_paused() ? GameTime::resume() : GameTime::pause();
        }));

    m_subscriptions.push_back(subscribe_scoped(EventType::PAUSE,
        [](const Event&) { GameTime::pause(); }));

    m_subscriptions.push_back(subscribe_scoped(EventType::RESUME,
        [](const Event&) { GameTime::resume(); }));
}

void SceneManager::add_scene(const std::string& key, std::shared_ptr<Scene> scene)
{
    if(!scene) return;

    scene->m_manager = this;
    m_scenes[key]    = std::move(scene);
}

void SceneManager::delete_scene(const std::string& key)
{
    auto it = m_scenes.find(key);
    if(it == m_scenes.end()) return;

    // Never leave m_current_scene dangling at a key that was removed.
    if(it->second == m_current_scene)
    {
        m_current_scene->on_exit();
        m_current_scene = nullptr;
    }

    m_scenes.erase(it);
}

bool SceneManager::has_scene(const std::string& key) const
{
    return m_scenes.find(key) != m_scenes.end();
}

void SceneManager::activate_scene(const std::shared_ptr<Scene>& scene)
{
    if(m_current_scene) m_current_scene->on_exit();
    m_current_scene = scene;
    if(m_current_scene) m_current_scene->on_enter();
}

void SceneManager::swap_scene(const std::string& key, SceneTransitionType transition)
{
    auto it = m_scenes.find(key);
    if(it == m_scenes.end())
    {
        std::cerr << "[SceneManager] no scene registered as \"" << key << "\""
                  << std::endl;
        return;
    }

    if(transition == SceneTransitionType::INSTANT || !m_current_scene)
    {
        activate_scene(it->second);
        m_phase = TransitionPhase::NONE;
        return;
    }

    begin_switch(key, transition);
}

void SceneManager::begin_switch(const std::string& key, SceneTransitionType transition)
{
    auto it = m_scenes.find(key);
    if(it == m_scenes.end()) return;

    (void)transition;

    m_incoming_scene = it->second;
    m_phase          = TransitionPhase::FADING_OUT;
    m_phase_elapsed  = 0.0f;
}

void SceneManager::request_scene_switch(const std::string& key,
                                        SceneTransitionType transition)
{
    m_switch_pending  = true;
    m_next_scene_key  = key;
    m_next_transition = transition;
}

void SceneManager::advance_transition(sf::Time delta_time)
{
    if(m_phase == TransitionPhase::NONE) return;

    // Unscaled, so a transition still runs while the game is paused.
    m_phase_elapsed += delta_time.asSeconds();

    if(m_phase_elapsed < m_fade_duration) return;

    if(m_phase == TransitionPhase::FADING_OUT)
    {
        activate_scene(m_incoming_scene);
        m_incoming_scene = nullptr;
        m_phase          = TransitionPhase::FADING_IN;
        m_phase_elapsed  = 0.0f;
    }
    else
    {
        m_phase         = TransitionPhase::NONE;
        m_phase_elapsed = 0.0f;
    }
}

sf::Vector2f SceneManager::current_mouse_position() const
{
    sf::Vector2i const pixel = sf::Mouse::getPosition(m_window);

    // Invert exactly the transform present() applies, rather than
    // assuming the window is SCALE times the base resolution.
    return m_present.to_base({static_cast<float>(pixel.x),
                             static_cast<float>(pixel.y)});
}

sf::Vector2f SceneManager::map_to_world(sf::Vector2f screen_pos, const sf::View& view) const
{
    return m_render_texture.mapPixelToCoords(
        sf::Vector2i{static_cast<int>(screen_pos.x), static_cast<int>(screen_pos.y)},
        view);
}

void SceneManager::update_presentation()
{
    /*
     * Works out how the 960x540 target maps onto the actual window, and
     * points the window's own view at its real pixel size.
     *
     * This is what the mouse bug came down to. The window's default view
     * is fixed at whatever size the window had when it was created, and
     * SFML does not update it on a resize -- it stretches the old view
     * to fill the new framebuffer instead. Ask for a 1920x1080 window on
     * a 1920x1080 display and the window manager shrinks it to fit the
     * title bar, so 1080 rows of content were being squashed into ~1000
     * physical rows.
     *
     * Dividing the cursor position by a hard-coded SCALE therefore drifted
     * further from the truth the further down the window you clicked,
     * and not at all along the top edge.
     */
    sf::Vector2u const window_size = m_window.getSize();
    if(window_size.x == 0 || window_size.y == 0) return;

    m_present = Letterbox::compute(window_size, BASE_RESOLUTION, m_scaling_mode);

    // 1:1 view, so window coordinates are real pixels.
    m_window.setView(sf::View{sf::FloatRect{{0.0f, 0.0f},
                                            {static_cast<float>(window_size.x),
                                             static_cast<float>(window_size.y)}}});

    m_render_sprite.setScale(m_present.scale);
    m_render_sprite.setPosition(m_present.offset);
}

void SceneManager::process_window_events()
{
    sf::Vector2f const mouse_pos = current_mouse_position();

    // Reset per-frame widget flags before this frame's events arrive.
    if(m_current_scene) m_current_scene->begin_frame();

    while(const std::optional event = m_window.pollEvent())
    {
        if(event->is<sf::Event::Closed>())
        {
            quit();
            return;
        }

        if(event->is<sf::Event::Resized>())
        {
            update_presentation();
        }

        // Events keep flowing during a transition so the window stays
        // responsive, but the outgoing scene should not act on them.
        if(m_current_scene && m_phase == TransitionPhase::NONE)
            m_current_scene->handle_event(*event, mouse_pos);
    }
}

void SceneManager::render_current(std::uint8_t fade_alpha)
{
    sf::Color const clear_color = m_current_scene ? m_current_scene->get_clear_color()
                                                 : sf::Color::Black;

    m_render_texture.setView(m_render_texture.getDefaultView());
    m_render_texture.clear(clear_color);

    if(m_current_scene) m_current_scene->render(m_render_texture);

    // Bloom reads the frame so far, so it has to run before the fade
    // overlay -- otherwise the black overlay would be blurred too.
    if(m_bloom_enabled && m_bloom.is_ready())
    {
        m_render_texture.display();
        m_render_texture.setView(m_render_texture.getDefaultView());
        m_bloom.apply(m_render_texture.getTexture(), m_render_texture);
    }

    if(fade_alpha > 0)
    {
        m_render_texture.setView(m_render_texture.getDefaultView());
        m_fade_overlay.setFillColor(sf::Color(0, 0, 0, fade_alpha));
        m_render_texture.draw(m_fade_overlay);
    }

    m_render_texture.display();
}

void SceneManager::present()
{
    m_render_sprite.setTexture(m_render_texture.getTexture(), true);

    m_window.clear(sf::Color::Black);
    m_window.draw(m_render_sprite);
    m_window.display();
}

void SceneManager::run(int frames_per_second)
{
    if(frames_per_second <= 0) frames_per_second = 60;

    sf::Time const time_per_frame = sf::seconds(1.0f / static_cast<float>(frames_per_second));
    sf::Time time_since_last_update = sf::Time::Zero;

    m_running = true;
    m_clock.restart();

    while(m_running && m_window.isOpen())
    {
        process_window_events();
        if(!m_running) break;

        time_since_last_update += m_clock.restart();

        // Clamp the accumulator. Without this, a long stall -- dragging
        // the window, a breakpoint, the machine sleeping -- queues up
        // hundreds of updates that all run at once, and the game either
        // teleports or locks up trying to catch up.
        sf::Time const max_accumulated = time_per_frame * 5.0f;
        if(time_since_last_update > max_accumulated)
            time_since_last_update = max_accumulated;

        bool repaint = false;

        while(time_since_last_update >= time_per_frame)
        {
            time_since_last_update -= time_per_frame;
            repaint = true;

            // One authoritative clock advance per fixed step. Every
            // Interpolated in the game reads from this.
            GameTime::advance(time_per_frame);

            // Transitions run on real time, so a fade still completes
            // if the game happens to be paused.
            advance_transition(time_per_frame);

            if(m_current_scene)
            {
                /*
                 * Scenes get a scaled delta. Without this, a scene that
                 * accumulates raw dt keeps advancing its own logic while
                 * paused, while every Interpolated it drives is frozen --
                 * so on unpause the tween finds its target has moved far
                 * away and races to catch up.
                 */
                sf::Time const scaled = time_per_frame * GameTime::get_scale();

                m_current_scene->update(scaled);
                m_current_scene->update_realtime(time_per_frame);
            }
        }

        if(repaint)
        {
            float alpha = 0.0f;
            if(m_phase != TransitionPhase::NONE && m_fade_duration > 0.0f)
            {
                float const t = std::clamp(m_phase_elapsed / m_fade_duration, 0.0f, 1.0f);
                alpha = (m_phase == TransitionPhase::FADING_OUT) ? t : (1.0f - t);
            }

            render_current(static_cast<std::uint8_t>(alpha * 255.0f));
            present();
        }

        // Applied between frames, so a scene is never swapped out from
        // underneath its own update or event handler.
        if(m_switch_pending)
        {
            m_switch_pending = false;
            swap_scene(m_next_scene_key, m_next_transition);
        }
    }

    m_running = false;
}

void SceneManager::quit()
{
    m_running = false;
    if(m_window.isOpen()) m_window.close();
}

void SceneManager::set_fade_duration(float seconds)
{
    m_fade_duration = std::max(0.0f, seconds);
}

void SceneManager::set_scaling_mode(ScalingMode mode)
{
    m_scaling_mode = mode;
    update_presentation();
}

ScalingMode SceneManager::get_scaling_mode() const
{
    return m_scaling_mode;
}

void SceneManager::set_key_repeat_enabled(bool enabled)
{
    m_window.setKeyRepeatEnabled(enabled);
}

const Letterbox& SceneManager::get_presentation() const
{
    return m_present;
}

bool SceneManager::enable_bloom(const std::string& shader_directory)
{
    m_shaders.load_fragment("bloom_extract", shader_directory + "/bloom_extract.frag");
    m_shaders.load_fragment("bloom_blur_v",  shader_directory + "/bloom_blur_v.frag");

    m_bloom_enabled = m_bloom.initialize(BASE_RESOLUTION, m_shaders);
    return m_bloom_enabled;
}

void SceneManager::set_bloom_enabled(bool enabled)
{
    m_bloom_enabled = enabled && m_bloom.is_ready();
}

BloomEffect& SceneManager::get_bloom()
{
    return m_bloom;
}

ShaderManager& SceneManager::get_shaders()
{
    return m_shaders;
}

sf::RenderWindow& SceneManager::get_window()
{
    return m_window;
}
