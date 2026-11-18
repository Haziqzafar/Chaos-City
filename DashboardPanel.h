// =====================================================================
//  Chaos City - DashboardPanel.h
//  The DASHBOARD tab of the side panel. Three sub-views:
//
//   LIVE   - line chart of one measure over time, from the Analytics
//            snapshots (circular array, one every 30 s): vehicles in the
//            city, vehicles waiting, average speed, road congestion.
//   TRIPS  - histogram of finished trip times (AVL tree range counts),
//            median and 90th percentile (AVL kth), averages per vehicle
//            type, the three slowest trips.
//   ROADS  - most congested roads (merge sort, click to show on map),
//            busiest intersections (heap sort), Chaos event log.
// =====================================================================
#ifndef DASHBOARDPANEL_H
#define DASHBOARDPANEL_H

#include "Simulation.h"
#include "GuiCommon.h"
#include "Widgets.h"
#include "Charts.h"

const char* const DASH_SUBS[3] = { "Live", "Trips", "Roads & log" };
const char* const DASH_METRICS[4] = { "Vehicles", "Waiting", "Speed", "Congestion" };

inline std::string dashTime(double seconds) {
    std::string t = Simulation::formatTime(seconds);
    return seconds < 3600 ? t.substr(2) : t;
}

inline float dashTitle(sf::RenderTarget& w, const sf::Font* f, const std::string& s,
                       float x, float y, float width) {
    drawText(w, f, s, x, y, 12, Theme::ACCENT, true);
    drawRect(w, x, y + 18, width, 1, Theme::BORDER);
    return y + 25;
}

// ---------------- LIVE ----------------
inline void dashLive(sf::RenderTarget& win, const sf::Font* f, UiInput& ui, Simulation& sim,
                     int& metric, float x, float y, float w) {
    Analytics& an = sim.analytics();
    metric = segmented(win, f, ui, x, y, w, 26, DASH_METRICS, 4, metric, Theme::ACCENT, 11);
    y += 36;

    int n = an.historyCount();
    double* vals = new double[n > 0 ? n : 1];
    for (int i = 0; i < n; i++) {
        const Snapshot& s = an.historyAt(i);
        if (metric == 0) vals[i] = s.active;
        else if (metric == 1) vals[i] = s.queued + s.entering;
        else if (metric == 2) vals[i] = s.avgSpeedKmh;
        else vals[i] = s.avgCongestion * 100;
    }
    sf::Color col = metric == 0 ? Theme::ACCENT : metric == 1 ? Theme::WARN : metric == 2 ? Theme::GOOD : Theme::BAD;
    const char* units[4] = { "", "", " km/h", "%" };
    std::string left = n > 0 ? dashTime(an.historyAt(0).time) : "";
    std::string right = n > 0 ? dashTime(an.historyAt(n - 1).time) : "";
    lineChart(win, f, x, y, w, 190, vals, n, col, left, right, units[metric]);
    y += 200;

    // min / average / max of the shown series
    if (n > 0) {
        double mn = vals[0], mx = vals[0], sum = 0;
        for (int i = 0; i < n; i++) { if (vals[i] < mn) mn = vals[i]; if (vals[i] > mx) mx = vals[i]; sum += vals[i]; }
        std::string v[3] = { toStr1(mn), toStr1(sum / n), toStr1(mx) };
        const char* l[3] = { "lowest", "average", "highest" };
        for (int i = 0; i < 3; i++) {
            drawText(win, f, v[i] + units[metric], x + i * w / 3, y, 15, Theme::TEXT, true);
            drawText(win, f, l[i], x + i * w / 3, y + 19, 11, Theme::TEXT_DIM);
        }
    }
    delete[] vals;
    y += 46;

    const char* explain[4] = {
        "Vehicles currently driving or queued in the city.",
        "Vehicles at red lights plus those waiting to enter.",
        "Average speed of moving vehicles (limit / congestion).",
        "Average fullness of roads that carry traffic." };
    drawText(win, f, explain[metric], x, y, 11, Theme::TEXT_DIM);
    y += 26;

    // headline comparison numbers
    y = dashTitle(win, f, "SINCE START", x, y, w);
    SimSummary s = sim.summary();
    std::string a[4] = { toStr(sim.vehicles().spawnedCount()), toStr(s.arrived),
                         toStr(s.rerouted), toStr(s.diverted + s.cancelled) };
    const char* b[4] = { "spawned", "arrived", "reroutes", "diverted/cancelled" };
    for (int i = 0; i < 4; i++) {
        float cx = x + (i % 2) * w / 2, cy = y + (i / 2) * 40;
        drawText(win, f, a[i], cx, cy, 16, Theme::TEXT, true);
        drawText(win, f, b[i], cx, cy + 20, 11, Theme::TEXT_DIM);
    }
}

