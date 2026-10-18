/**
 * PerkScene.cpp  -  Void Drift
 *
 * Three perk cards fly in from off-screen with EaseOutElastic stagger.
 * Each card is a Frame widget containing:
 *   - A coloured title TextBox
 *   - A description TextBox
 *   - A "TAKE IT" Button
 *
 * Clicking a card applies the perk, then fades to game.
 */

#include "../../include/scenes/PerkScene.hpp"

// ─────────────────────────────────────────────────────────────────────────────

PerkScene::PerkScene() {}

PerkScene::~PerkScene()
{
    delete m_header;
    delete m_lives_text;
    delete m_subtext;
}

// ─── Perk selection ───────────────────────────────────────────────────────────

void PerkScene::select_offered_perks()
{
    // Build candidate list (exclude EXTRA_LIFE if lives already at 5+)
    std::vector<int> candidates;
    for(int i = 0; i < (int)(sizeof(ALL_PERKS) / sizeof(ALL_PERKS[0])); ++i)
    {
        if(ALL_PERKS[i].type == PerkType::EXTRA_LIFE && RunState::lives >= 5) continue;
        candidates.push_back(i);
    }

    // Shuffle and take 3
    std::shuffle(candidates.begin(), candidates.end(), random_gen);
    for(int i = 0; i < 3; ++i)
        m_offered[i] = ALL_PERKS[candidates[i % candidates.size()]];
}

// ─── Build card widgets ───────────────────────────────────────────────────────

void PerkScene::build_cards()
{
    const float CARD_W  = 140.f;
    const float CARD_H  = 120.f;
    const float GAP     = 20.f;
    const float TOTAL_W = CARD_W * 3 + GAP * 2;
    const float START_X = ((float)WINDOW_WIDTH - TOTAL_W) / 2.f;
    const float TARGET_Y = (float)WINDOW_HEIGHT / 2.f - CARD_H / 2.f;

    for(int i = 0; i < 3; ++i)
    {
        const PerkDef& def = m_offered[i];

        sf::Color accent(def.color_r, def.color_g, def.color_b);
        sf::Color card_bg(12, 18, 32);
        sf::Color dark_accent(
            (uint8_t)(def.color_r / 4),
            (uint8_t)(def.color_g / 4),
            (uint8_t)(def.color_b / 4));

        // Target position (centred horizontally, vertically centred)
        float tx = START_X + i * (CARD_W + GAP);
        float ty = TARGET_Y;

        // Start position: fly in from bottom
        float sy = (float)WINDOW_HEIGHT + CARD_H + 20.f;

        m_cards[i].target_pos = {tx, ty};
        m_cards[i].start_pos  = {tx, sy};
        m_cards[i].perk       = def.type;

        // Build Frame
        auto frame = std::make_shared<Frame>(sf::Vector2f(CARD_W, CARD_H), card_bg, m_font);
        frame->set_outline(accent, 2);
        frame->set_transition(TransitionFunction::EaseOutElastic);
        frame->set_transition_duration(0.7f);

        // Start off-screen (instant), will animate to target
        frame->set_position({tx, sy}, true);

        // Taskbar as coloured title strip
        frame->add_taskbar(18, dark_accent, accent, 1, def.name);

        // Description TextBox inside the frame
        auto desc = std::make_shared<TextBox>(
            sf::Vector2f(CARD_W - 8.f, 60.f),
            sf::Color::Transparent,
            def.description,
            m_font,
            sf::Color(200, 220, 240));
        desc->set_position({4.f, 20.f}, true);
        desc->set_transition(TransitionFunction::None);
        desc->set_character_size(8);
        frame->add_widget("desc", desc);

        // "TAKE IT" pick button
        auto btn = std::make_shared<Button>(sf::Vector2f(CARD_W - 12.f, 20.f), dark_accent);
        btn->set_outline(accent, 1);
        btn->add_text("TAKE IT", m_font, accent);
        btn->set_position({6.f, CARD_H - 26.f}, true);
        btn->set_transition(TransitionFunction::None);

        int idx = i;  // capture by value
        btn->set_callback([this, idx](){ pick_perk(idx); });
        frame->add_widget("btn", btn);

        m_cards[i].frame = frame;
        m_card_manager.add_widget("card_" + std::to_string(i), frame);
    }
}

// ─── Pick a perk ─────────────────────────────────────────────────────────────

void PerkScene::pick_perk(int idx)
{
    if(m_chosen) return;
    m_chosen = true;

    RunState::apply_perk(m_cards[idx].perk);

    // Fly out the unchosen cards downward
    for(int i = 0; i < 3; ++i)
    {
        if(i == idx) continue;
        m_cards[i].frame->set_transition(TransitionFunction::EaseInBack);
        m_cards[i].frame->set_transition_duration(0.35f);
        m_cards[i].frame->set_position({m_cards[i].target_pos.x,
                                         (float)WINDOW_HEIGHT + 200.f});
    }

    // Chosen card flies to centre and up
    m_cards[idx].frame->set_transition(TransitionFunction::EaseOutBack);
    m_cards[idx].frame->set_transition_duration(0.45f);
    m_cards[idx].frame->set_position({
        (float)WINDOW_WIDTH / 2.f - 70.f,
        (float)WINDOW_HEIGHT / 2.f - 90.f});

    // Delay switch to game — handled by timer in update
}

// ─── on_enter ─────────────────────────────────────────────────────────────────

