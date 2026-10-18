/**
 * EasingScene.hpp  -  Showcase
 *
 * Every easing curve on its own lane, all chasing the same target.
 * Click to retarget them mid-flight; each one restarts from
 * wherever it currently is, which is what Interpolated guarantees.
 */

#ifndef SHOWCASE_EASING_SCENE_HPP
#define SHOWCASE_EASING_SCENE_HPP

#include "../../include/gui/Slider.hpp"

#include "DemoScene.hpp"

#include <deque>

namespace showcase
{

class EasingScene : public DemoScene
{
private:
    struct Lane
    {
        TransitionFunction  curve;
        sf::Color           color;
        Interpolated<float> x;
        std::deque<float>   trail;   // recent positions, newest first
    };

    std::vector<Lane> m_lanes;

    std::shared_ptr<Slider>  m_duration;
    std::shared_ptr<TextBox> m_duration_label;
    std::shared_ptr<Button>  m_auto_button;

    sf::VertexArray m_tracks{sf::PrimitiveType::Triangles};
    sf::VertexArray m_pucks{sf::PrimitiveType::Triangles};

    bool  m_auto{true};
    bool  m_at_right{false};
    float m_hold{0.0f};

    [[nodiscard]] float lane_y(std::size_t index) const;
    [[nodiscard]] bool  in_lane_area(sf::Vector2f point) const;

    void retarget(float x);
    void set_auto(bool on);
    void apply_duration();
    void rebuild_tracks();

public:
    EasingScene();

    void on_enter() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;
};

}

#endif
