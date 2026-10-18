/**
 * NameScene.cpp  -  Showcase
 */

#include "NameScene.hpp"

#include <cctype>
#include <cstdio>
#include <fstream>

namespace showcase
{

namespace
{
    constexpr int   COLUMNS   = 4;
    constexpr int   ROWS      = 7;
    constexpr float CARD_W    = 208.0f;
    constexpr float CARD_H    = 34.0f;
    constexpr float STEP_X    = 228.0f;
    constexpr float STEP_Y    = 48.0f;
    constexpr float GRID_TOP  = 128.0f;
    constexpr float STAGGER   = 0.035f;     // seconds between cards

    constexpr unsigned MIN_ORDER = 1;
    constexpr unsigned MAX_ORDER = 5;

    std::string lower(std::string text)
    {
        for(char& c : text)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text;
    }

    /*
     * Spells umlauts and eszett the way German does without them
     * (Goetz, Guenther, Gerhauss), and drops anything else outside
     * ASCII.
     *
     * Two reasons. Markov works byte by byte, so it can split a
     * multi-byte UTF-8 character and emit half of one. And TextWidget
     * hands std::string straight to sf::Text, which does not decode
     * UTF-8, so even a whole character would draw as two wrong glyphs.
     */
    std::string to_ascii(const std::string& utf8)
    {
        static const std::pair<const char*, const char*> table[] = {
            {"\xC3\xA4", "ae"}, {"\xC3\xB6", "oe"}, {"\xC3\xBC", "ue"},
            {"\xC3\x84", "Ae"}, {"\xC3\x96", "Oe"}, {"\xC3\x9C", "Ue"},
            {"\xC3\x9F", "ss"},
        };

        std::string out;
        for(std::size_t i = 0; i < utf8.size();)
        {
            unsigned char const c = static_cast<unsigned char>(utf8[i]);
            if(c < 0x80)
            {
                out += static_cast<char>(c);
                ++i;
                continue;
            }

            bool replaced = false;
            for(const auto& [from, to] : table)
            {
                if(utf8.compare(i, 2, from) == 0)
                {
                    out += to;
                    i += 2;
                    replaced = true;
                    break;
                }
            }
            if(replaced) continue;

            // Skip the rest of an unknown multi-byte sequence.
            ++i;
            while(i < utf8.size() &&
                  (static_cast<unsigned char>(utf8[i]) & 0xC0) == 0x80) ++i;
        }
        return out;
    }
}

NameScene::NameScene()
    : DemoScene("Name forge", "click generate or press space"),
      m_markov("assets/textfiles/englishnames.txt", 3)
{
    float const row_y = 76.0f;

    m_corpora = {
        {"english",   "assets/textfiles/englishnames.txt", nullptr},
        {"german",    "assets/textfiles/germannames.txt",  nullptr},
        {"callsigns", "assets/names.txt",                  nullptr},
    };

    for(std::size_t i = 0; i < m_corpora.size(); ++i)
    {
        auto button = make_button({100.0f, 26.0f}, m_corpora[i].label);
        button->set_position({24.0f + 108.0f * static_cast<float>(i), row_y}, true);
        button->set_callback([this, i]() { select_corpus(i); });
        m_widgets.add_widget("corpus_" + m_corpora[i].label, button);
        m_corpora[i].button = button;
    }

    auto order_title = make_label({56.0f, 26.0f}, "order", TEXT_DIM, Alignment::RIGHT);
    order_title->set_position({368.0f, row_y}, true);
    m_widgets.add_widget("order_title", order_title);

    m_order = std::make_shared<Slider>(sf::Vector2f{120.0f, 14.0f}, SAGE);
    m_order->set_body_color(LINE);
    m_order->set_transition(TransitionFunction::None);
    m_order->set_position({436.0f, row_y + 6.0f}, true);
    m_order->set_percentage(static_cast<float>(m_current_order - MIN_ORDER) /
                            static_cast<float>(MAX_ORDER - MIN_ORDER));
    m_widgets.add_widget("order", m_order);

    m_order_label = make_label({24.0f, 26.0f}, std::to_string(m_current_order), TEXT);
    m_order_label->set_position({566.0f, row_y}, true);
    m_widgets.add_widget("order_label", m_order_label);

    auto generate_button = make_button({140.0f, 26.0f}, "generate", SAGE, INK);
    generate_button->set_position({VIEW_W - 24.0f - 140.0f, row_y}, true);
    generate_button->set_callback([this]() { generate(); });
    m_widgets.add_widget("generate", generate_button);

    float const grid_left = (VIEW_W - (STEP_X * (COLUMNS - 1) + CARD_W)) / 2.0f;

    for(int row = 0; row < ROWS; ++row)
    {
        for(int col = 0; col < COLUMNS; ++col)
        {
            Card card;
            card.home = {grid_left + STEP_X * static_cast<float>(col),
                         GRID_TOP + STEP_Y * static_cast<float>(row)};

            card.box = std::make_shared<TextBox>(sf::Vector2f{CARD_W, CARD_H}, PANEL,
                                                 "", *font(), TEXT);
            card.box->set_character_size(TEXT_SIZE);
            card.box->set_transition(TransitionFunction::EaseOutBack);
            card.box->set_transition_duration(0.35f);
            card.box->set_position(card.home, true);

            m_widgets.add_widget("card_" + std::to_string(m_cards.size()), card.box);
            m_cards.push_back(card);
        }
    }

    m_stats = make_label({VIEW_W - 48.0f, 24.0f}, "", TEXT_DIM, Alignment::RIGHT);
    m_stats->set_position({24.0f, VIEW_H - 34.0f}, true);
    m_widgets.add_widget("stats", m_stats);

    select_corpus(0);
}

unsigned NameScene::slider_order() const
{
    float const value = m_order->get_value(static_cast<float>(MIN_ORDER),
                                           static_cast<float>(MAX_ORDER));
    return static_cast<unsigned>(std::lround(value));
}

void NameScene::select_corpus(std::size_t index)
{
    m_corpus = index;

    for(std::size_t i = 0; i < m_corpora.size(); ++i)
    {
        bool const active = (i == index);
        recolor(*m_corpora[i].button, active ? LINE : PANEL_HI);
        m_corpora[i].button->set_text_color(active ? TEXT : TEXT_DIM);
    }

    // Load the list here rather than with update_base_names(), so it can
    // be cleaned up first and kept for the "already in the list" check.
    std::vector<std::string> names;
    m_known.clear();

    std::ifstream file(m_corpora[index].path);
    std::string line;
    while(std::getline(file, line))
    {
        std::string name = to_ascii(line);
        while(!name.empty() && std::isspace(static_cast<unsigned char>(name.back())))
            name.pop_back();
        if(name.empty()) continue;

        m_known.insert(lower(name));
        names.push_back(std::move(name));
    }

    m_markov.set_base_names(names);
    generate();
}

void NameScene::generate()
{
    m_invented = 0;

    for(std::size_t i = 0; i < m_cards.size(); ++i)
    {
        Card& card   = m_cards[i];
        card.pending = m_markov.generate_name(4, 11);
        card.known   = m_known.count(lower(card.pending)) > 0;
        card.delay   = STAGGER * static_cast<float>(i);

        if(!card.known) ++m_invented;
    }

    update_stats();
}

void NameScene::update_stats()
{
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "%d of %zu new      dim names are already in the list",
                  m_invented, m_cards.size());
    m_stats->set_string(buffer);
}

