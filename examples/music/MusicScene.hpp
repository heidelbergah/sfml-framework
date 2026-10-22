/**
 * MusicScene.hpp  -  Music lab
 *
 * Two tabs over one scrolling list.
 *
 * Music: every track in assets/music, played through the effect chain and
 * drawn live on an oscilloscope. Sliders change the effects while it
 * plays, and presets try the music out in different places in the game:
 * inside the tank, on the field radio, late in the war, or right after a
 * shell lands nearby.
 *
 * Sounds: every sound effect in assets/sounds, with its whole waveform
 * shown and a playhead running across it. Files whose names end in _loop
 * repeat until stopped.
 */

#ifndef MUSIC_SCENE_HPP
#define MUSIC_SCENE_HPP

#include "../../include/scenes/SceneManager.hpp"
#include "../../include/gui/Button.hpp"
#include "../../include/gui/Slider.hpp"
#include "../../include/gui/TextBox.hpp"
#include "../../include/gui/WidgetManager.hpp"

#include "Audio.hpp"

#include <SFML/Audio/Sound.hpp>

namespace music
{

class MusicScene : public Scene
{
private:
    enum class Mode
    {
        MUSIC,
        SOUNDS
    };

    struct Control
    {
        std::shared_ptr<Slider>  slider;
        std::shared_ptr<TextBox> value;
        std::function<float(float)>        to_value;     // slider 0-1 -> setting
        std::function<float(float)>        to_slider;    // setting -> slider 0-1
        std::function<std::string(float)>  describe;
    };

    Mode m_mode{Mode::MUSIC};

    // ---------------------------------------------------------- the list
    std::vector<std::filesystem::path>   m_track_paths;
    std::vector<std::filesystem::path>   m_sound_paths;
    std::vector<std::shared_ptr<Button>> m_list;          // fixed pool of rows
    std::size_t m_scroll{0};
    std::shared_ptr<Button>  m_tab_music;
    std::shared_ptr<Button>  m_tab_sounds;
    std::shared_ptr<TextBox> m_more;
    std::shared_ptr<TextBox> m_empty;
    std::shared_ptr<TextBox> m_hint;

    std::vector<std::shared_ptr<Widget>> m_music_only;
    std::vector<std::shared_ptr<Widget>> m_sound_only;

    // ------------------------------------------------------------- music
    std::size_t m_current{0};

    Track                   m_track;
    Settings                m_settings;
    Processor               m_processor{m_settings};
    Ring                    m_ring;
    std::unique_ptr<Player> m_player;
    bool                    m_playing{false};

    WidgetManager m_widgets;
    Control m_volume, m_speed, m_bits, m_rate, m_cutoff;

    std::shared_ptr<Button>  m_play;
    std::shared_ptr<Button>  m_radio;
    std::shared_ptr<Button>  m_shock;
    std::shared_ptr<Slider>  m_progress;
    std::shared_ptr<TextBox> m_time;
    std::shared_ptr<TextBox> m_now_playing;
    bool m_seek_click{false};

    sf::Shader*               m_scope{nullptr};
    sf::Texture               m_samples;
    std::vector<std::uint8_t> m_pixels;
    std::vector<float>        m_window;
    sf::RenderTexture         m_scope_target;

    // Rendering ourselves when nothing is pulling audio, so the scope and
    // the transport still work on a machine without a sound device.
    std::uint64_t m_last_total{0};
    float         m_starved{0.0f};

    // ------------------------------------------------------------ sounds
    std::size_t              m_sound_index{static_cast<std::size_t>(-1)};
    const sf::SoundBuffer*   m_buffer{nullptr};
    std::unique_ptr<sf::Sound> m_sound;
    bool                     m_sound_playing{false};
    bool                     m_sound_loops{false};
    float                    m_sound_time{0.0f};       // our own clock, so the playhead
    float                    m_sound_length{0.0f};     // works without an audio device
    std::vector<sf::Vector2f> m_wave;                  // min/max per column, -1..1
    sf::VertexArray          m_wave_vertices{sf::PrimitiveType::Triangles};
    std::shared_ptr<Button>  m_sound_play;
    std::shared_ptr<TextBox> m_sound_info;

    void build_ui();
    Control make_control(const std::string& name, sf::Vector2f at,
                         std::function<float(float)> to_value,
                         std::function<float(float)> to_slider,
                         std::function<std::string(float)> describe);

    void set_mode(Mode mode);
    void refresh_list();
    void scroll_by(int rows);
    void select_row(std::size_t row);
    [[nodiscard]] const std::vector<std::filesystem::path>& items() const;

    void select_track(std::size_t index);
    void set_playing(bool playing);
    void set_radio(bool on);
    void apply_preset(float volume, float speed, float bits, float rate, float cutoff, bool radio);
    void refresh_buttons();
    void update_scope();

    void select_sound(std::size_t index);
    void play_sound();
    void stop_sound();
    void build_wave_vertices();

public:
    MusicScene();
    ~MusicScene() override;

    void on_enter() override;
    void begin_frame() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse) override;
    void update(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;

    [[nodiscard]] sf::Color get_clear_color() const override;
};

}

#endif
