/**
 * MusicScene.cpp  -  Music lab
 */

#include "MusicScene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

namespace music
{

namespace
{
    const sf::Color INK      {21, 19, 26};
    const sf::Color PANEL    {32, 29, 38};
    const sf::Color PANEL_HI {46, 42, 54};
    const sf::Color LINE     {68, 62, 78};
    const sf::Color TEXT     {214, 203, 182};
    const sf::Color TEXT_DIM {128, 118, 120};
    const sf::Color SAGE     {132, 180, 140};
    const sf::Color CORAL    {222, 106, 88};
    const sf::Color SCOPE_DIM{40, 62, 52};

    constexpr unsigned SIZE = 16;

    const sf::Vector2f SCOPE_POS {196.0f, 74.0f};
    const sf::Vector2u SCOPE_SIZE{740, 176};

    // The list on the left: a fixed pool of rows that scroll through the files.
    constexpr std::size_t ROWS     = 12;
    constexpr float       LIST_X   = 24.0f;
    constexpr float       LIST_Y   = 98.0f;
    constexpr float       ROW_STEP = 30.0f;
    constexpr float       LIST_W   = 150.0f;

    const sf::Font& font()
    {
        static const sf::Font* f = Assets::font("assets/fonts/pixel-font-bitmap.ttf");
        return *f;
    }

    std::shared_ptr<TextBox> label(sf::Vector2f size, const std::string& text, sf::Vector2f at,
                                   sf::Color color = TEXT_DIM, Alignment align = Alignment::LEFT,
                                   unsigned character_size = SIZE)
    {
        auto box = std::make_shared<TextBox>(size, sf::Color::Transparent, text, font(), color);
        box->set_character_size(character_size);
        box->set_alignment(align);
        box->set_transition(TransitionFunction::None);
        box->set_position(at, true);
        return box;
    }

    std::shared_ptr<Button> button(sf::Vector2f size, const std::string& text, sf::Vector2f at,
                                   sf::Color color = PANEL_HI)
    {
        auto b = std::make_shared<Button>(size, color);
        b->add_text(text, font(), TEXT);
        b->set_character_size(SIZE);
        b->set_state_tints(-16, 12);
        b->set_transition(TransitionFunction::None);
        b->set_position(at, true);
        return b;
    }

    void recolor(Button& b, sf::Color color)
    {
        b.set_color(color);
        b.set_state_tints(-16, 12);
    }

    // Logarithmic mapping, because frequencies are heard that way: the
    // middle of the slider should be the middle of what you hear.
    float log_map(float t, float lo, float hi)   { return lo * std::pow(hi / lo, t); }
    float log_unmap(float v, float lo, float hi) { return std::log(v / lo) / std::log(hi / lo); }

    std::string hz(float v)
    {
        char buffer[16];
        if(v >= 1000.0f) std::snprintf(buffer, sizeof(buffer), "%.1fk", static_cast<double>(v / 1000.0f));
        else             std::snprintf(buffer, sizeof(buffer), "%.0f", static_cast<double>(v));
        return buffer;
    }

    std::string clock(double seconds)
    {
        int const s = static_cast<int>(seconds);
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "%d:%02d", s / 60, s % 60);
        return buffer;
    }

    std::string seconds_text(float s)
    {
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "%.2fs", static_cast<double>(s));
        return buffer;
    }

    std::vector<std::filesystem::path> audio_files(const std::string& folder)
    {
        std::vector<std::filesystem::path> found;
        std::error_code error;
        for(const auto& entry : std::filesystem::directory_iterator(folder, error))
        {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if(ext == ".mp3" || ext == ".ogg" || ext == ".wav" || ext == ".flac")
                found.push_back(entry.path());
        }
        std::sort(found.begin(), found.end());
        return found;
    }

    bool is_loop(const std::filesystem::path& p)
    {
        std::string const stem = p.stem().string();
        return stem.size() >= 5 && stem.compare(stem.size() - 5, 5, "_loop") == 0;
    }
}

