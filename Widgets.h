// =====================================================================
//  Chaos City - Widgets.h
//  Tiny "immediate mode" GUI: every frame we draw each button and ask
//  "was it clicked?" in the same call. No button objects to manage.
//
//    if (button(win, font, ui, x, y, w, h, "Pause")) sim.togglePause();
//
//  UiInput holds this frame's mouse state. 'used' makes sure one click
//  only triggers one widget, and tells the map the click was taken.
// =====================================================================
#ifndef WIDGETS_H
#define WIDGETS_H

#include "GuiCommon.h"

struct UiInput {
    sf::Vector2f mouse;
    bool clicked;      // left button pressed this frame
    bool used;         // a widget already took this click
    UiInput() : clicked(false), used(false) {}
};

inline bool inside(const UiInput& in, float x, float y, float w, float h) {
    return in.mouse.x >= x && in.mouse.x < x + w && in.mouse.y >= y && in.mouse.y < y + h;
}

// Text centred inside a box
inline void centeredText(sf::RenderTarget& t, const sf::Font* f, const std::string& s,
                         float x, float y, float w, float h, unsigned int size,
                         const sf::Color& c, bool bold = false) {
    float tw = textWidth(f, s, size, bold);
    drawText(t, f, s, x + (w - tw) / 2, y + (h - size) / 2 - 2, size, c, bold);
}

// Push button. active = drawn highlighted (for toggles / selected tools).
inline bool button(sf::RenderTarget& t, const sf::Font* f, UiInput& in,
                   float x, float y, float w, float h, const std::string& label,
                   bool active = false, bool enabled = true,
                   const sf::Color& accent = Theme::ACCENT, unsigned int size = 13) {
    bool hover = enabled && inside(in, x, y, w, h);
    sf::Color fill = !enabled ? sf::Color(38, 41, 50)
                   : active ? accent
                   : hover ? sf::Color(62, 68, 82) : Theme::PANEL_LIGHT;
    drawRect(t, x, y, w, h, fill, hover ? withAlpha(accent, 200) : Theme::BORDER, 1);
    sf::Color tc = !enabled ? sf::Color(95, 100, 112) : active ? sf::Color(15, 17, 22) : Theme::TEXT;
    centeredText(t, f, label, x, y, w, h, size, tc, active);
    if (hover && in.clicked && !in.used) { in.used = true; return true; }
    return false;
}

// Row of mutually exclusive buttons. Returns the (possibly new) selection.
inline int segmented(sf::RenderTarget& t, const sf::Font* f, UiInput& in,
                     float x, float y, float w, float h,
                     const char* const* labels, int count, int current,
                     const sf::Color& accent = Theme::ACCENT, unsigned int size = 12) {
    float bw = w / count;
    int result = current;
    for (int i = 0; i < count; i++)
        if (button(t, f, in, x + i * bw, y, bw - 2, h, labels[i], i == current, true, accent, size))
            result = i;
    return result;
}

// Single-line text box. Click to focus (typing is handled in main via
// sf::Event::TextEntered while 'focused' is true). A click anywhere else
// removes the focus.
inline void textBox(sf::RenderTarget& t, const sf::Font* f, UiInput& in,
                    float x, float y, float w, float h, const std::string& text,
                    const std::string& placeholder, bool& focused, float seconds) {
    bool hover = inside(in, x, y, w, h);
    if (in.clicked && !in.used) {
        if (hover) { focused = true; in.used = true; }
        else focused = false;
    }
    drawRect(t, x, y, w, h, sf::Color(20, 22, 28), focused ? Theme::ACCENT : hover ? Theme::TEXT_DIM : Theme::BORDER,
             focused ? 2.0f : 1.0f);
    if (text.empty() && !focused) drawText(t, f, placeholder, x + 8, y + (h - 13) / 2 - 2, 13, Theme::TEXT_DIM);
    float tw = drawText(t, f, text, x + 8, y + (h - 14) / 2 - 2, 14, Theme::TEXT);
    if (focused && ((int)(seconds * 2)) % 2 == 0)
        drawRect(t, x + 10 + tw, y + 6, 2, h - 12, Theme::TEXT);
}

// Floating box with several lines of text, kept inside 'bounds'
inline void drawTooltip(sf::RenderTarget& t, const sf::Font* f, sf::Vector2f at,
                        const std::string* lines, const sf::Color* colours, int n,
                        const sf::FloatRect& bounds) {
    if (f == NULL || n == 0) return;
    float w = 0;
    for (int i = 0; i < n; i++) {
        float tw = textWidth(f, lines[i], 12, i == 0);
        if (tw > w) w = tw;
    }
    w += 20;
    float h = n * 18.0f + 12;
    float x = at.x + 16, y = at.y + 16;
    if (x + w > bounds.left + bounds.width) x = at.x - w - 10;
    if (y + h > bounds.top + bounds.height) y = at.y - h - 10;
    drawRect(t, x, y, w, h, sf::Color(18, 20, 26, 235), Theme::BORDER, 1);
    for (int i = 0; i < n; i++)
        drawText(t, f, lines[i], x + 10, y + 6 + i * 18.0f, 12, colours[i], i == 0);
}

#endif
