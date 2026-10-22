/**
 * Level.cpp  -  Cinder
 */

#include "Level.hpp"

namespace cinder
{

namespace
{
    sf::FloatRect R(float x, float y, float w, float h) { return {{x, y}, {w, h}}; }

    LevelDef hearth()
    {
        LevelDef L;
        L.name  = "Hearth";
        L.line  = "three old fires, gone out";
        L.size  = {1280.0f, 720.0f};
        L.start = {140.0f, 380.0f};
        L.seed  = 11;
        L.drain = 0.016f;

        L.walls = {
            R(560, 110, 24, 190), R(560, 390, 24, 130),       // a broken wall with a gap
            R(860, 590, 220, 22), R(300, 590, 140, 22),
            R(950, 130, 22, 110),
            R(250, 200, 24, 24),  R(910, 410, 24, 24), R(1160, 520, 24, 24),
        };
        L.braziers = {{420, 230}, {740, 500}, {1080, 290}};
        L.embers   = {{300, 380}, {640, 340}, {900, 330}, {1000, 470}};
        return L;
    }

    LevelDef mere()
    {
        LevelDef L;
        L.name  = "The Mere";
        L.line  = "cold water holds no fire";
        L.size  = {1600.0f, 900.0f};
        L.start = {120.0f, 460.0f};
        L.seed  = 22;
        L.drain = 0.02f;

        // A lake split by a dry causeway, with a small island in its upper
        // half that can only be reached by crossing water.
        L.water = {
            R(380, 160, 300, 270), R(680, 160, 220, 60), R(680, 330, 220, 100), R(840, 220, 60, 110),
            R(380, 490, 520, 250),
            R(1180, 0, 60, 200), R(1180, 270, 60, 160),       // a stream with one dry crossing
            R(1100, 620, 300, 170),
        };
        L.walls = {
            R(200, 640, 160, 22), R(1000, 120, 22, 160), R(1320, 470, 22, 120), R(1500, 140, 24, 24),
            R(150, 160, 24, 24),
        };
        L.braziers = {{250, 220}, {760, 275}, {1000, 790}, {1420, 300}, {1490, 760}};
        L.embers   = {{300, 460}, {560, 460}, {820, 460}, {1000, 330}, {1060, 680},
                      {1300, 240}, {1460, 560}, {760, 600}};
        return L;
    }

    LevelDef pass()
    {
        LevelDef L;
        L.name  = "Cold Pass";
        L.line  = "the wind wants you out";
        L.size  = {1920.0f, 900.0f};
        L.start = {100.0f, 460.0f};
        L.seed  = 33;
        L.drain = 0.018f;
        L.snow  = true;

        // Cliffs above and below, and four ridges with gaps that zig-zag.
        L.walls = {
            R(0, 0, 1920, 110), R(0, 790, 1920, 110),
            R(420, 110, 40, 300), R(420, 560, 40, 230),
            R(880, 300, 40, 490),
            R(1340, 110, 40, 490),
            R(1680, 110, 40, 270), R(1680, 520, 40, 270),
        };
        L.winds = {
            {R(460, 110, 420, 680),  {0.0f, 60.0f}},        // blowing down, against the climb
            {R(920, 110, 420, 680),  {-58.0f, 0.0f}},       // blowing back the way you came
            {R(1380, 110, 300, 680), {0.0f, -62.0f}},       // blowing up, toward the cliff
        };
        L.water    = {R(1520, 640, 120, 70)};
        L.braziers = {{220, 450}, {660, 300}, {1100, 700}, {1180, 200}, {1520, 450}, {1810, 450}};
        L.embers   = {{320, 300}, {540, 700}, {760, 470}, {980, 520}, {1250, 450},
                      {1450, 250}, {1610, 330}, {1770, 300}, {300, 650}};
        return L;
    }
}

const std::vector<LevelDef>& levels()
{
    static const std::vector<LevelDef> all = {hearth(), mere(), pass()};
    return all;
}

}