MusicScene::MusicScene()
{
    m_track_paths = audio_files("assets/music");
    m_sound_paths = audio_files("assets/sounds");

    (void)m_scope_target.resize(SCOPE_SIZE);
    (void)m_samples.resize({SCOPE_SIZE.x, 1});
    m_pixels.assign(SCOPE_SIZE.x * 4, 255);

    build_ui();
    set_mode(Mode::MUSIC);
}

MusicScene::~MusicScene()
{
    if(m_sound) m_sound->stop();
    m_player.reset();
}

MusicScene::Control MusicScene::make_control(const std::string& name, sf::Vector2f at,
                                             std::function<float(float)> to_value,
                                             std::function<float(float)> to_slider,
                                             std::function<std::string(float)> describe)
{
    Control c;
    c.to_value  = std::move(to_value);
    c.to_slider = std::move(to_slider);
    c.describe  = std::move(describe);

    m_widgets.add_widget("label_" + name, label({80.0f, 24.0f}, name, at));

    c.slider = std::make_shared<Slider>(sf::Vector2f{170.0f, 12.0f}, SAGE);
    c.slider->set_body_color(LINE);
    c.slider->set_transition(TransitionFunction::None);
    c.slider->set_position(at + sf::Vector2f{84.0f, 6.0f}, true);
    m_widgets.add_widget("slider_" + name, c.slider);

    c.value = label({70.0f, 24.0f}, "", at + sf::Vector2f{260.0f, 0.0f}, TEXT, Alignment::RIGHT);
    m_widgets.add_widget("value_" + name, c.value);
    return c;
}

