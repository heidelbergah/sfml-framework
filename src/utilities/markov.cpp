/**
 * markov.cpp
 *
 * Written by James Bouchat
 */

#include "../../include/utilities/markov.hpp"
#include "../../include/core/Random.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>

namespace
{
    constexpr char PAD        = '_';
    constexpr char TERMINATOR = '*';

    /*
     * Trims trailing CR, so a file saved on Windows does not train the
     * model to emit carriage returns.
     */
    void strip_line_ending(std::string& line)
    {
        while(!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
    }
}

Markov::Markov(const std::string& input_file_name, unsigned order)
    : m_order(std::max(1u, order))
{
    // No srand() here -- see Random.hpp.
    update_base_names(input_file_name);
}

bool Markov::update_base_names(const std::string& input_file_name)
{
    // Local stream rather than a member. The old version kept the
    // ifstream as a member and never closed it, so a second call opened
    // an already-open stream, failed silently, and left the model empty.
    std::ifstream input_file(input_file_name);

    if(!input_file.is_open())
    {
        std::cerr << "[Markov] could not open \"" << input_file_name << "\"" << std::endl;
        return false;
    }

    std::vector<std::string> names;
    std::string line;

    while(std::getline(input_file, line))
    {
        strip_line_ending(line);
        if(!line.empty()) names.push_back(line);
    }

    set_base_names(names);
    return true;
}

void Markov::set_base_names(const std::vector<std::string>& names)
{
    m_source_names = names;
    rebuild_model();
}

void Markov::rebuild_model()
{
    m_map.clear();

    for(const std::string& name : m_source_names)
    {
        if(name.empty()) continue;

        std::string const padded = std::string(m_order, PAD) + name + TERMINATOR;

        /*
         * Both operands are size_t. The old loop condition was
         * `i < name.size() - m_order`, so any line shorter than the
         * order wrapped to about four billion and walked substr off
         * the end of the string. Padding guarantees padded.size() is
         * always greater than m_order, and the comparison is written to
         * avoid the subtraction entirely.
         */
        for(std::size_t i = 0; i + m_order < padded.size(); ++i)
        {
            m_map[padded.substr(i, m_order)].push_back(padded[i + m_order]);
        }
    }
}

void Markov::change_order(unsigned order)
{
    m_order = std::max(1u, order);
    rebuild_model();
}

void Markov::set_max_length(std::size_t max_length)
{
    m_max_length = std::max<std::size_t>(1, max_length);
}

std::string Markov::generate_name()
{
    if(m_map.empty()) return {};

    std::string name(m_order, PAD);

    // Hard cap on iterations. Without it, a model that can never reach
    // a terminator loops until it exhausts memory.
    std::size_t const limit = m_max_length + m_order;

    while(name.size() < limit)
    {
        std::string const prefix = name.substr(name.size() - m_order, m_order);

        // find() rather than operator[], and a reference rather than a
        // copy -- the old version copied the whole candidate vector for
        // every single character generated, and inserted an empty entry
        // into the map for every prefix it had never seen.
        auto it = m_map.find(prefix);
        if(it == m_map.end() || it->second.empty()) break;

        char const next = Random::pick(it->second);
        if(next == TERMINATOR) break;

        name += next;
    }

    return name.substr(m_order);
}

std::string Markov::generate_name(std::size_t min_length, std::size_t max_length,
                                  int attempts)
{
    std::string best;

    for(int i = 0; i < attempts; ++i)
    {
        std::string candidate = generate_name();
        if(candidate.empty()) return {};

        if(candidate.size() >= min_length && candidate.size() <= max_length)
            return candidate;

        // Keep the closest miss, so we always return something usable.
        if(best.empty() || std::abs(static_cast<long>(candidate.size() - min_length)) <
                           std::abs(static_cast<long>(best.size() - min_length)))
        {
            best = candidate;
        }
    }

    return best;
}

bool Markov::is_trained() const
{
    return !m_map.empty();
}

std::size_t Markov::corpus_size() const
{
    return m_source_names.size();
}

unsigned Markov::get_order() const
{
    return m_order;
}
