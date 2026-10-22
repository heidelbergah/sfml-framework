/**
 * Screens.hpp  -  Cinder
 *
 * The title and the ending. Both sit on the same small meadow, drawn with
 * the same reveal pass as the game: on the title a single spark wanders
 * over it and colour follows; at the end the whole meadow is warm.
 */

#ifndef CINDER_SCREENS_HPP
#define CINDER_SCREENS_HPP

#include "../../include/scenes/Scene.hpp"
#include "../../include/scenes/SceneManager.hpp"

#include "Level.hpp"
#include "Reveal.hpp"
#include "World.hpp"

#include <vector>

namespace cinder
{

class Meadow
{
private:
    LevelDef m_def;
    World    m_world;
    Reveal   m_reveal;
    Motes    m_motes;

    struct Trail { sf::Vector2f pos; float life; };
    std::vector<Trail> m_trail;

    sf::Vector2f m_spark;
    sf::Vector2f m_last;
    float m_time{0.0f};
    float m_flame{0.0f};

public:
    Meadow();

    void initialize(ShaderManager& shaders);
    void update(float dt, bool wander);

    /*
     * `warmth` 0 shows only what the spark has touched; 1 shows it all.
     */
    void render(sf::RenderTarget& target, float warmth);
};

class MenuScene : public Scene
{
private:
    Meadow m_meadow;
    float  m_time{0.0f};
    bool   m_leaving{false};

public:
    void on_enter() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update_realtime(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;
    [[nodiscard]] sf::Color get_clear_color() const override;
};

class EndScene : public Scene
{
private:
    Meadow m_meadow;
    float  m_time{0.0f};

public:
    void on_enter() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update_realtime(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;
    [[nodiscard]] sf::Color get_clear_color() const override;
};

}

#endif
