/**
 * NameScene.hpp  -  Showcase
 *
 * Markov-chain names from three word lists. Each batch types itself
 * into a grid of TextBoxes on a stagger.
 *
 * Names that already exist in the source list are drawn dimmer, which
 * makes the order slider easy to read: low orders invent freely, high
 * orders mostly hand back names they were trained on.
 */

#ifndef SHOWCASE_NAME_SCENE_HPP
#define SHOWCASE_NAME_SCENE_HPP

#include "../../include/gui/Slider.hpp"
#include "../../include/utilities/markov.hpp"

#include "DemoScene.hpp"

#include <unordered_set>

namespace showcase
{

class NameScene : public DemoScene
{
private:
    struct Corpus
    {
        std::string             label;
        std::string             path;
        std::shared_ptr<Button> button;
    };

    struct Card
    {
        std::shared_ptr<TextBox> box;
        sf::Vector2f             home;
        std::string              pending;    // next name, shown when delay runs out
        bool                     known{false};
        float                    delay{-1.0f};
    };

    Markov m_markov;

    std::vector<Corpus>              m_corpora;
    std::unordered_set<std::string>  m_known;     // lower-cased source names
    std::vector<Card>                m_cards;

    std::shared_ptr<Slider>  m_order;
    std::shared_ptr<TextBox> m_order_label;
    std::shared_ptr<TextBox> m_stats;

    std::size_t m_corpus{0};
    unsigned    m_current_order{3};
    int         m_invented{0};

    void select_corpus(std::size_t index);
    void generate();
    void update_stats();

    [[nodiscard]] unsigned slider_order() const;

public:
    NameScene();

    void on_enter() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;
};

}

#endif
