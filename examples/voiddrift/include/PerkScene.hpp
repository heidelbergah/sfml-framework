/**
 * PerkScene.hpp  -  Void Drift
 *
 * Shown when the player dies but has lives remaining.
 * Three perk cards fly in with EaseOutElastic, player picks one,
 * then fades back to the game.
 */

#ifndef PERK_SCENE_HPP
#define PERK_SCENE_HPP

#include "Scene.hpp"
#include "../framework/EventBus.hpp"
#include "../gui/WidgetManager.hpp"
#include "../gui/Button.hpp"
#include "../gui/Frame.hpp"
#include "../gui/TextBox.hpp"
#include "../Starfield.hpp"
#include "../RunState.hpp"

#include <array>

// One perk card = a Frame widget holding title + description + a pick button
struct PerkCard
{
    std::shared_ptr<Frame>  frame;
    PerkType                perk;
    bool                    chosen = false;

    // Off-screen start position (for fly-in animation)
    sf::Vector2f            start_pos;
    sf::Vector2f            target_pos;
};

class PerkScene : public Scene
{
private:
    sf::Vector2f m_resolution_scale = {static_cast<float>(SCALE), static_cast<float>(SCALE)};

    sf::Font      m_font;
    Starfield     m_stars;
    WidgetManager m_card_manager;   // holds the three Frame widgets

    std::array<PerkCard, 3> m_cards;
    std::array<PerkDef, 3>  m_offered;

    sf::Text* m_header     = nullptr;
    sf::Text* m_lives_text = nullptr;
    sf::Text* m_subtext    = nullptr;

    float m_enter_timer = 0.f;   // stagger card fly-in
    bool  m_chosen      = false; // perk has been picked, waiting for exit

    ListenerHandle m_to_game_handle;

    void build_cards();
    void pick_perk(int card_index);

    // Choose 3 distinct random perks to offer
    void select_offered_perks();

public:
    PerkScene();
    ~PerkScene();

    void on_enter()       override;
    void on_exit()        override;
    void process_events() override;
    void update(sf::Time dt) override;
    void render()         override;
};

#endif