void MusicScene::build_ui()
{
    m_widgets.add_widget("title", label({400.0f, 40.0f}, "Music lab", {24.0f, 16.0f}, TEXT,
                                        Alignment::LEFT, 32));

    m_now_playing = label({500.0f, 40.0f}, "", {436.0f, 16.0f}, TEXT_DIM, Alignment::RIGHT);
    m_widgets.add_widget("now_playing", m_now_playing);

    // -------------------------------------------------------- tabs, list
    m_tab_music = button({72.0f, 22.0f}, "music", {LIST_X, 70.0f});
    m_tab_music->set_callback([this]() { set_mode(Mode::MUSIC); });
    m_widgets.add_widget("tab_music", m_tab_music);

    m_tab_sounds = button({72.0f, 22.0f}, "sounds", {LIST_X + 78.0f, 70.0f});
    m_tab_sounds->set_callback([this]() { set_mode(Mode::SOUNDS); });
    m_widgets.add_widget("tab_sounds", m_tab_sounds);

    for(std::size_t i = 0; i < ROWS; ++i)
    {
        auto row = button({LIST_W, 24.0f}, "", {LIST_X, LIST_Y + ROW_STEP * static_cast<float>(i)});
        row->set_callback([this, i]() { select_row(i); });
        m_widgets.add_widget("row_" + std::to_string(i), row);
        m_list.push_back(row);
    }

    m_more = label({LIST_W, 20.0f}, "", {LIST_X, LIST_Y + ROW_STEP * ROWS - 2.0f}, TEXT_DIM,
                   Alignment::CENTER);
    m_widgets.add_widget("more", m_more);

    m_empty = label({LIST_W, 110.0f}, "", {LIST_X, LIST_Y}, TEXT);
    m_widgets.add_widget("empty", m_empty);

    // Everything added from here to the sounds section only belongs to
    // the music tab. Remember what exists now, so the difference can be
    // collected afterwards rather than tagged one widget at a time.
    std::set<Widget*> before;
    for(const auto& w : m_widgets.get_widgets()) before.insert(w.get());

    // --------------------------------------------------------- transport
    m_play = button({64.0f, 24.0f}, "play", {SCOPE_POS.x, 262.0f});
    m_play->set_callback([this]() { set_playing(!m_playing); });
    m_widgets.add_widget("play", m_play);

    m_progress = std::make_shared<Slider>(sf::Vector2f{560.0f, 12.0f}, TEXT);
    m_progress->set_body_color(LINE);
    m_progress->set_transition(TransitionFunction::None);
    m_progress->set_position({272.0f, 268.0f}, true);
    m_progress->set_percentage(0.0f);
    m_widgets.add_widget("progress", m_progress);

    m_time = label({100.0f, 24.0f}, "0:00 / 0:00", {836.0f, 262.0f}, TEXT, Alignment::RIGHT);
    m_widgets.add_widget("time", m_time);

    // ----------------------------------------------------------- effects
    m_widgets.add_widget("effects_label", label({200.0f, 20.0f}, "effects", {SCOPE_POS.x, 302.0f}));

    float const left = SCOPE_POS.x, right = 606.0f;

    m_volume = make_control("volume", {left, 326.0f},
        [](float t) { return t; }, [](float v) { return v; },
        [](float v) { return std::to_string(static_cast<int>(std::round(v * 100.0f))) + "%"; });

    m_speed = make_control("speed", {left, 354.0f},
        [](float t) { return 0.5f + t; }, [](float v) { return v - 0.5f; },
        [](float v) { char b[16]; std::snprintf(b, sizeof(b), "%.2fx", static_cast<double>(v)); return std::string(b); });

    m_bits = make_control("bits", {left, 382.0f},
        [](float t) { return std::round(2.0f + t * 14.0f); }, [](float v) { return (v - 2.0f) / 14.0f; },
        [](float v) { return std::to_string(static_cast<int>(v)) + " bit"; });

    m_rate = make_control("rate", {right, 326.0f},
        [](float t) { return log_map(t, 2000.0f, 44100.0f); },
        [](float v) { return log_unmap(v, 2000.0f, 44100.0f); },
        [](float v) { return hz(v) + "Hz"; });

    m_cutoff = make_control("low-pass", {right, 354.0f},
        [](float t) { return log_map(t, 150.0f, 20000.0f); },
        [](float v) { return log_unmap(v, 150.0f, 20000.0f); },
        [](float v) { return v >= 19900.0f ? std::string("off") : hz(v) + "Hz"; });

    m_widgets.add_widget("radio_label", label({80.0f, 24.0f}, "radio", {right, 382.0f}));
    m_radio = button({70.0f, 24.0f}, "off", {right + 84.0f, 382.0f});
    m_radio->set_callback([this]() { set_radio(!m_settings.radio); });
    m_widgets.add_widget("radio", m_radio);

    // ----------------------------------------------------------- presets
    m_widgets.add_widget("presets_label", label({200.0f, 20.0f}, "try it as", {SCOPE_POS.x, 420.0f}));

    struct Preset { const char* name; float width; float volume, speed, bits, rate, cutoff; bool radio; };
    Preset const presets[] = {
        {"clean",       70.0f,  0.8f, 1.0f,  16.0f, 44100.0f, 20000.0f, false},
        {"tank hull",   100.0f, 0.8f, 1.0f,  16.0f, 44100.0f,   900.0f, false},
        {"field radio", 116.0f, 0.8f, 1.0f,  16.0f, 44100.0f, 20000.0f, true},
        {"late war",    92.0f,  0.8f, 0.94f,  6.0f, 11025.0f,  6000.0f, false},
    };

    float x = SCOPE_POS.x;
    for(const Preset& p : presets)
    {
        auto b = button({p.width, 24.0f}, p.name, {x, 444.0f});
        b->set_callback([this, p]() { apply_preset(p.volume, p.speed, p.bits, p.rate, p.cutoff, p.radio); });
        m_widgets.add_widget(std::string("preset_") + p.name, b);
        x += p.width + 8.0f;
    }

    m_shock = button({120.0f, 24.0f}, "shell shock", {936.0f - 120.0f, 444.0f}, sf::Color(120, 58, 50));
    m_shock->set_callback([this]() { m_processor.shell_shock(); });
    m_widgets.add_widget("shock", m_shock);

    for(const auto& w : m_widgets.get_widgets())
        if(!before.count(w.get())) m_music_only.push_back(w);

    // ------------------------------------------------------------ sounds
    m_sound_play = button({64.0f, 24.0f}, "play", {SCOPE_POS.x, 262.0f});
    m_sound_play->set_callback([this]() { m_sound_playing ? stop_sound() : play_sound(); });
    m_widgets.add_widget("sound_play", m_sound_play);
    m_sound_only.push_back(m_sound_play);

    m_sound_info = label({664.0f, 24.0f}, "", {272.0f, 262.0f}, TEXT);
    m_widgets.add_widget("sound_info", m_sound_info);
    m_sound_only.push_back(m_sound_info);

    auto about = label({740.0f, 140.0f},
        "Click a sound to hear it. Names ending in _loop repeat until\n"
        "you stop them, and are built to loop without a click.\n\n"
        "These were generated by tools/sfx/generate_sfx.py. Change a\n"
        "number or a seed there and run it again for a new variation.",
        {SCOPE_POS.x, 310.0f});
    m_widgets.add_widget("sound_about", about);
    m_sound_only.push_back(about);

    // --------------------------------------------------------------- hint
    m_hint = label({912.0f, 24.0f}, "", {24.0f, 504.0f});
    m_widgets.add_widget("hint", m_hint);

    apply_preset(0.8f, 1.0f, 16.0f, 44100.0f, 20000.0f, false);
}