// ---------------- TRIPS ----------------
inline void dashTrips(sf::RenderTarget& win, const sf::Font* f, Simulation& sim, float x, float y, float w) {
    Analytics& an = sim.analytics();
    Graph& g = sim.graph();
    y = dashTitle(win, f, "TRIP TIMES  (" + toStr(an.tripCount()) + " finished trips)", x, y, w);
    if (an.tripCount() == 0) {
        drawText(win, f, "No trips finished yet - let the simulation run.", x, y + 4, 12, Theme::TEXT_DIM);
        return;
    }
    // histogram: 5-minute buckets from the AVL tree
    const int B = 8;
    int buckets[B];
    an.histogram(buckets, B, 300);
    double vals[B];
    std::string labels[B];
    sf::Color cols[B];
    for (int i = 0; i < B; i++) {
        vals[i] = buckets[i];
        labels[i] = i == B - 1 ? "35+" : toStr(i * 5);
        cols[i] = congestionColor(i / (double)(B - 1));
    }
    float hx = x, hw = w, hy = y + 2, hh = 130;
    barChart(win, f, hx, hy, hw, hh, vals, labels, B, cols, true);
    // median / P90 markers (position inside the bucket axis)
    double med = an.medianTrip(), p90 = an.percentile(90);
    double marks[2] = { med, p90 };
    const char* mlab[2] = { "median", "90%" };
    for (int i = 0; i < 2; i++) {
        double pos = marks[i] / 300.0;
        if (pos > B) pos = B;
        float mx = hx + (float)(pos / B) * hw;
        drawDashedLine(win, sf::Vector2f(mx, hy + 12), sf::Vector2f(mx, hy + hh - 16), 1.5f, 3, Theme::TEXT);
        drawText(win, f, mlab[i], mx + 3, hy + (i == 0 ? 0 : 12), 10, Theme::TEXT);
    }
    y = hy + hh + 2;
    drawText(win, f, "minutes", x + w - 44, y - 2, 10, Theme::TEXT_DIM);
    y += 14;
    std::string kv[4] = { dashTime(an.fastestTrip()), dashTime(med), dashTime(p90), dashTime(an.slowestTrip()) };
    const char* kl[4] = { "fastest", "median", "90th %", "slowest" };
    for (int i = 0; i < 4; i++) {
        drawText(win, f, kv[i], x + i * w / 4, y, 14, Theme::TEXT, true);
        drawText(win, f, kl[i], x + i * w / 4, y + 18, 11, Theme::TEXT_DIM);
    }
    y += 38;
    drawText(win, f, "AVL tree: " + toStr(an.tripCount()) + " trips, height " +
             toStr(an.getTree().height()) + ", each query O(log n)", x, y, 11, Theme::TEXT_DIM);
    y += 24;

    // by vehicle type
    y = dashTitle(win, f, "AVERAGE TRIP BY VEHICLE TYPE", x, y, w);
    VehicleType types[4] = { CAR, BUS, TRUCK, AMBULANCE };
    sf::Color tc[4] = { Theme::CAR, Theme::BUS, Theme::TRUCK, Theme::BAD };
    double mx = 1;
    for (int i = 0; i < 4; i++) if (an.avgTimeByType(types[i]) > mx) mx = an.avgTimeByType(types[i]);
    for (int i = 0; i < 4; i++) {
        drawText(win, f, Vehicle::typeName(types[i]), x, y, 12, Theme::TEXT_DIM);
        hBar(win, x + 74, y + 3, w - 160, 11, an.avgTimeByType(types[i]) / mx, tc[i]);
        std::string v = an.countByType(types[i]) ? dashTime(an.avgTimeByType(types[i])) : "-";
        drawText(win, f, v, x + w - 80, y, 12, Theme::TEXT, true);
        drawText(win, f, "(" + toStr(an.countByType(types[i])) + ")", x + w - 32, y, 11, Theme::TEXT_DIM);
        y += 19;
    }
    y += 8;

    // slowest trips
    y = dashTitle(win, f, "SLOWEST TRIPS", x, y, w);
    TripRecord worst[3];
    int k = an.slowestTrips(worst, 3);
    for (int i = 0; i < k; i++) {
        std::string s = "#" + toStr(worst[i].vehicleId) + " " + Vehicle::typeName(worst[i].type) + "  " +
                        g.getIntersection(worst[i].origin).name + " -> " + g.getIntersection(worst[i].destination).name;
        drawText(win, f, fitText(f, s, 12, w - 64), x, y, 12, Theme::TEXT);
        drawText(win, f, dashTime(worst[i].travelTime), x + w - 56, y, 12, Theme::BAD, true);
        y += 18;
    }
}

