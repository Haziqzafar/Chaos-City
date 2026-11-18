// =====================================================================
//  Chaos City - Charts.h
//  Small chart helpers for the Dashboard and DSA panel (SFML 2.4).
//    lineChart       - time series with filled area, grid and labels
//    barChart        - vertical bars with labels and value on top
//    hBar            - one horizontal bar (for ranked lists)
//    fitText         - shorten a string with "..." to fit a width
// =====================================================================
#ifndef CHARTS_H
#define CHARTS_H

#include "GuiCommon.h"

// "Nice" upper limit for an axis: 1, 2, 2.5, 5 x 10^k above the maximum
inline double niceMax(double v) {
    if (v <= 0) return 1;
    double p = std::pow(10.0, std::floor(std::log10(v)));
    double steps[5] = { 1, 2, 2.5, 5, 10 };
    for (int i = 0; i < 5; i++) if (steps[i] * p >= v) return steps[i] * p;
    return 10 * p;
}

inline std::string fitText(const sf::Font* f, const std::string& s, unsigned int size, float maxW) {
    if (textWidth(f, s, size) <= maxW) return s;
    std::string t = s;
    while (t.length() > 1 && textWidth(f, t + "...", size) > maxW) t.erase(t.length() - 1);
    return t + "...";
}

// values[0..n-1] oldest -> newest; xLabelLeft/Right shown under the axis
inline void lineChart(sf::RenderTarget& win, const sf::Font* f, float x, float y, float w, float h,
                      const double* values, int n, const sf::Color& colour,
                      const std::string& xLabelLeft, const std::string& xLabelRight,
                      const std::string& unit) {
    float left = x + 34, bottom = y + h - 16, top = y + 4, right = x + w;
    drawRect(win, x, y, w, h, sf::Color(26, 29, 36), Theme::BORDER, 1);
    if (n < 2) {
        drawText(win, f, "Collecting data...", x + 12, y + h / 2 - 16, 12, Theme::TEXT_DIM);
        drawText(win, f, "(one point every 30 simulated seconds)", x + 12, y + h / 2, 11, Theme::TEXT_DIM);
        return;
    }
    double mx = 0;
    for (int i = 0; i < n; i++) if (values[i] > mx) mx = values[i];
    double top_v = niceMax(mx * 1.1);

    // grid + y labels
    for (int g = 0; g <= 4; g++) {
        float gy = bottom - (bottom - top) * g / 4.0f;
        drawRect(win, left, gy, right - left - 4, 1, Theme::GRID);
        double val = top_v * g / 4.0;
        std::string lab = val >= 10 || val == 0 ? toStr((int)(val + 0.5)) : toStr1(val);
        float tw = textWidth(f, lab, 10);
        drawText(win, f, lab, left - tw - 5, gy - 7, 10, Theme::TEXT_DIM);
    }
    // filled area + line
    float span = right - left - 6;
    sf::VertexArray area(sf::TrianglesStrip);
    sf::VertexArray line(sf::LineStrip);
    for (int i = 0; i < n; i++) {
        float px = left + span * i / (float)(n - 1);
        float py = bottom - (float)(values[i] / top_v) * (bottom - top);
        area.append(sf::Vertex(sf::Vector2f(px, py), withAlpha(colour, 70)));
        area.append(sf::Vertex(sf::Vector2f(px, bottom), withAlpha(colour, 10)));
        line.append(sf::Vertex(sf::Vector2f(px, py), colour));
    }
    win.draw(area);
    win.draw(line);
    // last point marker + value
    float lx = left + span, ly = bottom - (float)(values[n - 1] / top_v) * (bottom - top);
    drawCircle(win, sf::Vector2f(lx, ly), 3.5f, colour);
    std::string cur = (values[n - 1] >= 10 ? toStr((int)(values[n - 1] + 0.5)) : toStr1(values[n - 1])) + unit;
    float cw = textWidth(f, cur, 11, true);
    float tx = lx - cw - 6;
    drawRect(win, tx - 3, ly - 20, cw + 6, 15, sf::Color(18, 20, 26, 220));
    drawText(win, f, cur, tx, ly - 20, 11, colour, true);
    // x labels
    drawText(win, f, xLabelLeft, left, bottom + 2, 10, Theme::TEXT_DIM);
    float rw = textWidth(f, xLabelRight, 10);
    drawText(win, f, xLabelRight, right - rw - 4, bottom + 2, 10, Theme::TEXT_DIM);
}

// Vertical bars; colours must hold n colours.
inline void barChart(sf::RenderTarget& win, const sf::Font* f, float x, float y, float w, float h,
                     const double* values, const std::string* labels, int n,
                     const sf::Color* colours, bool showValues) {
    float bottom = y + h - 16, top = y + 14;
    drawRect(win, x, bottom, w, 1, Theme::BORDER);
    double mx = 0;
    for (int i = 0; i < n; i++) if (values[i] > mx) mx = values[i];
    if (mx <= 0) mx = 1;
    float slot = w / n, bw = slot * 0.7f;
    for (int i = 0; i < n; i++) {
        float bh = (float)(values[i] / mx) * (bottom - top);
        float bx = x + i * slot + (slot - bw) / 2;
        if (bh > 0 && bh < 1) bh = 1;
        drawRect(win, bx, bottom - bh, bw, bh, colours[i]);
        if (showValues && values[i] > 0) {
            std::string v = values[i] >= 10 ? toStr((int)(values[i] + 0.5)) : toStr1(values[i]);
            float vw = textWidth(f, v, 10);
            drawText(win, f, v, bx + (bw - vw) / 2, bottom - bh - 14, 10, Theme::TEXT);
        }
        float lw = textWidth(f, labels[i], 10);
        drawText(win, f, labels[i], x + i * slot + (slot - lw) / 2, bottom + 2, 10, Theme::TEXT_DIM);
    }
}

// One horizontal bar: fraction 0..1 of 'w'
inline void hBar(sf::RenderTarget& win, float x, float y, float w, float h, double fraction, const sf::Color& c) {
    drawRect(win, x, y, w, h, sf::Color(26, 29, 36));
    float fw = (float)(fraction < 0 ? 0 : fraction > 1 ? 1 : fraction) * w;
    if (fw > 0) drawRect(win, x, y, fw, h, c);
}

// Bar on a LOG scale (each step = 10x), so 0.5 ms and 50 ms both stay visible
inline void logBar(sf::RenderTarget& win, float x, float y, float w, float h,
                   double value, double maxValue, const sf::Color& c) {
    double frac = 0;
    if (value > 0 && maxValue > 0) frac = std::log10(1 + value) / std::log10(1 + maxValue);
    hBar(win, x, y, w, h, frac, c);
}

// Draws text wrapped at word boundaries; returns the y below the last line
inline float drawWrapped(sf::RenderTarget& win, const sf::Font* f, const std::string& text,
                         float x, float y, float w, unsigned int size, const sf::Color& c) {
    std::string line, word;
    for (size_t i = 0; i <= text.length(); i++) {
        char ch = i < text.length() ? text[i] : ' ';
        if (ch == ' ') {
            std::string test = line.empty() ? word : line + " " + word;
            if (textWidth(f, test, size) > w && !line.empty()) {
                drawText(win, f, line, x, y, size, c);
                y += size + 5;
                line = word;
            } else line = test;
            word.clear();
        } else word += ch;
    }
    if (!line.empty()) { drawText(win, f, line, x, y, size, c); y += size + 5; }
    return y;
}

#endif