// ================================================================== list

const std::vector<std::filesystem::path>& MusicScene::items() const
{
    return m_mode == Mode::MUSIC ? m_track_paths : m_sound_paths;
}

void MusicScene::set_mode(Mode mode)
{
    m_mode   = mode;
    m_scroll = 0;

    bool const music = (mode == Mode::MUSIC);
    if(music) stop_sound();
    else      set_playing(false);

    for(const auto& w : m_music_only) w->set_visible(music);
    for(const auto& w : m_sound_only) w->set_visible(!music);

    recolor(*m_tab_music,  music ? LINE : PANEL_HI);
    recolor(*m_tab_sounds, music ? PANEL_HI : LINE);
    m_tab_music->set_text_color(music ? TEXT : TEXT_DIM);
    m_tab_sounds->set_text_color(music ? TEXT_DIM : TEXT);

    m_hint->set_string(music
        ? "space  play / pause     1-9  tracks     B  shell shock     R  radio     wheel  scroll     esc  quit"
        : "space  play / stop     1-9  sounds     wheel  scroll the list     esc  quit");

    if(music) m_now_playing->set_string(m_track.frames() ? m_track.name : "");
    else      m_now_playing->set_string(m_buffer ? m_sound_paths[m_sound_index].stem().string() : "");

    refresh_list();
}

void MusicScene::refresh_list()
{
    const auto& files = items();

    for(std::size_t i = 0; i < m_list.size(); ++i)
    {
        std::size_t const index = m_scroll + i;
        Button& row = *m_list[i];

        if(index >= files.size())
        {
            row.set_visible(false);
            continue;
        }

        bool const active = (m_mode == Mode::MUSIC)
                          ? (index == m_current && m_track.frames() > 0)
                          : (index == m_sound_index);

        row.set_visible(true);
        row.add_text(files[index].stem().string(), font(), active ? TEXT : TEXT_DIM);
        recolor(row, active ? LINE : PANEL_HI);
    }

    std::size_t const below = files.size() > m_scroll + ROWS ? files.size() - m_scroll - ROWS : 0;
    if(below > 0)          m_more->set_string(std::to_string(below) + " more below");
    else if(m_scroll > 0)  m_more->set_string(std::to_string(m_scroll) + " more above");
    else                   m_more->set_string("");

    if(files.empty())
    {
        m_empty->set_string(m_mode == Mode::MUSIC
            ? "no tracks yet.\nput .mp3, .ogg,\n.wav or .flac\nfiles in\nassets/music"
            : "no sounds yet.\nrun\ntools/sfx/\ngenerate_sfx.py");
    }
    else
    {
        m_empty->set_string("");
    }
}

