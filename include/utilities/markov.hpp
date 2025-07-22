/**
 * markov.hpp
 *
 * Name generator utilizing markov models.
 *
 * Given a list of names, a markov model will
 * return a name of a similar "style" of the names
 * provided in the list provided.
 *
 * Written by James Bouchat
 */

#ifndef MARKOV_HPP
#define MARKOV_HPP

#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>
#include <random>

class Markov
{
private:
    std::unordered_map<std::string, std::vector<char>> m_map;
    std::ifstream m_input_file;
    unsigned m_order;

public:
    Markov(std::string input_file_name, unsigned order);

    void change_order(unsigned order);

    void update_base_names(std::string input_file_name);

    std::string generate_name();

};

#endif
