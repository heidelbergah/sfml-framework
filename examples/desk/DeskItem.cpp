/**
 * DeskItem.cpp  -  Desk
 */

#include "DeskItem.hpp"

#include "Widgets.hpp"

#include <cmath>

namespace desk
{

namespace
{
    constexpr float CORNER      = 22.0f;   // clickable square at each bottom corner
    constexpr float FOLD        = 9.0f;    // drawn size of the folded corner
    constexpr float FOLD_HOVER  = 13.0f;
    constexpr float MAX_LEAN    = 6.0f;    // degrees

    const sf::Color SHADOW{8, 5, 3, 110};
}

DeskItem::DeskItem(sf::Vector2f size, std::vector<std::shared_ptr<Frame>> pages,
                   sf::Vector2f center, Turn turn)
    : m_pages(std::move(pages)), m_size(size), m_turn(turn), m_center(center)
{
    (void)m_target.resize({static_cast<unsigned>(size.x), static_cast<unsigned>(size.y)});
    m_target.setSmooth(false);

    for(auto& page : m_pages)
    {
        page->set_transition(TransitionFunction::None);
        page->set_position({0.0f, 0.0f}, true);
    }

    m_flip.set_transition(TransitionFunction::EaseInOutQuad);
    m_flip.set_duration(0.32f);
    m_lift.set_transition(TransitionFunction::EaseOutQuad);
    m_lift.set_duration(0.12f);
}

float DeskItem::squash() const
{
    // Edge-on halfway through a turn: the page narrows to nothing, the
    // next one is swapped in, and it widens again.
    if(!m_turning) return 1.0f;
    return std::abs(std::cos(m_flip.get_value() * 3.14159265f));
}

sf::Transform DeskItem::transform() const
{
    float const lift = m_lift;
    sf::Transform t;
    t.translate(floor_vector(m_center - sf::Vector2f{lift * 3.0f, lift * 3.0f}));
    t.rotate(sf::degrees(m_angle));
    t.scale({squash(), 1.0f});
    t.translate(-floor_vector(m_size / 2.0f));
    return t;
}

sf::Vector2f DeskItem::to_local(sf::Vector2f point) const
{
    return transform().getInverse().transformPoint(point);
}

bool DeskItem::contains(sf::Vector2f point) const
{
    sf::Vector2f const local = to_local(point);
    return local.x >= 0.0f && local.y >= 0.0f && local.x < m_size.x && local.y < m_size.y;
}

int DeskItem::corner_at(sf::Vector2f local) const
{
    if(m_turn != Turn::CORNERS || local.y < m_size.y - CORNER) return 0;
    if(local.x > m_size.x - CORNER && m_page + 1 < m_pages.size()) return 1;
    if(local.x < CORNER && m_page > 0) return -1;
    return 0;
}

void DeskItem::pick_up()
{
    m_lift = 1.0f;
}

void DeskItem::put_down()
{
    m_lift = 0.0f;
    m_lean = 0.0f;
}

void DeskItem::drag_to(sf::Vector2f center, sf::Vector2f velocity)
{
    m_center = center;

    // Lean into the movement, like paper pushed across a table.
    m_lean = std::clamp(velocity.x * 0.012f, -MAX_LEAN, MAX_LEAN);
}

void DeskItem::click(sf::Vector2f local)
{
    if(m_turn == Turn::WHOLE) turn(m_page + 1 < m_pages.size() ? 1 : -1);
    else if(int const corner = corner_at(local)) turn(corner);
}

void DeskItem::turn(int direction)
{
    if(m_turning) return;

    long const next = static_cast<long>(m_page) + direction;
    if(next < 0 || next >= static_cast<long>(m_pages.size())) return;

    m_next_page = static_cast<std::size_t>(next);
    m_turning   = true;
    m_swapped   = false;
    m_flip.set_instant(0.0f);
    m_flip = 1.0f;
}

void DeskItem::update(float dt)
{
    if(m_turning)
    {
        if(!m_swapped && m_flip.get_value() >= 0.5f)
        {
            m_page    = m_next_page;
            m_swapped = true;
        }
        if(m_flip.is_transition_over()) m_turning = false;
    }

    // Ease toward the lean, and back to flat once let go. Resting pages
    // are always perfectly straight, so their pixel text stays crisp.
    m_angle += (m_lean - m_angle) * std::min(1.0f, dt * 12.0f);
    if(std::abs(m_angle) < 0.05f && m_lean == 0.0f) m_angle = 0.0f;
    m_lean *= std::pow(0.02f, dt);   // a lean fades if the mouse stops

    // Render the current page.
    Frame& page = *m_pages[m_page];
    page.update(sf::seconds(dt));

    m_target.setView(m_target.getDefaultView());
    m_target.clear(sf::Color::Transparent);
    m_target.draw(page);

    // A folded corner wherever there is another page to turn to.
    sf::VertexArray folds{sf::PrimitiveType::Triangles};
    auto fold = [&](bool right, bool hovered)
    {
        float const s = hovered ? FOLD_HOVER : FOLD;
        float const y = m_size.y;
        float const x = right ? m_size.x : 0.0f;
        float const d = right ? -s : s;

        // The flap folded over onto the page, a shade darker...
        sf::Color const paper = page.get_color();
        sf::Color const under{static_cast<std::uint8_t>(paper.r * 0.82f),
                              static_cast<std::uint8_t>(paper.g * 0.80f),
                              static_cast<std::uint8_t>(paper.b * 0.76f)};
        folds.append({{x + d, y}, under});
        folds.append({{x, y - s}, under});
        folds.append({{x + d, y - s}, under});

        // ...and the corner it came from, cut away so the shadow
        // underneath shows through.
        folds.append({{x, y}, sf::Color::Transparent});
        folds.append({{x + d, y}, sf::Color::Transparent});
        folds.append({{x, y - s}, sf::Color::Transparent});
    };

    if(m_turn == Turn::CORNERS)
    {
        if(m_page + 1 < m_pages.size()) fold(true,  m_hover_corner == 1);
        if(m_page > 0)                  fold(false, m_hover_corner == -1);
    }

    if(folds.getVertexCount() > 0)
    {
        // Overwrite rather than blend, so the cut-away corner becomes
        // truly transparent and the desk shows through.
        sf::RenderStates replace;
        replace.blendMode = sf::BlendNone;
        m_target.draw(folds, replace);
    }

    m_target.display();
}

void DeskItem::draw(sf::RenderTarget& target, sf::Shader* paper) const
{
    float const lift = m_lift;
    sf::Transform const t = transform();

    // The shadow falls further away the higher the paper is held.
    sf::Transform shadow;
    shadow.translate(floor_vector({2.0f + lift * 5.0f, 3.0f + lift * 6.0f}));
    shadow *= t;

    sf::RectangleShape shape(m_size);
    shape.setFillColor(sf::Color(SHADOW.r, SHADOW.g, SHADOW.b,
                                 static_cast<std::uint8_t>(SHADOW.a * (0.8f + 0.4f * lift))));
    target.draw(shape, shadow);

    sf::Sprite sprite(m_target.getTexture());
    sf::RenderStates states(t);
    if(paper)
    {
        paper->setUniform("texture", sf::Shader::CurrentTexture);
        paper->setUniform("age", m_age);
        states.shader = paper;
    }
    target.draw(sprite, states);
}

}