void MusicScene::scroll_by(int rows)
{
    long const most = std::max<long>(0, static_cast<long>(items().size()) - static_cast<long>(ROWS));
    long const next = std::clamp<long>(static_cast<long>(m_scroll) + rows, 0, most);
    if(next == static_cast<long>(m_scroll)) return;
    m_scroll = static_cast<std::size_t>(next);
    refresh_list();
}

void MusicScene::select_row(std::size_t row)
{
    std::size_t const index = m_scroll + row;
    if(index >= items().size()) return;

    if(m_mode == Mode::MUSIC) select_track(index);
    else                      select_sound(index);
}

// ================================================================= music

void MusicScene::on_enter()
{
    if(!m_scope && m_manager)
        m_scope = m_manager->get_shaders().load_fragment("music_scope", "shaders/music/scope.frag");

    if(!m_track_paths.empty() && m_track.frames() == 0) select_track(0);
}

void MusicScene::select_track(std::size_t index)
{
    if(index >= m_track_paths.size()) return;

    bool const was_playing = m_playing || m_track.frames() == 0;

    // The player reads the processor, which reads the track, so stop the
    // stream before the track it is reading is replaced.
    m_player.reset();
    m_processor.set_track(nullptr);

    m_current = index;
    if(!m_track.load(m_track_paths[index]))
    {
        m_now_playing->set_string("could not read " + m_track_paths[index].filename().string());
        m_track = Track{};
        refresh_buttons();
        return;
    }

    m_processor.set_track(&m_track);
    m_ring.clear();
    m_player = std::make_unique<Player>(m_processor, m_ring, m_track.sample_rate);
    m_now_playing->set_string(m_track.name);

    set_playing(was_playing);
    refresh_buttons();
}

void MusicScene::set_playing(bool playing)
{
    m_playing = playing && m_player;
    if(m_player)
    {
        if(m_playing) m_player->play();
        else          m_player->pause();
    }
    m_starved = 0.0f;
    refresh_buttons();
}

void MusicScene::set_radio(bool on)
{
    m_settings.radio = on;
    refresh_buttons();
}

void MusicScene::apply_preset(float volume, float speed, float bits, float rate, float cutoff, bool radio)
{
    m_volume.slider->set_percentage(m_volume.to_slider(volume));
    m_speed.slider->set_percentage(m_speed.to_slider(speed));
    m_bits.slider->set_percentage(m_bits.to_slider(bits));
    m_rate.slider->set_percentage(m_rate.to_slider(rate));
    m_cutoff.slider->set_percentage(m_cutoff.to_slider(cutoff));
    set_radio(radio);
}

void MusicScene::refresh_buttons()
{
    if(!m_play) return;   // called during construction, before the transport exists

    refresh_list();

    m_play->add_text(m_playing ? "pause" : "play", font(), TEXT);

    bool const radio = m_settings.radio;
    m_radio->add_text(radio ? "on" : "off", font(), radio ? TEXT : TEXT_DIM);
    recolor(*m_radio, radio ? LINE : PANEL_HI);
}

// ================================================================ sounds

