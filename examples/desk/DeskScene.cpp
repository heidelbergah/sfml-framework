/**
 * DeskScene.cpp  -  Desk
 */

#include "DeskScene.hpp"

#include "Content.hpp"
#include "Widgets.hpp"

#include <cmath>
#include <random>

namespace desk
{

namespace
{
    constexpr float VIEW_W = static_cast<float>(WINDOW_WIDTH);
    constexpr float VIEW_H = static_cast<float>(WINDOW_HEIGHT);

    // A press only becomes a drag once the mouse has moved this far, so
    // a slightly shaky click still turns a page.
    constexpr float DRAG_THRESHOLD = 3.0f;

    const sf::Color WOOD      {96,  62,  40};
    const sf::Color WOOD_ALT  {88,  56,  36};
    const sf::Color SEAM      {52,  32,  22};
    const sf::Color GRAIN_DARK{78,  48,  31};
    const sf::Color GRAIN_LIT {108, 72,  48};
    const sf::Color RING      {70,  44,  30};

    const sf::Vector2f LAMP{430.0f, 210.0f};
}

DeskScene::DeskScene()
{
    build_desk();

    // Bottom of the pile first. The late edition sits under the early
    // one, so turning it over is something you go looking for.
    auto late = std::make_unique<DeskItem>(NEWSPAPER_SIZE, late_edition(), sf::Vector2f{318.0f, 262.0f});
    late->set_age(1.0f);
    m_items.push_back(std::move(late));

    m_items.push_back(std::make_unique<DeskItem>(NEWSPAPER_SIZE, early_edition(),
                                                 sf::Vector2f{240.0f, 276.0f}));

    auto note = std::make_unique<DeskItem>(NOTE_SIZE, sticky_note(), sf::Vector2f{828.0f, 452.0f});
    m_items.push_back(std::move(note));

    auto from_home = std::make_unique<DeskItem>(LETTER_SIZE, letter(), sf::Vector2f{620.0f, 290.0f});
    from_home->set_age(0.12f);
    m_items.push_back(std::move(from_home));

    auto photo = std::make_unique<DeskItem>(PHOTO_SIZE, photograph(), sf::Vector2f{830.0f, 150.0f},
                                            DeskItem::Turn::WHOLE);
    photo->set_age(0.2f);
    m_items.push_back(std::move(photo));
}

void DeskScene::build_desk()
{
    m_desk.clear();
    std::mt19937 rng{1921};
    auto real = [&rng](float lo, float hi) { return std::uniform_real_distribution<float>{lo, hi}(rng); };

    // Planks, with a dark seam between each.
    float const plank = 54.0f;
    for(int row = 0; row * plank < VIEW_H; ++row)
    {
        float const y = static_cast<float>(row) * plank;
        push_rect(m_desk, {{0.0f, y}, {VIEW_W, plank}}, row % 2 ? WOOD_ALT : WOOD);
        push_rect(m_desk, {{0.0f, y + plank - 2.0f}, {VIEW_W, 2.0f}}, SEAM);

        // Grain: short streaks along the plank, a few dark, a few light.
        for(int i = 0; i < 46; ++i)
        {
            float const gx = std::floor(real(-20.0f, VIEW_W));
            float const gy = std::floor(real(y + 3.0f, y + plank - 5.0f));
            float const gw = std::floor(real(12.0f, 70.0f));
            push_rect(m_desk, {{gx, gy}, {gw, 1.0f}}, real(0.0f, 1.0f) < 0.7f ? GRAIN_DARK : GRAIN_LIT);
        }

        // The odd knot.
        if(real(0.0f, 1.0f) < 0.6f)
        {
            float const kx = std::floor(real(40.0f, VIEW_W - 40.0f));
            float const ky = std::floor(real(y + 14.0f, y + plank - 16.0f));
            push_rect(m_desk, {{kx - 4.0f, ky - 2.0f}, {9.0f, 5.0f}}, GRAIN_DARK);
            push_rect(m_desk, {{kx - 2.0f, ky - 1.0f}, {5.0f, 3.0f}}, SEAM);
        }
    }

    // A ring from someone's mug, not quite closed.
    sf::Vector2f const ring{742.0f, 486.0f};
    for(int y = -24; y <= 24; ++y)
    {
        for(int x = -24; x <= 24; ++x)
        {
            float const d = std::hypot(float(x), float(y));
            float const a = std::atan2(float(y), float(x));
            if(d > 20.5f && d < 23.0f && !(a > 0.4f && a < 1.1f))
                push_rect(m_desk, {{ring.x + float(x), ring.y + float(y)}, {1.0f, 1.0f}}, RING);
        }
    }

    // A pencil.
    sf::Vector2f const pencil{486.0f, 470.0f};
    push_rect(m_desk, {pencil + sf::Vector2f{2.0f, 3.0f}, {78.0f, 5.0f}}, sf::Color(30, 18, 12, 120));
    push_rect(m_desk, {pencil, {64.0f, 5.0f}}, sf::Color(214, 170, 72));
    push_rect(m_desk, {pencil + sf::Vector2f{0.0f, 2.0f}, {64.0f, 1.0f}}, sf::Color(184, 140, 56));
    push_rect(m_desk, {pencil + sf::Vector2f{64.0f, 0.0f}, {5.0f, 5.0f}}, sf::Color(170, 170, 160));
    push_rect(m_desk, {pencil + sf::Vector2f{69.0f, 0.0f}, {6.0f, 5.0f}}, sf::Color(206, 128, 120));
    push_rect(m_desk, {pencil + sf::Vector2f{-6.0f, 1.0f}, {6.0f, 3.0f}}, sf::Color(222, 196, 150));
    push_rect(m_desk, {pencil + sf::Vector2f{-9.0f, 2.0f}, {3.0f, 1.0f}}, sf::Color(50, 44, 44));
}

void DeskScene::on_enter()
{
    if(!m_manager || m_paper) return;

    ShaderManager& shaders = m_manager->get_shaders();
    m_paper = shaders.load_fragment("desk_paper", "shaders/desk/paper.frag");
    m_lamp  = shaders.load_fragment("desk_lamp",  "shaders/desk/lamp.frag");
}

DeskItem* DeskScene::item_at(sf::Vector2f point)
{
    // Top of the pile wins.
    for(auto it = m_items.rbegin(); it != m_items.rend(); ++it)
        if((*it)->contains(point)) return it->get();
    return nullptr;
}

void DeskScene::raise(DeskItem* item)
{
    auto it = std::find_if(m_items.begin(), m_items.end(),
                           [item](const auto& p) { return p.get() == item; });
    if(it == m_items.end() || std::next(it) == m_items.end()) return;

    std::unique_ptr<DeskItem> moved = std::move(*it);
    m_items.erase(it);
    m_items.push_back(std::move(moved));
}

void DeskScene::handle_event(const sf::Event& event, sf::Vector2f mouse)
{
    if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        if(key->code == sf::Keyboard::Key::Escape) EventBus::publish(Event(EventType::QUIT));
        return;
    }

