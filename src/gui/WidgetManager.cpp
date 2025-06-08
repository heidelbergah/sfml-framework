/**
 * WidgetManager.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/gui/WidgetManager.hpp"

void WidgetManager::update_z_order()
{
    m_z_ordered_widgets.clear();
    for (const auto& [id, widget] : m_widgets)
        m_z_ordered_widgets.push_back(widget);

    std::sort(m_z_ordered_widgets.begin(), m_z_ordered_widgets.end(),
        [](const std::shared_ptr<Widget>& a, const std::shared_ptr<Widget>& b) {
            return a->get_z_value() < b->get_z_value();
        });
}

WidgetManager::WidgetManager() {}

void WidgetManager::add_widget(std::string key, std::shared_ptr<Widget> widget)
{
    m_widgets[key] = widget;
    update_z_order();
}

void WidgetManager::remove_widget(std::string key)
{
    m_widgets.erase(key);
    update_z_order();
}

std::vector<std::shared_ptr<Widget>> WidgetManager::get_widgets()
{
    return m_z_ordered_widgets;
}