void MusicScene::select_sound(std::size_t index)
{
    if(index >= m_sound_paths.size()) return;

    stop_sound();
    m_sound.reset();

    m_sound_index = index;
    m_buffer      = Assets::sounds().try_get(m_sound_paths[index].string());
    m_sound_loops = is_loop(m_sound_paths[index]);
    m_wave.clear();

    if(!m_buffer || m_buffer->getSampleCount() == 0)
    {
        m_sound_info->set_string("could not read " + m_sound_paths[index].filename().string());
        m_buffer = nullptr;
        refresh_list();
        return;
    }

    unsigned const    channels = std::max(1u, m_buffer->getChannelCount());
    std::size_t const frames   = static_cast<std::size_t>(m_buffer->getSampleCount()) / channels;
    m_sound_length = static_cast<float>(frames) / static_cast<float>(m_buffer->getSampleRate());

    // The whole sound, as the loudest and quietest sample in each column.
    const std::int16_t* samples = m_buffer->getSamples();
    m_wave.resize(SCOPE_SIZE.x);
    for(std::size_t x = 0; x < SCOPE_SIZE.x; ++x)
    {
        std::size_t const from = frames * x / SCOPE_SIZE.x;
        std::size_t const to   = std::max(from + 1, frames * (x + 1) / SCOPE_SIZE.x);
        float lo = 0.0f, hi = 0.0f;
        for(std::size_t f = from; f < to && f < frames; ++f)
        {
            float const s = samples[f * channels] / 32768.0f;
            lo = std::min(lo, s);
            hi = std::max(hi, s);
        }
        m_wave[x] = {lo, hi};
    }

    m_sound = std::make_unique<sf::Sound>(*m_buffer);
    m_sound->setLooping(m_sound_loops);

    m_sound_info->set_string(m_sound_paths[index].stem().string() + "    " +
                             seconds_text(m_sound_length) +
                             (m_sound_loops ? "    loops" : "    one shot"));
    m_now_playing->set_string(m_sound_paths[index].stem().string());

    play_sound();
    refresh_list();
}

void MusicScene::play_sound()
{
    if(!m_sound) return;
    m_sound->stop();
    m_sound->play();
    m_sound_time    = 0.0f;
    m_sound_playing = true;
    m_sound_play->add_text("stop", font(), TEXT);
}

void MusicScene::stop_sound()
{
    if(m_sound) m_sound->stop();
    m_sound_playing = false;
    if(m_sound_play) m_sound_play->add_text("play", font(), TEXT);
}

void MusicScene::build_wave_vertices()
{
    m_wave_vertices.clear();
    if(m_wave.empty()) return;

    float const mid   = SCOPE_POS.y + static_cast<float>(SCOPE_SIZE.y) / 2.0f;
    float const scale = static_cast<float>(SCOPE_SIZE.y) * 0.45f;

    float const played = m_sound_length > 0.0f ? m_sound_time / m_sound_length : 0.0f;
    std::size_t const head = static_cast<std::size_t>(played * static_cast<float>(m_wave.size()));

    auto rect = [this](float x, float y0, float y1, sf::Color c)
    {
        sf::Vector2f const a{x, y0}, b{x + 1.0f, y0}, cc{x + 1.0f, y1}, d{x, y1};
        for(sf::Vector2f p : {a, b, cc, a, cc, d}) m_wave_vertices.append({p, c});
    };

    for(std::size_t x = 0; x < m_wave.size(); ++x)
    {
        float const top    = std::floor(mid - m_wave[x].y * scale);
        float const bottom = std::ceil(mid - m_wave[x].x * scale) + 1.0f;
        bool const  done   = m_sound_playing && x < head;
        rect(SCOPE_POS.x + static_cast<float>(x), top, bottom, done ? TEXT : SAGE);
    }

    if(m_sound_playing)
        rect(SCOPE_POS.x + static_cast<float>(std::min(head, m_wave.size() - 1)),
             SCOPE_POS.y, SCOPE_POS.y + static_cast<float>(SCOPE_SIZE.y), CORAL);
}

// ================================================================ events

void MusicScene::begin_frame()
{
    m_widgets.begin_frame();
}

