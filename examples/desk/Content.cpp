/**
 * Content.cpp  -  Desk
 */

#include "Content.hpp"

#include "Widgets.hpp"

#include <cmath>

namespace desk
{

namespace
{
    constexpr unsigned BODY  = 16;
    constexpr unsigned LARGE = 32;

    const sf::Color NEWSPRINT {222, 213, 190};
    const sf::Color INK       {38,  32,  36};
    const sf::Color INK_SOFT  {98,  88,  86};

    const sf::Color LETTER_PAPER{232, 225, 206};
    const sf::Color LETTER_INK  {46,  58,  96};
    const sf::Color RULED       {170, 186, 210};
    const sf::Color MARGIN_LINE {206, 140, 132};

    const sf::Color PHOTO_BORDER{236, 230, 216};
    const sf::Color PHOTO_BACK  {214, 206, 188};
    const sf::Color PENCIL      {92,  88,  92};

    const sf::Color NOTE_PAPER{228, 206, 120};
    const sf::Color NOTE_INK  {64,  52,  40};

    // Four-tone sepia, darkest first.
    const sf::Color SEPIA[4] = {{54, 40, 32}, {112, 86, 62}, {170, 140, 104}, {218, 198, 160}};

    constexpr float BAYER[4][4] = {
        { 0.0f / 16,  8.0f / 16,  2.0f / 16, 10.0f / 16},
        {12.0f / 16,  4.0f / 16, 14.0f / 16,  6.0f / 16},
        { 3.0f / 16, 11.0f / 16,  1.0f / 16,  9.0f / 16},
        {15.0f / 16,  7.0f / 16, 13.0f / 16,  5.0f / 16},
    };

    float threshold(int x, int y) { return BAYER[y & 3][x & 3]; }

    /*
     * Paints an image one pixel at a time from a function giving a
     * brightness level (0 dark to 1 light) and a palette of colours from
     * dark to light, dithering between neighbouring colours. Horizontal
     * runs of one colour merge into a single rectangle.
     */
    void paint(sf::VertexArray& out, sf::Vector2f size,
               const std::function<float(float, float)>& level,
               const std::vector<sf::Color>& palette)
    {
        int const w = static_cast<int>(size.x);
        int const h = static_cast<int>(size.y);
        float const steps = static_cast<float>(palette.size() - 1);

        for(int y = 0; y < h; ++y)
        {
            int run_start = 0;
            int run_color = -1;

            for(int x = 0; x <= w; ++x)
            {
                int color = -1;
                if(x < w)
                {
                    float const v     = std::clamp(level(float(x), float(y)), 0.0f, 1.0f) * steps;
                    float const base  = std::floor(v);
                    int const   index = static_cast<int>(base) + ((v - base) > threshold(x, y) ? 1 : 0);
                    color = std::min(index, static_cast<int>(steps));
                }

                if(color != run_color)
                {
                    if(run_color >= 0 && palette[static_cast<std::size_t>(run_color)].a > 0)
                        push_rect(out, {{float(run_start), float(y)}, {float(x - run_start), 1.0f}},
                                  palette[static_cast<std::size_t>(run_color)]);
                    run_start = x;
                    run_color = color;
                }
            }
        }
    }

    // Mountain ridges, in pixels from the top, for a picture `w` wide.
    float far_ridge(float x, float h)  { return h * 0.36f + std::sin(x * 0.045f) * 7.0f + std::sin(x * 0.13f + 1.0f) * 4.0f; }
    float near_ridge(float x, float h) { return h * 0.56f + std::sin(x * 0.06f + 2.0f) * 6.0f + std::sin(x * 0.17f) * 3.0f; }

    /*
     * Lays widgets out top to bottom inside a page.
     */
    class Column
    {
    private:
        Frame& m_frame;
        float  m_x;
        float  m_y;
        float  m_width;

        // Widget keys must be unique within a Frame -- adding a second
        // widget under an existing key silently replaces the first --
        // and a page can be built from more than one Column.
        static std::string next_key()
        {
            static int count = 0;
            return "w" + std::to_string(count++);
        }

    public:
        Column(Frame& frame, float x, float y, float width)
            : m_frame(frame), m_x(x), m_y(y), m_width(width)
        {}

        void text(const std::string& s, unsigned size, sf::Color color,
                  bool centred = false, float gap = 6.0f, float spacing = 1.0f)
        {
            auto p = std::make_shared<Paragraph>(m_width, s, font(), size, color, spacing, centred);
            p->set_position({m_x, m_y}, true);
            p->set_z_value(1);             // text above pictures and rules
            m_frame.add_widget(next_key(), p);
            m_y += p->get_size().y + gap;
        }

