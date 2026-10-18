/**
 * WidgetManager.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/WidgetManager.hpp"

void WidgetManager::update_z_order()
{
    m_z_ordered_widgets.clear();
    m_z_ordered_widgets.reserve(m_widgets.size());

    for(const auto& [key, widget] : m_widgets)
        m_z_ordered_widgets.push_back(widget);

    // stable_sort so widgets sharing a z value keep a consistent
    // relative order between frames instead of flickering as the
    // unordered_map rehashes.
    std::stable_sort(m_z_ordered_widgets.begin(), m_z_ordered_widgets.end(),
        [](const std::shared_ptr<Widget>& a, const std::shared_ptr<Widget>& b)
        {
            return a->get_z_value() < b->get_z_value();
        });
}

void WidgetManager::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    for(const auto& widget : m_z_ordered_widgets)
        target.draw(*widget, states);
}

void WidgetManager::add_widget(const std::string& key, std::shared_ptr<Widget> widget)
{
    if(!widget) return;

    m_widgets[key] = std::move(widget);
    update_z_order();
}

void WidgetManager::remove_widget(const std::string& key)
{
    if(m_widgets.erase(key) > 0) update_z_order();
}

void WidgetManager::clear()
{
    m_widgets.clear();
    m_z_ordered_widgets.clear();
}

bool WidgetManager::has_widget(const std::string& key) const
{
    return m_widgets.find(key) != m_widgets.end();
}

const std::vector<std::shared_ptr<Widget>>& WidgetManager::get_widgets() const
{
    return m_z_ordered_widgets;
}

void WidgetManager::refresh_z_order()
{
    update_z_order();
}

void WidgetManager::bring_to_front(const std::string& key)
{
    auto it = m_widgets.find(key);
    if(it != m_widgets.end()) bring_to_front(it->second);
}

void WidgetManager::bring_to_front(const std::shared_ptr<Widget>& widget)
{
    if(!widget || m_z_ordered_widgets.empty()) return;

    // Already in front: leave z values alone rather than creeping them
    // upward on every click.
    if(m_z_ordered_widgets.back() == widget) return;

    widget->set_z_value(m_z_ordered_widgets.back()->get_z_value() + 1);
    update_z_order();
}

std::shared_ptr<Widget> WidgetManager::widget_at(sf::Vector2f point) const
{
    for(auto it = m_z_ordered_widgets.rbegin(); it != m_z_ordered_widgets.rend(); ++it)
    {
        if((*it)->is_visible() && (*it)->contains(point)) return *it;
    }
    return nullptr;
}

void WidgetManager::begin_frame()
{
    for(const auto& widget : m_z_ordered_widgets)
        widget->begin_frame();
}

void WidgetManager::update(sf::Time delta_time)
{
    for(const auto& widget : m_z_ordered_widgets)
        widget->update(delta_time);
}

void WidgetManager::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    // Front to back: the topmost widget sees the event first.
    for(auto it = m_z_ordered_widgets.rbegin(); it != m_z_ordered_widgets.rend(); ++it)
        (*it)->handle_event(event, mouse_pos);
}

std::size_t WidgetManager::size() const
{
    return m_widgets.size();
}