void MusicScene::handle_event(const sf::Event& event, sf::Vector2f mouse)
{
    m_widgets.handle_event(event, mouse);

    if(const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if(pressed->button == sf::Mouse::Button::Left && m_progress->is_visible() &&
           m_progress->contains(mouse))
            m_seek_click = true;
    }

    if(const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>())
    {
        if(mouse.x < LIST_X + LIST_W + 16.0f) scroll_by(wheel->delta > 0.0f ? -1 : 1);
    }

    const auto* key = event.getIf<sf::Event::KeyPressed>();
    if(!key) return;

    bool const music = (m_mode == Mode::MUSIC);

    switch(key->code)
    {
        case sf::Keyboard::Key::Escape: EventBus::publish(Event(EventType::QUIT)); break;
        case sf::Keyboard::Key::Space:
            if(music) set_playing(!m_playing);
            else      m_sound_playing ? stop_sound() : play_sound();
            break;
        case sf::Keyboard::Key::B: if(music) m_processor.shell_shock(); break;
        case sf::Keyboard::Key::R: if(music) set_radio(!m_settings.radio); break;
        case sf::Keyboard::Key::Down: scroll_by(1); break;
        case sf::Keyboard::Key::Up:   scroll_by(-1); break;
        default:
        {
            int const n = static_cast<int>(key->code) - static_cast<int>(sf::Keyboard::Key::Num1);
            if(n >= 0 && n < 9) select_row(static_cast<std::size_t>(n));
        }
    }
}

// ================================================================ update

void MusicScene::update(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();
    m_widgets.update(delta_time);

    if(m_mode == Mode::SOUNDS)
    {
        if(m_sound_playing)
        {
            m_sound_time += dt;
            if(m_sound_time >= m_sound_length)
            {
                if(m_sound_loops) m_sound_time = std::fmod(m_sound_time, std::max(m_sound_length, 0.001f));
                else              stop_sound();
            }
        }
        build_wave_vertices();
        return;
    }

    // Sliders -> settings, every frame. Cheap, and it means presets,
    // keyboard and mouse all go through one path.
    auto read = [](Control& c, std::atomic<float>& setting)
    {
        float const v = c.to_value(c.slider->get_percentage());
        setting = v;
        c.value->set_string(c.describe(v));
    };
    read(m_volume, m_settings.volume);
    read(m_speed,  m_settings.speed);
    read(m_bits,   m_settings.bits);
    read(m_rate,   m_settings.sample_rate);
    read(m_cutoff, m_settings.cutoff);

    if(m_track.frames() == 0) return;

    // No audio device pulling the stream: render here instead, at the
    // same rate it would have played, so everything still moves.
    std::uint64_t const total = m_ring.total();
    m_starved    = (m_playing && total == m_last_total) ? m_starved + dt : 0.0f;
    m_last_total = total;
    if(m_playing && m_starved > 0.25f)
    {
        std::size_t const frames = static_cast<std::size_t>(dt * static_cast<float>(m_track.sample_rate));
        std::vector<std::int16_t> local(frames * 2);
        m_processor.render(local.data(), frames);
        m_ring.push_stereo(local.data(), frames);
        m_last_total = m_ring.total();
    }

    // Transport. A click or drag on the bar seeks; otherwise the bar
    // follows the music.
    float const length = m_track.seconds();
    if(m_progress->value_changed() && (m_seek_click || m_progress->is_being_dragged()))
    {
        m_processor.seek(m_progress->get_percentage() * length);
    }
    else if(!m_progress->is_being_dragged())
    {
        m_progress->set_percentage(static_cast<float>(m_processor.position_seconds()) / length);
    }
    m_seek_click = false;

    m_time->set_string(clock(m_processor.position_seconds()) + " / " + clock(length));

    // The shell shock button glows while the shock is wearing off.
    float const shock = m_processor.shock_level();
    sf::Color const calm{120, 58, 50};
    sf::Color const hot = CORAL;
    auto lerp = [shock](std::uint8_t a, std::uint8_t b)
    {
        return static_cast<std::uint8_t>(a + (b - a) * shock);
    };
    recolor(*m_shock, {lerp(calm.r, hot.r), lerp(calm.g, hot.g), lerp(calm.b, hot.b)});

    update_scope();
}

