/**
 * ShaderManager.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/framework/ShaderManager.hpp"

sf::Shader* ShaderManager::load_fragment(const std::string& key, const std::string& path)
{
    if(!sf::Shader::isAvailable())
    {
        std::cerr << "[ShaderManager] shaders are not available on this system"
                  << std::endl;
        return nullptr;
    }

    auto shader = std::make_unique<sf::Shader>();
    if(!shader->loadFromFile(path, sf::Shader::Type::Fragment))
    {
        std::cerr << "[ShaderManager] failed to load fragment shader \"" << path
                  << "\"" << std::endl;
        return nullptr;
    }

    sf::Shader* raw  = shader.get();
    m_shaders[key]   = std::move(shader);
    return raw;
}

sf::Shader* ShaderManager::load(const std::string& key,
                                const std::string& vertex_path,
                                const std::string& fragment_path)
{
    if(!sf::Shader::isAvailable())
    {
        std::cerr << "[ShaderManager] shaders are not available on this system"
                  << std::endl;
        return nullptr;
    }

    auto shader = std::make_unique<sf::Shader>();
    if(!shader->loadFromFile(vertex_path, fragment_path))
    {
        std::cerr << "[ShaderManager] failed to load shader pair \"" << vertex_path
                  << "\" + \"" << fragment_path << "\"" << std::endl;
        return nullptr;
    }

    sf::Shader* raw = shader.get();
    m_shaders[key]  = std::move(shader);
    return raw;
}

void ShaderManager::add_shader(const std::string& key, std::unique_ptr<sf::Shader> shader)
{
    if(!shader) return;
    m_shaders[key] = std::move(shader);
}

void ShaderManager::remove_shader(const std::string& key)
{
    m_shaders.erase(key);
}

void ShaderManager::clear()
{
    m_shaders.clear();
}

sf::Shader* ShaderManager::get_shader(const std::string& key) const
{
    auto it = m_shaders.find(key);
    return (it == m_shaders.end()) ? nullptr : it->second.get();
}

bool ShaderManager::has_shader(const std::string& key) const
{
    return m_shaders.find(key) != m_shaders.end();
}

std::size_t ShaderManager::size() const
{
    return m_shaders.size();
}
