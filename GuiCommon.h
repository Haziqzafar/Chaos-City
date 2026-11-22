// =====================================================================
//  Chaos City - GuiCommon.h
//  Shared look & small drawing helpers for the SFML GUI.
//  Only uses features available in SFML 2.4.
// =====================================================================
#ifndef GUICOMMON_H
#define GUICOMMON_H

#include <SFML/Graphics.hpp>
#include <string>
#include <sstream>
#include <fstream>
#include <cmath>

const float GUI_PI = 3.14159265f;

// ---------- colour theme ----------
namespace Theme {
    const sf::Color BG(22, 24, 30);
    const sf::Color MAP_BG(27, 30, 37);
    const sf::Color GRID(34, 38, 46);
    const sf::Color PANEL(33, 36, 44);
    const sf::Color PANEL_LIGHT(45, 49, 60);
    const sf::Color BORDER(60, 66, 80);
    const sf::Color TEXT(228, 231, 238);
    const sf::Color TEXT_DIM(148, 155, 170);
    const sf::Color ACCENT(84, 160, 255);
    const sf::Color GOOD(70, 200, 110);
    const sf::Color WARN(242, 190, 50);
    const sf::Color BAD(232, 72, 60);
    const sf::Color ROAD_EMPTY(78, 86, 100);
    const sf::Color ROAD_CLOSED(55, 58, 66);
    const sf::Color NODE(70, 140, 220);
    const sf::Color NODE_RING(170, 205, 255);
    const sf::Color CAR(250, 200, 70);
    const sf::Color BUS(80, 195, 255);
    const sf::Color TRUCK(190, 140, 255);
    const sf::Color AMBULANCE(255, 255, 255);
}

// ---------- small utilities ----------
inline bool fileExists(const std::string& path) {
    std::ifstream f(path.c_str());
    return f.good();
}

// Tries several font locations, checking the file exists first so SFML
// does not print "Failed to load font" warnings.
inline bool loadGuiFont(sf::Font& font) {
    const char* paths[] = {
        "arial.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    };
    for (int i = 0; i < 5; i++)
        if (fileExists(paths[i]) && font.loadFromFile(paths[i])) return true;
    return false;
}

template <typename T>
inline std::string toStr(T value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

inline std::string toStr1(double value) {          // one decimal place
    std::stringstream ss;
    ss.setf(std::ios::fixed);
    ss.precision(1);
    ss << value;
    return ss.str();
}

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline sf::Color lerpColor(const sf::Color& a, const sf::Color& b, float t) {
    t = clampf(t, 0, 1);
    return sf::Color((sf::Uint8)(a.r + (b.r - a.r) * t),
                     (sf::Uint8)(a.g + (b.g - a.g) * t),
                     (sf::Uint8)(a.b + (b.b - a.b) * t),
                     (sf::Uint8)(a.a + (b.a - a.a) * t));
}

inline sf::Color withAlpha(sf::Color c, sf::Uint8 a) { c.a = a; return c; }

// 0 = free (green) -> 0.5 (yellow) -> 1.0+ full (red)
inline sf::Color congestionColor(double ratio) {
    if (ratio <= 0.5) return lerpColor(Theme::GOOD, Theme::WARN, (float)(ratio / 0.5));
    return lerpColor(Theme::WARN, Theme::BAD, (float)((ratio - 0.5) / 0.5));
}

// ---------- drawing helpers ----------
inline void drawThickLine(sf::RenderTarget& t, sf::Vector2f a, sf::Vector2f b,
                          float thickness, const sf::Color& c) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.01f) return;
    sf::RectangleShape r(sf::Vector2f(len, thickness));
    r.setOrigin(0, thickness / 2);
    r.setPosition(a);
    r.setRotation(std::atan2(d.y, d.x) * 180.0f / GUI_PI);
    r.setFillColor(c);
    t.draw(r);
}

inline void drawDashedLine(sf::RenderTarget& t, sf::Vector2f a, sf::Vector2f b,
                           float thickness, float dash, const sf::Color& c) {
    sf::Vector2f d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 0.01f) return;
    sf::Vector2f u = d / len;
    for (float s = 0; s < len; s += dash * 2) {
        float e = s + dash < len ? s + dash : len;
        drawThickLine(t, a + u * s, a + u * e, thickness, c);
    }
}

inline void drawRect(sf::RenderTarget& t, float x, float y, float w, float h, const sf::Color& fill,
                     const sf::Color& outline = sf::Color::Transparent, float outlineThickness = 0) {
    sf::RectangleShape r(sf::Vector2f(w, h));
    r.setPosition(x, y);
    r.setFillColor(fill);
    if (outlineThickness > 0) { r.setOutlineColor(outline); r.setOutlineThickness(outlineThickness); }
    t.draw(r);
}

inline void drawCircle(sf::RenderTarget& t, sf::Vector2f c, float radius, const sf::Color& fill,
                       const sf::Color& outline = sf::Color::Transparent, float outlineThickness = 0) {
    sf::CircleShape s(radius);
    s.setOrigin(radius, radius);
    s.setPosition(c);
    s.setFillColor(fill);
    if (outlineThickness > 0) { s.setOutlineColor(outline); s.setOutlineThickness(outlineThickness); }
    t.draw(s);
}

// Draws text; returns its width. font may be NULL (then nothing is drawn).
inline float drawText(sf::RenderTarget& t, const sf::Font* font, const std::string& s,
                      float x, float y, unsigned int size, const sf::Color& c,
                      bool bold = false, bool outline = false) {
    if (font == NULL) return 0;
    sf::Text txt(s, *font, size);
    txt.setFillColor(c);
    if (bold) txt.setStyle(sf::Text::Bold);
    if (outline) { txt.setOutlineColor(sf::Color(15, 17, 22)); txt.setOutlineThickness(2); }
    txt.setPosition(std::floor(x), std::floor(y));
    t.draw(txt);
    return txt.getLocalBounds().width;
}

inline float textWidth(const sf::Font* font, const std::string& s, unsigned int size, bool bold = false) {
    if (font == NULL) return 0;
    sf::Text txt(s, *font, size);
    if (bold) txt.setStyle(sf::Text::Bold);
    return txt.getLocalBounds().width;
}

// Distance from point p to segment ab (for mouse hit-testing roads)
inline float distToSegment(sf::Vector2f p, sf::Vector2f a, sf::Vector2f b) {
    sf::Vector2f ab = b - a, ap = p - a;
    float len2 = ab.x * ab.x + ab.y * ab.y;
    float t = len2 > 0 ? (ap.x * ab.x + ap.y * ab.y) / len2 : 0;
    t = clampf(t, 0, 1);
    sf::Vector2f q = a + ab * t - p;
    return std::sqrt(q.x * q.x + q.y * q.y);
}

#endif