void MusicScene::update_scope()
{
    // Trigger on a rising zero crossing, as a hardware scope does, so a
    // steady tone holds still instead of scrolling.
    m_ring.latest(m_window, 4096);
    std::size_t const width = SCOPE_SIZE.x;
    std::size_t start = m_window.size() - width;
    for(std::size_t i = m_window.size() - width - 1; i > m_window.size() - width - 1400; --i)
    {
        if(m_window[i - 1] < 0.0f && m_window[i] >= 0.0f)
        {
            start = i;
            break;
        }
    }

    for(std::size_t x = 0; x < width; ++x)
    {
        float const s = m_window[start + x];
        m_pixels[x * 4] = static_cast<std::uint8_t>(std::clamp(s * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f);
    }
    m_samples.update(m_pixels.data());
}

// ================================================================ render

void MusicScene::render(sf::RenderTarget& target)
{
    target.setView(target.getDefaultView());

    // Panel, with a one-pixel frame.
    sf::RectangleShape frame(sf::Vector2f(SCOPE_SIZE) + sf::Vector2f{2.0f, 2.0f});
    frame.setPosition(SCOPE_POS - sf::Vector2f{1.0f, 1.0f});
    frame.setFillColor(LINE);
    target.draw(frame);

    if(m_mode == Mode::SOUNDS)
    {
        sf::RectangleShape back{sf::Vector2f(SCOPE_SIZE)};
        back.setPosition(SCOPE_POS);
        back.setFillColor(INK);
        target.draw(back);

        sf::RectangleShape centre({static_cast<float>(SCOPE_SIZE.x), 1.0f});
        centre.setPosition({SCOPE_POS.x, SCOPE_POS.y + static_cast<float>(SCOPE_SIZE.y) / 2.0f});
        centre.setFillColor(SCOPE_DIM);
        target.draw(centre);

        if(m_wave_vertices.getVertexCount() > 0) target.draw(m_wave_vertices);
    }
    else if(m_scope)
    {
        m_scope->setUniform("resolution", sf::Glsl::Vec2{float(SCOPE_SIZE.x), float(SCOPE_SIZE.y)});
        m_scope->setUniform("samples", m_samples);
        // Fill the height whatever the track's mastering level.
        m_scope->setUniform("gain", 0.9f / std::max(0.05f, m_track.peak));
        m_scope->setUniform("c0", sf::Glsl::Vec4(INK));
        m_scope->setUniform("c1", sf::Glsl::Vec4(SCOPE_DIM));
        m_scope->setUniform("c2", sf::Glsl::Vec4(SAGE));
        m_scope->setUniform("c3", sf::Glsl::Vec4(TEXT));

        float const w = float(SCOPE_SIZE.x), h = float(SCOPE_SIZE.y);
        sf::Vertex const quad[] = {
            {{0, 0}, sf::Color::White, {0, 0}}, {{w, 0}, sf::Color::White, {1, 0}},
            {{0, h}, sf::Color::White, {0, 1}}, {{w, h}, sf::Color::White, {1, 1}},
        };
        m_scope_target.clear(INK);
        sf::RenderStates states;
        states.shader = m_scope;
        m_scope_target.draw(quad, 4, sf::PrimitiveType::TriangleStrip, states);
        m_scope_target.display();

        sf::Sprite sprite(m_scope_target.getTexture());
        sprite.setPosition(SCOPE_POS);
        target.draw(sprite);
    }

    target.draw(m_widgets);
}

sf::Color MusicScene::get_clear_color() const
{
    return INK;
}

}
