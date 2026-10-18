/**
 * BloomEffect.hpp
 *
 * Full-screen bloom, applied to a finished frame.
 *
 * Three passes:
 *   1. bright-pass extract + horizontal blur, into ping
 *   2. vertical blur, into pong
 *   3. additive composite of pong over the original
 *
 * Two things this fixes versus the old single-pass shader. The 9x9
 * kernel is separable, so running it as 9 + 9 taps instead of 81 costs
 * about a fifth as much. And the composite adds only the blurred
 * result -- the old `original + bloom` double-counted the centre tap,
 * because the blur already included the source pixel at full weight,
 * which is why bright areas blew out and the threshold had to be set
 * unusually low to compensate.
 *
 * Bloom is also cheaper here because it runs at half resolution: a blur
 * is low-frequency by definition, so the downsample is invisible.
 */

#ifndef BLOOM_EFFECT_HPP
#define BLOOM_EFFECT_HPP

#include "../Includes.hpp"
#include "ShaderManager.hpp"

class BloomEffect
{
private:
    sf::RenderTexture m_ping;
    sf::RenderTexture m_pong;

    sf::Shader* m_extract{nullptr};
    sf::Shader* m_blur{nullptr};

    sf::Vector2u m_size;         // half-resolution working size
    bool         m_ready{false};

    // Deliberately restrained. Bloom that reads as "glowing" on a dark
    // background is usually already strong enough to smear small text.
    float m_threshold{0.80f};
    float m_soft_knee{0.15f};
    float m_intensity{0.45f};

public:
    BloomEffect() = default;

    /*
     * Allocates the working targets and takes non-owning pointers to
     * the shaders. Returns false if either shader is missing, in which
     * case apply() becomes a no-op and the frame draws unbloomed.
     */
    bool initialize(sf::Vector2u target_size, ShaderManager& shaders);

    /*
     * Composites bloom from `source` onto `target`.
     *
     * `source` is normally the same texture that `target` was rendered
     * into, displayed and ready to read.
     */
    void apply(const sf::Texture& source, sf::RenderTarget& target);

    /*
     * Luminance above which a pixel starts to bloom, 0-1.
     */
    void set_threshold(float threshold);

    /*
     * Width of the rolloff above the threshold. Larger is softer.
     */
    void set_soft_knee(float knee);

    /*
     * How strongly the blurred result is added back.
     */
    void set_intensity(float intensity);

    [[nodiscard]] bool is_ready() const;
};

#endif
