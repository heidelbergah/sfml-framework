/**
 * ShaderManager.hpp
 *
 * Written by James Bouchat
 */

#ifndef SHADER_MANAGER_HPP
#define SHADER_MANAGER_HPP

#include "../Includes.hpp"

class ShaderManager
{
private:
    std::unordered_map<std::string, std::shared_ptr<sf::Shader>> m_shaders;

public:
    ShaderManager();

    void add_shader(std::string key, sf::Shader shader);
    void remove_shader(std::string key);

    std::shared_ptr<sf::Shader> get_shader(std::string key);

};

#endif
