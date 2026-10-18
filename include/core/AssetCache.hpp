/**
 * AssetCache.hpp
 *
 * Loads each asset once and hands out references to it.
 *
 * Widgets, sprites and sounds hold non-owning pointers into the cache
 * rather than copies. An sf::Font copy duplicates its glyph atlas in
 * VRAM and an sf::Texture copy is a fresh GPU allocation, so sharing
 * matters as soon as more than one thing uses the same asset.
 *
 *     const sf::Font& font = Assets::fonts().get("assets/fonts/main.ttf");
 *     button->add_text("Play", font, sf::Color::Black);
 *
 * Lifetime rule: the cache must outlive everything that borrowed from
 * it. Assets::* are function-local statics, so they outlive any scene.
 * Do not call clear() while widgets are still holding references.
 */

#ifndef ASSET_CACHE_HPP
#define ASSET_CACHE_HPP

#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

template<typename T>
class AssetCache
{
private:
    // unique_ptr keeps the address stable across rehashes, so references
    // handed out earlier stay valid as the map grows.
    std::unordered_map<std::string, std::unique_ptr<T>> m_assets;

public:
    AssetCache() = default;

    AssetCache(const AssetCache&)            = delete;
    AssetCache& operator=(const AssetCache&) = delete;

    /*
     * Returns the cached asset, loading it on first request.
     * Throws if the file cannot be loaded.
     */
    const T& get(const std::string& path)
    {
        auto it = m_assets.find(path);
        if(it != m_assets.end()) return *it->second;

        auto asset = std::make_unique<T>(path);      // SFML 3 ctors throw on failure
        const T& ref = *asset;
        m_assets.emplace(path, std::move(asset));
        return ref;
    }

    /*
     * Non-throwing variant. Returns nullptr and logs if loading fails,
     * so a missing asset degrades instead of killing the process.
     */
    const T* try_get(const std::string& path) noexcept
    {
        try
        {
            return &get(path);
        }
        catch(const std::exception& e)
        {
            std::cerr << "[AssetCache] failed to load \"" << path << "\": "
                      << e.what() << std::endl;
            return nullptr;
        }
    }

    /*
     * Mutable access, for one-time setup at load such as
     * sf::Font::setSmooth or sf::Texture::setRepeated. Prefer get().
     */
    T& get_mutable(const std::string& path)
    {
        auto it = m_assets.find(path);
        if(it != m_assets.end()) return *it->second;

        auto asset = std::make_unique<T>(path);
        T& ref = *asset;
        m_assets.emplace(path, std::move(asset));
        return ref;
    }

    [[nodiscard]]
    bool contains(const std::string& path) const
    {
        return m_assets.find(path) != m_assets.end();
    }

    [[nodiscard]]
    std::size_t size() const { return m_assets.size(); }

    /*
     * Drops every asset. Anything still holding a reference dangles,
     * so only call this once the owners are gone.
     */
    void clear() { m_assets.clear(); }
};

using TextureCache     = AssetCache<sf::Texture>;
using FontCache        = AssetCache<sf::Font>;
using SoundBufferCache = AssetCache<sf::SoundBuffer>;

/*
 * Shared caches for the whole application. Constructed on first use and
 * destroyed at exit, which puts them safely outside any scene lifetime.
 */
namespace Assets
{
    inline TextureCache& textures()
    {
        static TextureCache cache;
        return cache;
    }

    inline FontCache& fonts()
    {
        static FontCache cache;
        return cache;
    }

    inline SoundBufferCache& sounds()
    {
        static SoundBufferCache cache;
        return cache;
    }

    /*
     * Loads a font with glyph smoothing DISABLED by default.
     *
     * This matters more than it sounds. sf::Font smooths its glyph
     * atlas out of the box, which is right for large vector type and
     * badly wrong for small or pixel fonts, at 12px, on a target that
     * is then scaled up 2x, smoothing is what makes text look muddy.
     *
     * Always prefer this over fonts().get() for in-game text. Pass
     * smooth = true only for large display type.
     */
    inline const sf::Font* font(const std::string& path, bool smooth = false)
    {
        try
        {
            sf::Font& f = fonts().get_mutable(path);
            f.setSmooth(smooth);
            return &f;
        }
        catch(const std::exception& e)
        {
            std::cerr << "[Assets] failed to load font \"" << path << "\": "
                      << e.what() << std::endl;
            return nullptr;
        }
    }

    /*
     * Loads a texture with smoothing disabled, which is what pixel art
     * wants. Returns nullptr on failure.
     */
    inline const sf::Texture* texture(const std::string& path, bool smooth = false)
    {
        try
        {
            sf::Texture& t = textures().get_mutable(path);
            t.setSmooth(smooth);
            return &t;
        }
        catch(const std::exception& e)
        {
            std::cerr << "[Assets] failed to load texture \"" << path << "\": "
                      << e.what() << std::endl;
            return nullptr;
        }
    }
}

#endif
