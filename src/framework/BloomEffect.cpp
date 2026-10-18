/**
 * BloomEffect.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/framework/BloomEffect.hpp"

bool BloomEffect::initialize(sf::Vector2u target_size, ShaderManager& shaders)
{
    m_ready = false;

    m_extract = shaders.get_shader("bloom_extract");
    m_blur    = shaders.get_shader("bloom_blur_v");

    if(!m_extract || !m_blur)
    {
        std::cerr << "[BloomEffect] shaders missing, bloom disabled" << std::endl;
        return false;
    }

    // Half resolution. A Gaussian blur has no high-frequency detail to
    // lose, so this is a free 4x reduction in fragment work.
    m_size = { std::max(1u, target_size.x / 2u),
               std::max(1u, target_size.y / 2u) };

    if(!m_ping.resize(m_size) || !m_pong.resize(m_size))
    {
        std::cerr << "[BloomEffect] failed to allocate render targets" << std::endl;
        return false;
    }

    m_ping.setSmooth(true);   // smoothing helps the blur here, unlike the
    m_pong.setSmooth(true);   // main pixel-art target

    m_ready = true;
    return true;
}

void BloomEffect::apply(const sf::Texture& source, sf::RenderTarget& target)
{
    if(!m_ready) return;

    sf::Vector2f const texel{1.0f / static_cast<float>(m_size.x),
                             1.0f / static_cast<float>(m_size.y)};

    // Pass 1: bright-pass + horizontal blur, downsampling as it goes.
    {
        sf::Sprite source_sprite(source);

        sf::Vector2u const source_size = source.getSize();
        source_sprite.setScale({static_cast<float>(m_size.x) / static_cast<float>(source_size.x),
                                static_cast<float>(m_size.y) / static_cast<float>(source_size.y)});

        m_extract->setUniform("texture", sf::Shader::CurrentTexture);
        m_extract->setUniform("texel_size", texel);
        m_extract->setUniform("threshold", m_threshold);
        m_extract->setUniform("soft_knee", m_soft_knee);

        m_ping.clear(sf::Color::Black);
        m_ping.draw(source_sprite, m_extract);
        m_ping.display();
    }

    // Pass 2: vertical blur.
    {
        sf::Sprite ping_sprite(m_ping.getTexture());

        m_blur->setUniform("texture", sf::Shader::CurrentTexture);
        m_blur->setUniform("texel_size", texel);
        m_blur->setUniform("intensity", m_intensity);

        m_pong.clear(sf::Color::Black);
        m_pong.draw(ping_sprite, m_blur);
        m_pong.display();
    }

    // Pass 3: additive composite, upscaled back to full size.
    {
        sf::Sprite bloom_sprite(m_pong.getTexture());

        sf::Vector2u const target_size = target.getSize();
        bloom_sprite.setScale({static_cast<float>(target_size.x) / static_cast<float>(m_size.x),
                               static_cast<float>(target_size.y) / static_cast<float>(m_size.y)});

        // Only the blurred contribution is added. The original is
        // already in the target.
        target.draw(bloom_sprite, sf::BlendAdd);
    }
}

void BloomEffect::set_threshold(float threshold)
{
    m_threshold = std::clamp(threshold, 0.0f, 1.0f);
}

void BloomEffect::set_soft_knee(float knee)
{
    m_soft_knee = std::max(0.001f, knee);
}

void BloomEffect::set_intensity(float intensity)
{
    m_intensity = std::max(0.0f, intensity);
}

bool BloomEffect::is_ready() const
{
    return m_ready;
}
