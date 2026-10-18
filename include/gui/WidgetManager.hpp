/**
 * WidgetManager.hpp
 *
 * Keyed collection of widgets, kept sorted by z value.
 */

#ifndef WIDGET_MANAGER_HPP
#define WIDGET_MANAGER_HPP

#include "Widget.hpp"

class WidgetManager : public sf::Drawable
{
private:
    std::unordered_map<std::string, std::shared_ptr<Widget>> m_widgets;
    std::vector<std::shared_ptr<Widget>>                     m_z_ordered_widgets;

    void update_z_order();

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

public:
    WidgetManager() = default;

    void add_widget(const std::string& key, std::shared_ptr<Widget> widget);
    void remove_widget(const std::string& key);
    void clear();

    [[nodiscard]] bool has_widget(const std::string& key) const;

    /*
     * Returns nullptr if the key is absent or the widget is not a T.
     * Never inserts -- the old version used map::operator[], so a
     * mistyped key silently created a null entry that was then
     * dereferenced.
     */
    template<typename T = Widget>
    [[nodiscard]] std::shared_ptr<T> get_widget(const std::string& key) const;

    /*
     * Z-ordered, back to front. Returned by reference -- copying meant
     * a heap allocation plus an atomic refcount bump per widget, three
     * times per frame.
     */
    [[nodiscard]] const std::vector<std::shared_ptr<Widget>>& get_widgets() const;

    /*
     * Re-sorts after a widget's z value changed.
     */
    void refresh_z_order();

    void begin_frame();
    void update(sf::Time delta_time);

    /*
     * Feeds the event to every widget, front to back. `mouse_pos` is in
     * the same space the widgets were positioned in.
     */
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos);

    [[nodiscard]] std::size_t size() const;
};

template<typename T>
std::shared_ptr<T> WidgetManager::get_widget(const std::string& key) const
{
    auto it = m_widgets.find(key);
    if(it == m_widgets.end()) return nullptr;
    return std::dynamic_pointer_cast<T>(it->second);
}

#endif