        void rule(float thickness, sf::Color color, float gap = 6.0f)
        {
            auto line = std::make_shared<Picture>(sf::Vector2f{m_width, thickness},
                [color](sf::VertexArray& v, sf::Vector2f s) { push_rect(v, {{0, 0}, s}, color); });
            line->set_position({m_x, m_y}, true);
            m_frame.add_widget(next_key(), line);
            m_y += thickness + gap;
        }

        void picture(float height, const Picture::Builder& build, float gap = 4.0f)
        {
            auto pic = std::make_shared<Picture>(sf::Vector2f{m_width, height}, build);
            pic->set_position({m_x, m_y}, true);
            m_frame.add_widget(next_key(), pic);
            m_y += height + gap;
        }

        void gap(float amount) { m_y += amount; }
    };

    std::shared_ptr<Frame> page(sf::Vector2f size, sf::Color paper)
    {
        auto frame = std::make_shared<Frame>(size, paper, font());
        return frame;
    }

    // ------------------------------------------------------- pictures

    /*
     * Newsprint halftone: ink dots on paper, densest where it is dark.
     */
    void tanks_at_the_pass(sf::VertexArray& v, sf::Vector2f size)
    {
        float const h = size.y;
        paint(v, size, [h](float x, float y)
        {
            // Three small tanks on the road, solid ink.
            for(float tx : {58.0f, 128.0f, 196.0f})
            {
                float const ty = h * 0.80f;
                bool const hull   = x > tx - 11 && x < tx + 11 && y > ty - 4 && y < ty + 2;
                bool const turret = x > tx - 5  && x < tx + 4  && y > ty - 8 && y < ty - 3;
                bool const barrel = x > tx + 3  && x < tx + 15 && y > ty - 7 && y < ty - 5;
                if(hull || turret || barrel) return 0.0f;
            }

            if(y > h * 0.84f) return 0.55f;                                  // road
            if(y > h * 0.76f) return 0.72f;                                  // verge
            if(y > near_ridge(x, h)) return 0.36f;                           // near slopes
            if(y > far_ridge(x, h))  return 0.62f;                           // far peaks
            return 0.92f - y / h * 0.25f;                                    // sky
        }, {INK, NEWSPRINT});
    }

    void smoke_over_the_pass(sf::VertexArray& v, sf::Vector2f size)
    {
        float const h = size.y;
        paint(v, size, [h](float x, float y)
        {
            // Columns of smoke behind the ridge, widening as they rise.
            for(float sx : {84.0f, 170.0f, 214.0f})
            {
                float const rise  = std::clamp((near_ridge(sx, h) - y) / h, 0.0f, 1.0f);
                float const drift = std::sin(y * 0.16f + sx) * 3.0f + rise * 14.0f;
                float const width = 3.0f + rise * 16.0f;
                if(y < near_ridge(sx, h) && std::abs(x - sx - drift) < width)
                    return 0.18f + rise * 0.3f;
            }

            if(y > h * 0.80f) return 0.48f;
            if(y > near_ridge(x, h)) return 0.30f;
            if(y > far_ridge(x, h))  return 0.56f;
            return 0.78f - y / h * 0.2f;
        }, {INK, NEWSPRINT});
    }

    /*
     * A small sepia snapshot: mountains, a field, a tractor and two
     * people standing beside it.
     */
    void summer_photo(sf::VertexArray& v, sf::Vector2f size)
    {
        float const w = size.x;
        float const h = size.y;
        std::vector<sf::Color> tones(std::begin(SEPIA), std::end(SEPIA));

        paint(v, size, [w, h](float x, float y)
        {
            // Two figures, left of centre.
            auto figure = [&](float fx, float tall)
            {
                float const top = h * 0.80f - tall;
                bool const head = std::hypot(x - fx, y - (top + 3.0f)) < 3.0f;
                bool const body = x > fx - 3 && x < fx + 3 && y > top + 5 && y < h * 0.80f;
                return head || body;
            };
            if(figure(w * 0.30f, 26.0f) || figure(w * 0.38f, 23.0f)) return 0.02f;

            // The tractor.
            float const tx = w * 0.62f, ty = h * 0.80f;
            if(std::hypot(x - (tx - 8), y - (ty - 5)) < 7.0f) return 0.05f;    // big wheel
            if(std::hypot(x - (tx + 10), y - (ty - 3)) < 4.0f) return 0.05f;   // small wheel
            if(x > tx - 10 && x < tx + 12 && y > ty - 14 && y < ty - 6) return 0.22f;
            if(x > tx - 6  && x < tx       && y > ty - 22 && y < ty - 14) return 0.22f;

            if(y > h * 0.80f) return 0.48f + std::sin(x * 0.9f + y) * 0.04f;   // field
            if(y > near_ridge(x * 1.6f, h)) return 0.40f;
            if(y > far_ridge(x * 1.6f, h) - 6.0f) return 0.66f;
            return 0.92f - y / h * 0.18f;                                       // sky
        }, tones);
    }