// ---------------- ROADS & LOG ----------------
// Returns a road id if the user clicked one in the ranking, else -1
inline int dashRoads(sf::RenderTarget& win, const sf::Font* f, UiInput& ui, Simulation& sim,
                     float x, float y, float w, float bottom) {
    Analytics& an = sim.analytics();
    Graph& g = sim.graph();
    int clicked = -1;

    int top[6];
    SortStats ss;
    int k = an.topCongestedRoads(top, 6, MERGE_SORT, ss);
    y = dashTitle(win, f, "MOST CONGESTED ROADS  (click to show)", x, y, w);
    for (int i = 0; i < k; i++) {
        const Road* r = g.getRoad(top[i]);
        bool hover = inside(ui, x - 4, y - 2, w + 8, 19);
        if (hover) drawRect(win, x - 4, y - 2, w + 8, 19, Theme::PANEL_LIGHT);
        if (hover && ui.clicked && !ui.used) { ui.used = true; clicked = top[i]; }
        std::string name = toStr(i + 1) + ". " + sim.roadLabel(top[i]);
        drawText(win, f, fitText(f, name, 12, w - 120), x, y, 12, Theme::TEXT);
        hBar(win, x + w - 116, y + 3, 70, 10, r->congestionRatio(), congestionColor(r->congestionRatio()));
        std::string pct = toStr((int)(r->congestionRatio() * 100)) + "%";
        drawText(win, f, pct, x + w - textWidth(f, pct, 12, true), y, 12, Theme::TEXT, true);
        y += 19;
    }
    drawText(win, f, "Ranked with merge sort: " + toStr(ss.comparisons) + " comparisons for " +
             toStr(g.getRoadCount()) + " roads", x, y + 2, 10, Theme::TEXT_DIM);
    y += 24;

    ScoredItem busy[3];
    SortStats hs;
    an.busiestIntersections(busy, 3, HEAP_SORT, hs);
    y = dashTitle(win, f, "BUSIEST INTERSECTIONS  (vehicles at lights)", x, y, w);
    for (int i = 0; i < 3; i++) {
        drawText(win, f, toStr(i + 1) + ". " + g.getIntersection(busy[i].id).name, x + (i * w / 3), y, 12, Theme::TEXT);
        drawText(win, f, toStr(busy[i].score) + " waiting", x + (i * w / 3), y + 16, 11,
                 busy[i].score > 5 ? Theme::WARN : Theme::TEXT_DIM);
    }
    y += 42;

    // event log, newest first
    int total = sim.chaos().eventCount();
    y = dashTitle(win, f, "EVENT LOG  (" + toStr(total) + " events)", x, y, w);
    int room = (int)((bottom - y - 10) / 17);
    if (room <= 0) return clicked;
    const ChaosEvent** last = new const ChaosEvent*[room];
    int cnt = 0;
    for (ListNode<ChaosEvent>* n = sim.chaos().eventLogHead(); n != NULL; n = n->next) {
        if (cnt < room) last[cnt++] = &n->data;
        else { for (int i = 1; i < room; i++) last[i - 1] = last[i]; last[room - 1] = &n->data; }
    }
    if (cnt == 0) drawText(win, f, "Nothing has happened yet. Try the Chaos buttons.", x, y, 11, Theme::TEXT_DIM);
    for (int i = cnt - 1; i >= 0; i--) {
        const std::string& t = last[i]->text;
        sf::Color c = Theme::TEXT_DIM;
        if (t.find("ACCIDENT") != std::string::npos || t.find("CLOSED") != std::string::npos) c = Theme::BAD;
        else if (t.find("CLEARED") != std::string::npos || t.find("REOPENED") != std::string::npos) c = Theme::GOOD;
        else if (t.find("SURGE") != std::string::npos || t.find("CHAOS") != std::string::npos) c = Theme::WARN;
        drawText(win, f, dashTime(last[i]->time), x, y, 11, Theme::TEXT_DIM);
        drawText(win, f, fitText(f, t, 11, w - 46), x + 44, y, 11, c);
        y += 17;
    }
    delete[] last;
    return clicked;
}

// Draws the whole tab. Returns a road id clicked in the ranking, or -1.
inline int drawDashboardTab(sf::RenderTarget& win, const sf::Font* f, UiInput& ui, Simulation& sim,
                            int& sub, int& metric, float x, float y, float w, float bottom) {
    sub = segmented(win, f, ui, x, y, w, 26, DASH_SUBS, 3, sub, Theme::GOOD, 12);
    y += 38;
    if (sub == 0) { dashLive(win, f, ui, sim, metric, x, y, w); return -1; }
    if (sub == 1) { dashTrips(win, f, sim, x, y, w); return -1; }
    return dashRoads(win, f, ui, sim, x, y, w, bottom);
}

#endif
