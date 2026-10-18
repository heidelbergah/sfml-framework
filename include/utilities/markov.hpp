/**
 * markov.hpp
 *
 * Name generator built on a character-level Markov chain.
 *
 * Give it a file of example names, one per line, and it produces new
 * names in the same style. Higher orders stay closer to the source
 * material; order 2 or 3 is usually the sweet spot for names.
 *
 * Written by James Bouchat
 */

#ifndef MARKOV_HPP
#define MARKOV_HPP

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class Markov
{
private:
    std::unordered_map<std::string, std::vector<char>> m_map;
    std::vector<std::string> m_source_names;

    unsigned    m_order{2};
    std::size_t m_max_length{64};

    void rebuild_model();

public:
    /*
     * Loads and trains in one step. Order must be at least 1.
     */
    Markov(const std::string& input_file_name, unsigned order);

    /*
     * Retrains on the names already loaded.
     */
    void change_order(unsigned order);

    /*
     * Replaces the corpus and retrains. Returns false if the file could
     * not be opened.
     */
    bool update_base_names(const std::string& input_file_name);

    /*
     * Trains directly from memory, no file involved.
     */
    void set_base_names(const std::vector<std::string>& names);

    /*
     * Longest name to generate before giving up. Guards against a model
     * that can never reach a terminator.
     */
    void set_max_length(std::size_t max_length);

    /*
     * Empty if the model has no training data.
     */
    [[nodiscard]] std::string generate_name();

    /*
     * Generates until the result is within the length bounds, or the
     * attempt budget runs out.
     */
    [[nodiscard]] std::string generate_name(std::size_t min_length,
                                            std::size_t max_length,
                                            int attempts = 32);

    [[nodiscard]] bool        is_trained() const;
    [[nodiscard]] std::size_t corpus_size() const;
    [[nodiscard]] unsigned    get_order() const;
};

#endif