void NameScene::on_enter()
{
    DemoScene::on_enter();
    generate();
}

void NameScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    DemoScene::handle_event(event, mouse_pos);

    if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        if(key->code == sf::Keyboard::Key::Space) generate();
    }
}

void NameScene::update(sf::Time delta_time)
{
    DemoScene::update(delta_time);

    // Snap the slider to whole orders and retrain when it lands on a new one.
    unsigned const order = slider_order();
    if(order != m_current_order)
    {
        m_current_order = order;
        m_markov.change_order(order);
        m_order_label->set_string(std::to_string(order));
        generate();
    }
    if(!m_order->is_being_dragged())
    {
        m_order->set_percentage(static_cast<float>(order - MIN_ORDER) /
                                static_cast<float>(MAX_ORDER - MIN_ORDER));
    }

    // Each card waits its turn, then pops up from just below its slot
    // and types the new name in.
    float const dt = delta_time.asSeconds();
    for(Card& card : m_cards)
    {
        if(card.delay < 0.0f) continue;

        card.delay -= dt;
        if(card.delay >= 0.0f) continue;

        card.box->set_position(card.home + sf::Vector2f{0.0f, 10.0f}, true);
        card.box->set_position(card.home);
        card.box->set_text_color(card.known ? TEXT_DIM : TEXT);
        card.box->set_string(card.pending);
        card.box->add_text_scroll(45.0f);
        card.box->set_color(card.known ? mix(PANEL, INK, 0.5f) : PANEL);
    }
}

void NameScene::render(sf::RenderTarget& target)
{
    target.setView(target.getDefaultView());
    target.draw(m_widgets);
}

}
