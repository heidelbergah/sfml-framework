/**
 * WidgetManager.hpp
 *
 * Written by James Bouchat
 */

#ifndef WIDGET_MANAGER_HPP
#define WIDGET_MANAGER_HPP

#include "../gui/Widget.hpp"

class WidgetManager
{
private:
    std::unordered_map<std::string, std::shared_ptr<Widget>> m_widgets;
    std::vector<std::shared_ptr<Widget>> m_z_ordered_widgets;
    
    void update_z_order();
public:
    WidgetManager();

    void add_widget(std::string key, std::shared_ptr<Widget> widget);
    void remove_widget(std::string key);

    template<typename T>
    std::shared_ptr<T> get_widget(std::string key);

    std::vector<std::shared_ptr<Widget>> get_widgets();
};

template<typename T>
std::shared_ptr<T> WidgetManager::get_widget(std::string key)
{
    auto it = m_widgets.find(key);
    if(it != m_widgets.end()) return std::dynamic_pointer_cast<T>(it->second);
    return nullptr;
}

#endif
