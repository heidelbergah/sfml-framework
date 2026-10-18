// The smallest useful program: one scene, one square that springs to
// wherever you click. Copy this folder to start something of your own.
//
//     make EXAMPLE=hello run

#include "../../include/scenes/SceneManager.hpp"

class HelloScene : public Scene
{
    sf::RectangleShape         m_square{{24.0f, 24.0f}};
    Interpolated<sf::Vector2f> m_position{{480.0f, 270.0f}};

public:
    HelloScene()
    {
        m_square.setOrigin({12.0f, 12.0f});
        m_square.setFillColor(sf::Color(240, 170, 78));

        m_position.set_transition(TransitionFunction::EaseOutElastic);
        m_position.set_duration(0.8f);
    }

    void handle_event(const sf::Event& event, sf::Vector2f mouse) override
    {
        // Assigning starts a tween from wherever the square is right now.
        if(event.is<sf::Event::MouseButtonPressed>())
            m_position = mouse;
    }

    void render(sf::RenderTarget& target) override
    {
        m_square.setPosition(m_position);
        target.draw(m_square);
    }
};

int main()
{
    SceneManager scenes("hello");
    scenes.add_scene("hello", std::make_shared<HelloScene>());
    scenes.swap_scene("hello", SceneTransitionType::INSTANT);
    scenes.run(60);
}
