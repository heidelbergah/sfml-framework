/**
 * ShaderView.cpp  -  Showcase
 */

#include "ShaderView.hpp"

#include "Theme.hpp"

namespace showcase
{

ShaderView::ShaderView(Effect& effect, ShaderManager& shaders)
    : Widget(sf::Vector2f(effect.size())), m_effect(&effect)
{
    set_transition(TransitionFunction::None);

    if(!sf::Shader::isAvailable())
        m_failure = "shaders are not\nsupported here";
    else if(!m_target.resize(effect.size()))
        m_failure = "could not create\na render texture";
    else if(!effect.load(shaders))
        m_failure = "shader failed\nto load";
    else
        m_ready = true;

    m_target.setSmooth(false);
}

void ShaderView::begin_frame()
{
    m_pointer.pressed = false;
}

void ShaderView::update(sf::Time delta_time)
{
    if(!m_ready)
    {
        // Still draw something, so a broken shader is a message in a
        // window rather than an empty hole.
        if(m_target.getSize().x == 0) return;

        m_target.clear(INK);
        sf::Text text(*font(), m_failure, TEXT_SIZE);
        text.setFillColor(TEXT_DIM);
        text.setPosition({10.0f, 8.0f});
        m_target.draw(text);
        m_target.display();
        return;
    }

    m_effect->update(delta_time.asSeconds(), m_pointer);

    m_target.setView(m_target.getDefaultView());
    m_target.clear(INK);
    m_effect->render(m_target);
    m_target.display();
}

void ShaderView::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    // mouse_pos is in the window's local space; ours starts at our own
    // position inside it.
    sf::Vector2f const local = mouse_pos - floor_vector(get_position());

    if(event.is<sf::Event::MouseMoved>())
    {
        m_pointer.pos    = local;
        m_pointer.inside = contains(mouse_pos);
    }
    else if(const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        m_pointer.pos    = local;
        m_pointer.inside = contains(mouse_pos);
        if(pressed->button == sf::Mouse::Button::Left && m_pointer.inside)
        {
            m_pointer.down    = true;
            m_pointer.pressed = true;
        }
    }
    else if(const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if(released->button == sf::Mouse::Button::Left) m_pointer.down = false;
    }
}

void ShaderView::on_draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    sf::Sprite sprite(m_target.getTexture());
    target.draw(sprite, states);
}

void draw_quad(sf::RenderTarget& target, sf::Vector2u size, const sf::Shader* shader)
{
    float const w = static_cast<float>(size.x);
    float const h = static_cast<float>(size.y);

    sf::Vertex const quad[] = {
        {{0.0f, 0.0f}, sf::Color::White, {0.0f, 0.0f}},
        {{w,    0.0f}, sf::Color::White, {1.0f, 0.0f}},
        {{0.0f, h},    sf::Color::White, {0.0f, 1.0f}},
        {{w,    h},    sf::Color::White, {1.0f, 1.0f}},
    };

    sf::RenderStates states;
    states.shader = shader;
    target.draw(quad, 4, sf::PrimitiveType::TriangleStrip, states);
}

void set_ramp(sf::Shader& shader, sf::Color c0, sf::Color c1, sf::Color c2, sf::Color c3)
{
    shader.setUniform("c0", sf::Glsl::Vec4(c0));
    shader.setUniform("c1", sf::Glsl::Vec4(c1));
    shader.setUniform("c2", sf::Glsl::Vec4(c2));
    shader.setUniform("c3", sf::Glsl::Vec4(c3));
}

}
