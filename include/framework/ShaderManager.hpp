/**
 * ShaderManager.hpp
 *
 * Keyed collection of shaders.
 *
 * get_shader() returns nullptr for an unknown key rather than
 * default-constructing an entry. The old version used
 * map::operator[], so a mistyped key silently inserted a null
 * shared_ptr and grew the map on every miss.
 *
 * A null return is safe to hand straight to SFML, sf::RenderTarget::draw
 * treats a null shader as "no shader", but check it if the effect is
 * required rather than optional.
 */

#ifndef SHADER_MANAGER_HPP
#define SHADER_MANAGER_HPP

#include "../Includes.hpp"

class ShaderManager
{
private:
    std::unordered_map<std::string, std::unique_ptr<sf::Shader>> m_shaders;

public:
    ShaderManager() = default;

    ShaderManager(const ShaderManager&)            = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    /*
     * Loads a fragment shader. Returns nullptr and logs on failure, so
     * a broken shader degrades to an unshaded draw instead of throwing.
     */
    sf::Shader* load_fragment(const std::string& key, const std::string& path);

    /*
     * Loads a vertex + fragment pair.
     */
    sf::Shader* load(const std::string& key,
                     const std::string& vertex_path,
                     const std::string& fragment_path);

    void add_shader(const std::string& key, std::unique_ptr<sf::Shader> shader);
    void remove_shader(const std::string& key);
    void clear();

    /*
     * Non-owning. Valid until the shader is removed or the manager dies.
     */
    [[nodiscard]] sf::Shader* get_shader(const std::string& key) const;

    [[nodiscard]] bool        has_shader(const std::string& key) const;
    [[nodiscard]] std::size_t size() const;
};

#endif
