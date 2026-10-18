/**
 * HubScene.cpp  -  Showcase
 */

#include "HubScene.hpp"

namespace showcase
{

namespace
{
    constexpr float ROW_X      = 190.0f;   // left edge of the marker column
    constexpr float ROW_Y      = 176.0f;
    constexpr float ROW_STEP   = 54.0f;
    constexpr float BUTTON_W   = 240.0f;
    constexpr float BUTTON_H   = 36.0f;
    constexpr float MARKER     = 8.0f;
    constexpr float GAP        = 16.0f;
}

HubScene::HubScene()
{
    m_title = make_label({VIEW_W, 40.0f}, "sfml-framework", TEXT,
                         Alignment::CENTER, TITLE_SIZE);
    m_widgets.add_widget("title", m_title);

    m_subtitle = make_label({VIEW_W, 24.0f}, "five small demos", TEXT_DIM,
                            Alignment::CENTER);
    m_widgets.add_widget("subtitle", m_subtitle);

    m_footer = make_label({VIEW_W, 24.0f}, "1-5  open      esc  quit", TEXT_DIM,
                          Alignment::CENTER);
    m_widgets.add_widget("footer", m_footer);

    struct Row { const char* key; const char* name; const char* blurb; sf::Color accent; };
    Row const rows[] = {
        {"easing",    "Easing playground", "eleven curves, retargeted mid-flight",    AMBER},
        {"particles", "Particle sandbox",  "right click anywhere for the settings",   CORAL},
        {"shake",     "Shake lab",         "trauma that stacks, a HUD that can join", SKY},
        {"names",     "Name forge",        "markov chains trained on lists of names", SAGE},
        {"shaders",   "Shader windows",    "five effects, each in a window of its own", ROSE},
    };

    for(const Row& row : rows)
    {
        Entry entry;
        entry.scene_key = row.key;
        entry.accent    = row.accent;

        entry.button = make_button({BUTTON_W, BUTTON_H}, row.name);
        std::string const key = row.key;
        entry.button->set_callback([this, key]() { open(key); });

        entry.description = make_label({320.0f, BUTTON_H}, row.blurb, TEXT_DIM);

        m_widgets.add_widget(std::string(row.key) + "_button", entry.button);
        m_widgets.add_widget(std::string(row.key) + "_blurb",  entry.description);
        m_entries.push_back(entry);
    }
}

void HubScene::open(const std::string& scene_key)
{
    if(m_manager) m_manager->request_scene_switch(scene_key, SceneTransitionType::FADE);
}

void HubScene::on_enter()
{
    GameTime::resume();

    m_title->set_position({0.0f, 72.0f}, true);
    m_subtitle->set_position({0.0f, 116.0f}, true);
    m_footer->set_position({0.0f, VIEW_H - 46.0f}, true);

    // Rows slide in from the left on a stagger, the same Interpolated
    // every widget position is built on, just with longer durations
    // further down.
    for(std::size_t i = 0; i < m_entries.size(); ++i)
    {
        Entry& entry  = m_entries[i];
        float const y = ROW_Y + ROW_STEP * static_cast<float>(i);
        float const x = ROW_X + MARKER + GAP;

        entry.button->set_transition(TransitionFunction::EaseOutBack);
        entry.button->set_transition_duration(0.45f + 0.09f * static_cast<float>(i));
        entry.button->set_position({x - 320.0f, y}, true);
        entry.button->set_position({x, y});

        entry.description->set_position({x + BUTTON_W + GAP, y}, true);
        entry.description->add_text_scroll(70.0f);
    }
}

void HubScene::begin_frame()
{
    m_widgets.begin_frame();
}

void HubScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    m_widgets.handle_event(event, mouse_pos);

    const auto* key = event.getIf<sf::Event::KeyPressed>();
    if(!key) return;

    if(key->code == sf::Keyboard::Key::Escape)
    {
        EventBus::publish(Event(EventType::QUIT));
        return;
    }

    int const index = static_cast<int>(key->code) - static_cast<int>(sf::Keyboard::Key::Num1);
    if(index >= 0 && index < static_cast<int>(m_entries.size()))
        open(m_entries[static_cast<std::size_t>(index)].scene_key);
}

void HubScene::update(sf::Time delta_time)
{
    m_widgets.update(delta_time);

    // A small square in each demo's colour rides along with its button,
    // and grows a little while the button is hovered.
    m_markers.clear();
    for(const Entry& entry : m_entries)
    {
        sf::Vector2f const pos  = entry.button->get_position();
        float const        size = entry.button->is_hovered() ? MARKER + 4.0f : MARKER;
        float const        x    = pos.x - GAP - size / 2.0f - MARKER / 2.0f;
        float const        y    = pos.y + (BUTTON_H - size) / 2.0f;

        push_rect(m_markers, {floor_vector({x, y}), {size, size}}, entry.accent);
    }
}

void HubScene::render(sf::RenderTarget& target)
{
    target.setView(target.getDefaultView());
    target.draw(m_markers);
    target.draw(m_widgets);
}

}
