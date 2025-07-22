/**
 * markov.cpp
 *
 * Written by James Bouchat
 *
 */

#include "../../include/utilities/markov.hpp"

Markov::Markov(std::string input_file_name, unsigned order)
{
    srand(time(NULL));
    m_order = order;
    update_base_names(input_file_name);
}

void Markov::change_order(unsigned order)
{
    m_order = order;
}

void Markov::update_base_names(std::string input_file_name)
{
    std::vector<std::string> names;

    m_input_file.open(input_file_name);

    std::string line;
    while(std::getline(m_input_file, line))
    {
        std::string prefix(m_order, '_');
        line = prefix + line + "*";
        names.push_back(line);
    }

    for(std::string name : names)
    {
        for(int i = 0; i < name.size()-m_order; ++i)
        {
            std::string substring = name.substr(i, m_order);
            m_map[substring].push_back(name[i+m_order]);
        }
    }
}

std::string Markov::generate_name()
{
    std::string name(m_order, '_');

    int idx = 0;
    while(name[name.size()-1] != '*')
    {
        std::string substring = name.substr(idx, m_order);
    
        std::vector<char> list = m_map[substring];
        
        char result;

        if(list.size() != 0)
            result = list[rand() % list.size()];
        else
            result = '*';

        name += result;

        idx++;
    }

    return name.substr(m_order, name.size()-(m_order+1));

}