void PerkScene::on_enter()
{
    if(!m_font.openFromFile("assets/fonts/Windows-Regular.ttf"))
        m_font.openFromFile("././assets/fonts/Windows-Regular.ttf");
    m_font.setSmooth(false);

    m_enter_timer = 0.f;
    m_chosen      = false;

    // Clear any leftover cards from a previous visit
    for(int i = 0; i < 3; ++i)
        m_card_manager.remove_widget("card_" + std::to_string(i));

    select_offered_perks();
    build_cards();

    // Header
    m_header = new sf::Text(m_font, "CHOOSE A PERK", 18);
    m_header->setFillColor(sf::Color(200, 240, 255));
    m_header->setOutlineColor(sf::Color(0, 60, 120));
    m_header->setOutlineThickness(1.5f);
    auto hb = m_header->getLocalBounds();
    m_header->setOrigin({hb.size.x / 2.f, hb.size.y / 2.f});
    m_header->setPosition({(float)WINDOW_WIDTH / 2.f, (float)WINDOW_HEIGHT * 0.18f});

    m_lives_text = new sf::Text(m_font,
        std::to_string(RunState::lives) + " " +
        (RunState::lives == 1 ? "LIFE" : "LIVES") + " REMAINING", 10);
    m_lives_text->setFillColor(sf::Color(80, 220, 140));
    auto lb = m_lives_text->getLocalBounds();
    m_lives_text->setOrigin({lb.size.x / 2.f, lb.size.y / 2.f});
    m_lives_text->setPosition({(float)WINDOW_WIDTH / 2.f, (float)WINDOW_HEIGHT * 0.18f + 22.f});

    m_subtext = new sf::Text(m_font, "Click a card to choose", 8);
    m_subtext->setFillColor(sf::Color(100, 140, 180));
    auto sb = m_subtext->getLocalBounds();
    m_subtext->setOrigin({sb.size.x / 2.f, sb.size.y / 2.f});
    m_subtext->setPosition({(float)WINDOW_WIDTH / 2.f, (float)WINDOW_HEIGHT * 0.82f});

    m_to_game_handle = EventBus::subscribe(EventType::SWITCH_TO_GAME, [](const Event&){});
}

// ─── on_exit ──────────────────────────────────────────────────────────────────

void PerkScene::on_exit()
{
    EventBus::unsubscribe(EventType::SWITCH_TO_GAME, m_to_game_handle);
    for(int i = 0; i < 3; ++i)
        m_card_manager.remove_widget("card_" + std::to_string(i));

    delete m_header;     m_header     = nullptr;
    delete m_lives_text; m_lives_text = nullptr;
    delete m_subtext;    m_subtext    = nullptr;
}

// ─── process_events ───────────────────────────────────────────────────────────

void PerkScene::process_events()
{
    sf::Vector2f m_resolution_scale = {static_cast<float>(SCALE), static_cast<float>(SCALE)};
    sf::Vector2i px  = sf::Mouse::getPosition(*m_render_window);
    sf::Vector2f sp  = {(float)px.x / m_resolution_scale.x, (float)px.y / m_resolution_scale.y};
    sf::Vector2f wp  = m_render_texture->mapPixelToCoords(sf::Vector2i(sp));

    while(const std::optional ev = m_render_window->pollEvent())
    {
        if(ev->is<sf::Event::Closed>()) m_render_window->close();

        if(ev->getIf<sf::Event::MouseButtonPressed>() || ev->getIf<sf::Event::MouseMoved>())
        {
            if(!m_chosen)
            {
                for(const auto& w : m_card_manager.get_widgets())
                    w->handle_event(*m_render_window, wp);
            }
        }
    }
}

// ─── update ───────────────────────────────────────────────────────────────────

void PerkScene::update(sf::Time dt)
{
    float s = dt.asSeconds();
    m_enter_timer += s;
    m_stars.update(dt);

    // Stagger card fly-in: card 0 at 0.1s, card 1 at 0.25s, card 2 at 0.4s
    float stagger[] = {0.1f, 0.25f, 0.4f};
    for(int i = 0; i < 3; ++i)
    {
        if(m_enter_timer >= stagger[i] && !m_chosen)
        {
            // Trigger fly-in by setting target position (transition already set)
            sf::Vector2f cur = m_cards[i].frame->get_position();
            // Only trigger once — check if card is still at start Y
            if(cur.y > (float)WINDOW_HEIGHT - 10.f)
                m_cards[i].frame->set_position(m_cards[i].target_pos);
        }
    }

    for(const auto& w : m_card_manager.get_widgets()) w->update(dt);

    // After picking, wait for animation then switch to game
    if(m_chosen)
    {
        // Wait until chosen card's transition is done (~0.5s)
        static float exit_timer = 0.f;
        exit_timer += s;
        if(exit_timer > 0.7f)
        {
            exit_timer = 0.f;
            EventBus::publish(Event(EventType::SWITCH_TO_GAME, ""));
        }
    }
}

// ─── render ───────────────────────────────────────────────────────────────────

void PerkScene::render()
{
    m_render_texture->clear(sf::Color(4, 6, 20));
    m_stars.draw(*m_render_texture);

    // Subtle vignette-like radial darkening: draw a dark ring
    {
        sf::RectangleShape top({(float)WINDOW_WIDTH, 40.f});
        top.setFillColor(sf::Color(0, 0, 0, 80));
        top.setPosition({0.f, 0.f});
        m_render_texture->draw(top);
        sf::RectangleShape bot({(float)WINDOW_WIDTH, 40.f});
        bot.setFillColor(sf::Color(0, 0, 0, 80));
        bot.setPosition({0.f, (float)WINDOW_HEIGHT - 40.f});
        m_render_texture->draw(bot);
    }

    if(m_header)     m_render_texture->draw(*m_header);
    if(m_lives_text) m_render_texture->draw(*m_lives_text);
    if(!m_chosen && m_subtext) m_render_texture->draw(*m_subtext);

    for(const auto& w : m_card_manager.get_widgets())
        m_render_texture->draw(*w);
}