    // ------------------------------------------------------ masthead

    void masthead(Column& c, const std::string& dateline)
    {
        c.text("THE NIXICUS HERALD", LARGE, INK, true, 2.0f);
        c.rule(2.0f, INK, 3.0f);
        c.text(dateline, BODY, INK_SOFT, true, 2.0f);
        c.rule(1.0f, INK, 8.0f);
    }
}

const sf::Font& font()
{
    static const sf::Font* f = Assets::font("assets/fonts/pixel-font-bitmap.ttf");
    return *f;
}

// =========================================================== newspapers

std::vector<std::shared_ptr<Frame>> early_edition()
{
    float const margin = 14.0f;
    float const width  = NEWSPAPER_SIZE.x - margin * 2.0f;

    auto front = page(NEWSPAPER_SIZE, NEWSPRINT);
    {
        Column c(*front, margin, 10.0f, width);
        masthead(c, "Spring, 921   |   Two pennies");
        c.text("LINE HOLDS AT THE EASTERN PASS", LARGE, INK, true, 8.0f, 0.9f);
        c.picture(78.0f, tanks_at_the_pass);
        c.text("Monteran armour moving up to the passes.", BODY, INK_SOFT, false, 8.0f);
        c.text("Rechivii forces were turned back at the eastern passes on Tuesday "
               "after two days of fighting. Officers at the front describe morale "
               "as high.", BODY, INK);
    }

    auto home = page(NEWSPAPER_SIZE, NEWSPRINT);
    {
        Column c(*home, margin, 12.0f, width);
        c.text("AT HOME", LARGE, INK, true, 2.0f);
        c.rule(1.0f, INK, 8.0f);
        c.text("Rail yard works through the night", BODY, INK, false, 4.0f);
        c.text("Volunteers at the Nixicus yard loaded eleven trains last week, most "
               "of them by lamplight. The foreman asks that tea be left at the north "
               "gate, and promises it will be drunk.", BODY, INK_SOFT, false, 10.0f);
        c.rule(1.0f, INK, 6.0f);
        c.text("THE CITY HOSPITAL asks for volunteers. Medical students most "
               "welcome. Ask for the night sister.", BODY, INK, false, 6.0f);
        c.rule(1.0f, INK, 10.0f);
        c.text("NOTICES", BODY, INK, false, 4.0f);
        c.text("For sale: two goats, good temper, one opinion. Ask at the mill.\n"
               "Lost: grey cat answering to Mud. Small reward.", BODY, INK_SOFT);
    }

    auto back = page(NEWSPAPER_SIZE, NEWSPRINT);
    {
        Column c(*back, margin, 12.0f, width);
        c.text("WEATHER", LARGE, INK, true, 2.0f);
        c.rule(1.0f, INK, 8.0f);
        c.text("Snow on the high passes. Mild in the valleys, with rain by the "
               "weekend.", BODY, INK_SOFT, false, 14.0f);
        c.text("LETTERS", LARGE, INK, true, 2.0f);
        c.rule(1.0f, INK, 8.0f);
        c.text("Sir, my son writes that the food at the front is better than mine. "
               "I do not believe him, and I have told him so.", BODY, INK, false, 4.0f);
        c.text("A mother in Nixicus", BODY, INK_SOFT, false, 12.0f);
        c.text("Sir, the mountains have kept us before. They will keep us again.",
               BODY, INK, false, 4.0f);
        c.text("A farmer, Upper Valley", BODY, INK_SOFT);
    }

    return {front, home, back};
}

std::vector<std::shared_ptr<Frame>> late_edition()
{
    float const margin = 14.0f;
    float const width  = NEWSPAPER_SIZE.x - margin * 2.0f;

    // Words in {braces} reach the reader as a censor's black bar.
    auto front = page(NEWSPAPER_SIZE, NEWSPRINT);
    {
        Column c(*front, margin, 10.0f, width);
        masthead(c, "Winter, 921   |   Four pennies");
        c.text("VICTORY NEAR, SAYS MINISTRY", LARGE, INK, true, 8.0f, 0.9f);
        c.picture(78.0f, smoke_over_the_pass);
        c.text("Our forces, in good order.", BODY, INK_SOFT, false, 8.0f);
        c.text("The Ministry confirmed on Friday that our forces have {withdrawn from "
               "the eastern passes} to stronger positions near {the capital}. Losses "
               "were described as {heavy} acceptable.", BODY, INK);
    }

    auto notices = page(NEWSPAPER_SIZE, NEWSPRINT);
    {
        Column c(*notices, margin, 12.0f, width);
        c.text("NOTICES", LARGE, INK, true, 2.0f);
        c.rule(1.0f, INK, 8.0f);
        c.text("Flour is rationed from Monday. Bring your card.", BODY, INK, false, 10.0f);
        c.text("The following crews are reported missing:", BODY, INK, false, 4.0f);
        c.text("{Sergeant Varo and crew}\n{Corporal Hemm and crew}\n{Private Ostrin}\n"
               "{Corporal Tarre and crew}", BODY, INK, false, 10.0f);
        c.text("Citizens are reminded that rumour costs lives.", BODY, INK_SOFT, false, 14.0f);
        c.rule(1.0f, INK, 8.0f);
        c.text("Lost: grey cat answering to Mud. Still looking.", BODY, INK_SOFT);
    }

    return {front, notices};
}

// ================================================================ letter

std::vector<std::shared_ptr<Frame>> letter()
{
    float const left    = 30.0f;
    float const top     = 26.0f;
    float const width   = LETTER_SIZE.x - left - 14.0f;
    float const advance = std::floor(font().getLineSpacing(BODY));

    // Ruled lines matched to the font's line height, so the writing sits on them.
    auto ruled = [advance, top](sf::VertexArray& v, sf::Vector2f size)
    {
        for(float y = top + advance - 3.0f; y < size.y - 8.0f; y += advance)
            push_rect(v, {{0.0f, y}, {size.x, 1.0f}}, RULED);
        push_rect(v, {{22.0f, 0.0f}, {1.0f, size.y}}, MARGIN_LINE);
    };

    auto make = [&](const std::string& text)
    {
        auto sheet = page(LETTER_SIZE, LETTER_PAPER);
        Column lines(*sheet, 0.0f, 0.0f, LETTER_SIZE.x);
        lines.picture(LETTER_SIZE.y, ruled, 0.0f);

        Column words(*sheet, left, top, width);
        words.text(text, BODY, LETTER_INK);
        return sheet;
    };

    return {
        make("My love,\n\n"
             "The hospital has me on nights now. I fall asleep over my anatomy books "
             "and dream about that farm of yours, the one with the crooked fence.\n\n"
             "Mother asks after you every Sunday."),
        make("Please write when you can, even if it is only a line. I keep your last "
             "letter in my coat.\n\n"
             "Come home in one piece. The goats will need a mechanic.\n\n"
             "Yours, always"),
    };
}

// ============================================================ photograph

std::vector<std::shared_ptr<Frame>> photograph()
{
    float const border = 6.0f;

    auto front = page(PHOTO_SIZE, PHOTO_BORDER);
    {
        auto image = std::make_shared<Picture>(
            sf::Vector2f{PHOTO_SIZE.x - border * 2.0f, PHOTO_SIZE.y - border * 2.0f - 6.0f},
            summer_photo);
        image->set_position({border, border}, true);
        front->add_widget("image", image);
    }

    auto back = page(PHOTO_SIZE, PHOTO_BACK);
    {
        Column c(*back, 10.0f, 10.0f, PHOTO_SIZE.x - 20.0f);
        c.text("Nixicus,\nsummer 920.\n\nThe day you fixed\nher father's tractor.",
               BODY, PENCIL);
    }

    return {front, back};
}

// =========================================================== sticky note

std::vector<std::shared_ptr<Frame>> sticky_note()
{
    auto note = page(NOTE_SIZE, NOTE_PAPER);
    Column c(*note, 10.0f, 8.0f, NOTE_SIZE.x - 18.0f);
    c.text("Drag papers around.\nClick a folded corner to turn the page.\n"
           "Click the photo to turn it over.", BODY, NOTE_INK, false, 0.0f, 0.95f);
    return {note};
}

}
