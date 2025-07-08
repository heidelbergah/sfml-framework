#include "../../include/framework/ShaderManager.hpp"

ShaderManager::ShaderManager() {}

void ShaderManager::add_shader(std::string key, sf::Shader shader)
{
    std::shared_ptr p_shader = std::make_shared<sf::Shader>(std::move(shader));
    m_shaders[key] = p_shader;
}

void ShaderManager::remove_shader(std::string key)
{
    m_shaders.erase(key);
}

std::shared_ptr<sf::Shader> ShaderManager::get_shader(std::string key)
{
    return m_shaders[key];
}

