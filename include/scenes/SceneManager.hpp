/**
 * SceneManager.hpp
 *
 * Owns the window, the render target and the game loop, and moves
 * between scenes.
 *
 * This is the single entry point for the framework. The old Game class
 * duplicated all of this, its own window, render texture, sprite,
 * clock and fixed-timestep loop, which meant two competing owners of
 * the same responsibilities.
 *
 * Scene transitions are a state machine advanced by the main loop, not
 * a blocking while loop. The old fade ran its own inner loop that never
 * polled events, so the window was frozen and unclosable for its whole
 * duration and never cleared the target between frames.
 */

#ifndef SCENE_MANAGER_HPP
#define SCENE_MANAGER_HPP

#include "Scene.hpp"

#include "../framework/BloomEffect.hpp"
#include "../framework/EventBus.hpp"
#include "../framework/ShaderManager.hpp"

enum class SceneTransitionType
{
    INSTANT,
    FADE
};

class SceneManager
{
private:
    enum class TransitionPhase
    {
        NONE,
        FADING_OUT,
        FADING_IN
    };

    sf::RenderWindow  m_window;
    sf::RenderTexture m_render_texture;
    sf::Sprite        m_render_sprite;
    sf::Clock         m_clock;

    ShaderManager m_shaders;
    BloomEffect   m_bloom;
    bool          m_bloom_enabled{false};

    std::unordered_map<std::string, std::shared_ptr<Scene>> m_scenes;
    std::shared_ptr<Scene> m_current_scene;

    // Pending switch, applied at a safe point in the loop rather than
    // partway through an event callback.
    bool                m_switch_pending{false};
    std::string         m_next_scene_key;
    SceneTransitionType m_next_transition{SceneTransitionType::INSTANT};

    TransitionPhase        m_phase{TransitionPhase::NONE};
    float                  m_phase_elapsed{0.0f};
    float                  m_fade_duration{0.3f};
    std::shared_ptr<Scene> m_incoming_scene;
    sf::RectangleShape     m_fade_overlay;

    std::string m_title{"SFML Framework"};
    bool        m_running{false};

    // How the low-res target is mapped onto the window. Recomputed on
    // creation and on every resize; mouse input is inverted through it.
    Letterbox   m_present;
    ScalingMode m_scaling_mode{ScalingMode::STRETCH};

    std::vector<Subscription> m_subscriptions;

    void setup_event_listeners();
    void update_presentation();
    void process_window_events();
    void advance_transition(sf::Time delta_time);
    void begin_switch(const std::string& key, SceneTransitionType transition);
    void activate_scene(const std::shared_ptr<Scene>& scene);
    void render_current(std::uint8_t fade_alpha);
    void present();

    [[nodiscard]] sf::Vector2f current_mouse_position() const;

public:
    explicit SceneManager(const std::string& title = "SFML Framework",
                          bool fullscreen = false);
    ~SceneManager();

    SceneManager(const SceneManager&)            = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    void add_scene(const std::string& key, std::shared_ptr<Scene> scene);
    void delete_scene(const std::string& key);

    [[nodiscard]] bool has_scene(const std::string& key) const;

    /*
     * Switches immediately. Use for the initial scene; prefer
     * request_scene_switch() from inside a running scene, so the change
     * lands at a clean point in the loop.
     */
    void swap_scene(const std::string& key, SceneTransitionType transition);

    /*
     * Queues a switch, applied after the current frame completes. Safe
     * to call from an update, an event handler or an EventBus callback.
     */
    void request_scene_switch(const std::string& key, SceneTransitionType transition);

    /*
     * Runs until the window closes. `frames_per_second` sets the fixed
     * update rate.
     */
    void run(int frames_per_second = 60);

    void quit();

    void set_fade_duration(float seconds);

    /*
     * How the render target fills the window. STRETCH by default:
     * no black bars, at the cost of a couple of percent of distortion
     * in a maximised window. See ScalingMode for the alternatives.
     */
    void set_scaling_mode(ScalingMode mode);
    [[nodiscard]] ScalingMode get_scaling_mode() const;

    /*
     * Whether a held key produces repeated KeyPressed events.
     *
     * OFF by default, which is almost always what a game wants: with it
     * on, holding a key fires its action at the OS repeat rate, so
     * anything that accumulates per keypress runs away. Turn it on if
     * you add a text input widget.
     */
    void set_key_repeat_enabled(bool enabled);

    /*
     * How the render target is currently mapped onto the window.
     * Useful for converting raw window pixels yourself.
     */
    [[nodiscard]] const Letterbox& get_presentation() const;

    /*
     * Loads the bloom shaders and turns the effect on. Returns false if
     * the shaders are unavailable, in which case rendering continues
     * without bloom.
     */
    bool enable_bloom(const std::string& shader_directory = "shaders");
    void set_bloom_enabled(bool enabled);
    [[nodiscard]] BloomEffect& get_bloom();

    [[nodiscard]] ShaderManager&    get_shaders();
    [[nodiscard]] sf::RenderWindow& get_window();

    /*
     * Maps a base-resolution point through a view into world space.
     * Use with your scene's camera view for world-space mouse input.
     */
    [[nodiscard]] sf::Vector2f map_to_world(sf::Vector2f screen_pos,
                                            const sf::View& view) const;
};

#endif
