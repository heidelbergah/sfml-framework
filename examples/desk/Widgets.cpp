/**
 * Widgets.cpp  -  Desk
 */

#include "Widgets.hpp"

namespace desk
{

namespace
{
    struct Range
    {
        std::size_t begin;
        std::size_t end;   // one past the last character
    };

    /*
     * Strips {braces} out of `source`, remembering where they were.
     */
    std::string strip_censored(const std::string& source, std::vector<Range>& ranges)
    {
        std::string clean;
        std::size_t open = std::string::npos;

        for(char c : source)
        {
            if(c == '{')      { open = clean.size(); continue; }
            if(c == '}' && open != std::string::npos)
            {
                ranges.push_back({open, clean.size()});
                open = std::string::npos;
                continue;
            }
            clean += c;
        }
        return clean;
    }

    /*
     * Greedy word wrap. Only ever turns a space into a newline, so every
     * character keeps its index -- which is what lets censored ranges
     * found before wrapping still line up afterwards.
     */
    std::string wrap(std::string text, const sf::Font& font, unsigned size, float width)
    {
        sf::Text probe(font, "", size);

        std::size_t line_start = 0;
        std::size_t last_space = std::string::npos;

        for(std::size_t i = 0; i <= text.size(); ++i)
        {
            bool const at_end     = (i == text.size());
            char const c          = at_end ? '\n' : text[i];
            bool const line_break = (c == '\n');

            if(c != ' ' && !line_break) continue;

            // Measure the line so far at every space, and also at a line
            // break or the end -- otherwise the last word before a
            // paragraph break is never checked and can run off the page.
            probe.setString(text.substr(line_start, i - line_start));
            if(probe.getLocalBounds().size.x > width && last_space != std::string::npos)
            {
                text[last_space] = '\n';
                line_start = last_space + 1;
            }

            if(line_break)
            {
                line_start = i + 1;
                last_space = std::string::npos;
            }
            else
            {
                last_space = i;
            }
        }
        return text;
    }
}

Paragraph::Paragraph(float width, const std::string& text, const sf::Font& font,
                     unsigned size, sf::Color color, float line_spacing, bool centred)
    : Widget({width, 0.0f})
{
    set_transition(TransitionFunction::None);
    m_background.setFillColor(sf::Color::Transparent);

    std::vector<Range> censored;
    std::string const wrapped = wrap(strip_censored(text, censored), font, size, width);

    auto is_censored = [&censored](std::size_t i)
    {
        for(const Range& r : censored)
            if(i >= r.begin && i < r.end) return true;
        return false;
    };

    // One sf::Text per line, so lines can be centred and censored
    // characters measured line by line.
    float const advance = std::floor(font.getLineSpacing(size) * line_spacing);
    float       y       = 0.0f;
    std::size_t start   = 0;

    while(start <= wrapped.size())
    {
        std::size_t end = wrapped.find('\n', start);
        if(end == std::string::npos) end = wrapped.size();

        std::string const line = wrapped.substr(start, end - start);

        sf::Text measure(font, line, size);
        float const top = std::floor(font.getLineSpacing(size) - static_cast<float>(size));
        float const x   = centred ? std::floor((width - measure.getLocalBounds().size.x) / 2.0f)
                                  : 0.0f;
        measure.setPosition({x, y - top});

        // Walk the line in runs. Visible runs become their own text,
        // placed exactly where they fall in the full line; censored runs
        // become bars. Blanking censored letters with spaces instead
        // would shift everything after them, because a space is
        // narrower than most letters.
        for(std::size_t i = 0; i < line.size();)
        {
            bool const hidden = is_censored(start + i);
            std::size_t j = i;
            while(j < line.size() && is_censored(start + j) == hidden) ++j;

            sf::Vector2f const from = measure.findCharacterPos(i);
            sf::Vector2f const to   = measure.findCharacterPos(j);

            if(hidden)
            {
                push_rect(m_bars, {floor_vector({from.x - 1.0f, y + 2.0f}),
                                   {std::ceil(to.x - from.x) + 1.0f, static_cast<float>(size) - 3.0f}},
                          color);
            }
            else
            {
                sf::Text run(font, line.substr(i, j - i), size);
                run.setFillColor(color);
                run.setPosition({std::floor(from.x), y - top});
                m_lines.push_back(run);
            }
            i = j;
        }

        y += advance;
        if(end == wrapped.size()) break;
        start = end + 1;
    }

    set_size({width, y});
    m_background.setFillColor(sf::Color::Transparent);
}

void Paragraph::on_draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    for(const sf::Text& line : m_lines) target.draw(line, states);
    if(m_bars.getVertexCount() > 0) target.draw(m_bars, states);
}

Picture::Picture(sf::Vector2f size, const Builder& build)
    : Widget(size)
{
    set_transition(TransitionFunction::None);
    build(m_vertices, size);
}

void Picture::on_draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(m_vertices, states);
}

void push_rect(sf::VertexArray& vertices, sf::FloatRect rect, sf::Color color)
{
    sf::Vector2f const a = rect.position;
    sf::Vector2f const b = {rect.position.x + rect.size.x, rect.position.y};
    sf::Vector2f const c = rect.position + rect.size;
    sf::Vector2f const d = {rect.position.x, rect.position.y + rect.size.y};

    for(sf::Vector2f p : {a, b, c, a, c, d})
        vertices.append(sf::Vertex{p, color});
}

}