    if(event.is<sf::Event::MouseMoved>())
    {
        m_mouse = mouse;

        if(m_held && !m_dragging && distance(m_mouse, m_press) > DRAG_THRESHOLD)
        {
            m_dragging = true;
            m_held->pick_up();
            m_held->set_hover_corner(0);
        }

        // Folded corners grow a little under the cursor, but only on the
        // paper that would actually receive the click.
        if(!m_held)
        {
            DeskItem* top = item_at(m_mouse);
            for(auto& item : m_items)
                item->set_hover_corner(item.get() == top ? item->corner_at(item->to_local(m_mouse)) : 0);
        }
    }
    else if(const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if(pressed->button != sf::Mouse::Button::Left) return;

        m_mouse = mouse;
        m_held  = item_at(mouse);
        if(!m_held) return;

        raise(m_held);
        m_press       = mouse;
        m_grab        = mouse - m_held->get_center();
        m_last_center = m_held->get_center();
        m_dragging    = false;
    }
    else if(const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if(released->button != sf::Mouse::Button::Left || !m_held) return;

        if(m_dragging) m_held->put_down();
        else           m_held->click(m_held->to_local(mouse));

        m_held     = nullptr;
        m_dragging = false;
    }
}

void DeskScene::update(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();

    if(m_held && m_dragging && dt > 0.0f)
    {
        // Keep at least a strip of the paper on the desk.
        sf::Vector2f const half = m_held->get_size() / 2.0f;
        sf::Vector2f const want = m_mouse - m_grab;
        sf::Vector2f const center{std::clamp(want.x, 24.0f - half.x, VIEW_W - 24.0f + half.x),
                                  std::clamp(want.y, 24.0f - half.y, VIEW_H - 24.0f + half.y)};

        m_held->drag_to(center, (center - m_last_center) / dt);
        m_last_center = center;
    }

    for(auto& item : m_items) item->update(dt);
}

void DeskScene::render(sf::RenderTarget& target)
{
    target.setView(target.getDefaultView());
    target.draw(m_desk);

    for(const auto& item : m_items) item->draw(target, m_paper);

    // The lamp lights everything, papers included.
    if(m_lamp)
    {
        m_lamp->setUniform("resolution", sf::Glsl::Vec2{VIEW_W, VIEW_H});
        m_lamp->setUniform("light", sf::Glsl::Vec2{LAMP.x, LAMP.y});

        sf::Vertex const quad[] = {
            {{0.0f, 0.0f},     sf::Color::White, {0.0f, 0.0f}},
            {{VIEW_W, 0.0f},   sf::Color::White, {1.0f, 0.0f}},
            {{0.0f, VIEW_H},   sf::Color::White, {0.0f, 1.0f}},
            {{VIEW_W, VIEW_H}, sf::Color::White, {1.0f, 1.0f}},
        };
        sf::RenderStates states;
        states.shader    = m_lamp;
        states.blendMode = sf::BlendMultiply;
        target.draw(quad, 4, sf::PrimitiveType::TriangleStrip, states);
    }
}

}
