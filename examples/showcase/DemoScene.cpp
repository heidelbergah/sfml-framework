/**
 * DemoScene.cpp  -  Showcase
 */

#include "DemoScene.hpp"

namespace showcase
{

DemoScene::DemoScene(const std::string& title, const std::string& hint)
{
    m_title = make_label({VIEW_W - 48.0f, 40.0f}, title, TEXT,
                         Alignment::LEFT, TITLE_SIZE);
    m_title->set_z_value(10);
    m_widgets.add_widget("title", m_title);

    m_hint = make_label({VIEW_W - 48.0f, 24.0f}, hint + "   esc  back", TEXT_DIM);
    m_hint->set_z_value(10);
    m_widgets.add_widget("hint", m_hint);
}

void DemoScene::on_enter()
{
    GameTime::resume();

    // The title drops in; the hint rises to meet it.
    m_title->set_transition(TransitionFunction::EaseOutBack);
    m_title->set_transition_duration(0.5f);
    m_title->set_position({24.0f, -40.0f}, true);
    m_title->set_position({24.0f, 16.0f});

    m_hint->set_transition(TransitionFunction::EaseOutQuad);
    m_hint->set_transition_duration(0.6f);
    m_hint->set_position({24.0f, VIEW_H + 8.0f}, true);
    m_hint->set_position({24.0f, VIEW_H - 34.0f});
}

void DemoScene::back_to_hub()
{
    if(m_manager) m_manager->request_scene_switch("hub", SceneTransitionType::FADE);
}

void DemoScene::begin_frame()
{
    m_widgets.begin_frame();
}

void DemoScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    m_widgets.handle_event(event, mouse_pos);

    if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        if(key->code == sf::Keyboard::Key::Escape) back_to_hub();
    }
}

void DemoScene::update(sf::Time delta_time)
{
    m_widgets.update(delta_time);
}

}
