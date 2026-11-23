// =====================================================================
//  CHAOS CITY - SFML GUI  (Step 10: map, controls, dashboard, DSA panel)
//
//  Layout:  [ live map ][ side panel: header + tabs ]
//  Tabs:    CONTROL | DASHBOARD (charts, trips, roads & log) |
//           DSA PANEL (sorting, search & paths, graphs, strings)
//
//  Map tools (what a LEFT click on the map does):
//    1 Inspect   click a road / intersection to pin its details
//    2 Close     close / reopen a road (both directions)
//    3 Accident  accident on one lane for 3 minutes
//    4 Route     click start A, then destination B -> fastest route shown,
//                compared with BFS and DFS; send an ambulance along it
//    5 Surge     40 vehicles head to the clicked intersection
//
//  Keys: Space pause  +/- speed  F fit map  1-5 tools  Z undo  Y redo
//        C Chaos Mode  S signal strategy  L legend  Esc clear selection
//  Mouse: wheel zoom, right/middle drag pan, hover for live details.
//
//  Dev-C++ project: this must be the ONLY .cpp file in the project.
// =====================================================================
#include <SFML/Graphics.hpp>
#include <iostream>
#include "Simulation.h"
#include "GuiCommon.h"
#include "Widgets.h"
#include "MapView.h"
#include "DashboardPanel.h"
#include "DSAPanelTab.h"

const float PANEL_WIDTH = 350;
const float ACCIDENT_SECONDS = 180;
const int   SURGE_SIZE = 40;

enum Tool { TOOL_INSPECT = 0, TOOL_CLOSE, TOOL_ACCIDENT, TOOL_ROUTE, TOOL_SURGE };
const int TOOL_COUNT = 5;
const char* const TOOL_NAMES[TOOL_COUNT] = { "Inspect", "Close", "Accident", "Route", "Surge" };

enum Tab { TAB_CONTROL = 0, TAB_DASHBOARD, TAB_DSA };
const char* const TAB_NAMES[3] = { "CONTROL", "DASHBOARD", "DSA PANEL" };

// Everything the GUI remembers between frames
struct AppState {
    int tab;
    int dashSub, dashMetric;       // dashboard sub-view and chart measure
    int tool;
    bool showLegend;

    // route tool
    int routeA, routeB;
    LinkedList<int> route;
    float routeRefresh;           // seconds until the route is recalculated
    int dijkstraTime, dijkstraRoads;
    int bfsTime, bfsRoads, dfsTime, dfsRoads;
    int dijkstraVisited, bfsVisited, dfsVisited;

    // followed vehicle
    int followId;

    // DSA panel tab
    DsaTabState dsa;

    // notification ("toast")
    std::string toast;
    float toastAge;

    AppState() : tab(TAB_CONTROL), dashSub(0), dashMetric(0), tool(TOOL_INSPECT), showLegend(true), routeA(-1), routeB(-1),
                 routeRefresh(0), dijkstraTime(INF), dijkstraRoads(0), bfsTime(0), bfsRoads(0),
                 dfsTime(0), dfsRoads(0), dijkstraVisited(0), bfsVisited(0), dfsVisited(0),
                 followId(-1), toastAge(100) {}
};

void notify(AppState& st, const std::string& msg) { st.toast = msg; st.toastAge = 0; }

// "12:34" under an hour, "1:02:03" from one hour on
std::string shortTime(double seconds) {
    std::string t = Simulation::formatTime(seconds);
    return seconds < 3600 ? t.substr(2) : t;
}

std::string lastEventText(Simulation& sim) {
    std::string s;
    for (ListNode<ChaosEvent>* n = sim.chaos().eventLogHead(); n != NULL; n = n->next) s = n->data.text;
    return s;
}

// ---------------------------------------------------------------
//  Route tool: BFS / DFS / Dijkstra between A and B (live traffic)
// ---------------------------------------------------------------
void recomputeRoute(AppState& st, Simulation& sim, MapView& map) {
    if (st.routeA < 0 || st.routeB < 0) { map.setRouteEnds(st.routeA, st.routeB); return; }
    Graph& g = sim.graph();
    LinkedList<int> tmp;
    SearchStats s;
    st.bfsRoads = GraphAlgorithms::bfsRoute(g, st.routeA, st.routeB, tmp, s);
    st.bfsTime = GraphAlgorithms::routeTime(g, tmp, true);
    st.bfsVisited = s.nodesVisited;
    st.dfsRoads = GraphAlgorithms::dfsRoute(g, st.routeA, st.routeB, tmp, s);
    st.dfsTime = GraphAlgorithms::routeTime(g, tmp, true);
    st.dfsVisited = s.nodesVisited;
    st.dijkstraTime = GraphAlgorithms::dijkstraRoute(g, st.routeA, st.routeB, true, st.route, s);
    st.dijkstraRoads = st.route.size();
    st.dijkstraVisited = s.nodesVisited;
    map.setRoute(st.route, st.routeA, st.routeB);
    st.routeRefresh = 0.5f;
}

void clearRoute(AppState& st, MapView& map) {
    st.routeA = st.routeB = -1;
    st.route.clear();
    map.clearRoute();
}

// ---------------------------------------------------------------
//  Detail lines for a road / intersection (tooltip + inspect panel)
// ---------------------------------------------------------------
int roadLines(Simulation& sim, int r, std::string* L, sf::Color* C) {
    const Road* road = sim.graph().getRoad(r);
    int n = 0;
    L[n] = sim.roadLabel(r); C[n++] = Theme::TEXT;
    L[n] = toStr1(road->length / 1000.0) + " km, " + toStr(road->lanes) + " lane(s), limit " +
           toStr(road->speedLimit) + " km/h"; C[n++] = Theme::TEXT_DIM;
    L[n] = "Vehicles " + toStr(road->vehiclesOnRoad) + " / " + toStr(road->capacity()) + "  (" +
           toStr((int)(road->congestionRatio() * 100)) + "% full)";
    C[n++] = road->vehiclesOnRoad ? congestionColor(road->congestionRatio()) : Theme::TEXT_DIM;
    L[n] = road->blocked ? std::string("Travel time: blocked")
                         : "Travel time " + toStr(road->currentTime()) + " s (free " + toStr(road->baseTime()) + " s)";
    C[n++] = Theme::TEXT_DIM;
    LightColor lc = sim.signals().colorOf(r);
    L[n] = "Queue " + toStr(sim.vehicles().queueLength(r)) + ", light " +
           (lc == LIGHT_GREEN ? "GREEN" : lc == LIGHT_YELLOW ? "YELLOW" : "RED");
    C[n++] = lc == LIGHT_GREEN ? Theme::GOOD : lc == LIGHT_YELLOW ? Theme::WARN : Theme::BAD;
    if (sim.chaos().hasAccident(r)) { L[n] = "ACCIDENT - vehicles trapped"; C[n++] = Theme::BAD; }
    else if (sim.chaos().isClosed(r)) { L[n] = "CLOSED - no entry"; C[n++] = Theme::BAD; }
    return n;
}

int nodeLines(Simulation& sim, int v, std::string* L, sf::Color* C) {
    Graph& g = sim.graph();
    int n = 0, waiting = 0, entering = sim.vehicles().totalEntering() >= 0 ? 0 : 0;
    for (int r = 0; r < g.getRoadCount(); r++) {
        if (g.getRoad(r)->to == v) waiting += sim.vehicles().queueLength(r);
        if (g.getRoad(r)->from == v) entering += sim.vehicles().entryQueueLength(r);
    }
    L[n] = g.getIntersection(v).name; C[n++] = Theme::TEXT;
    L[n] = "Roads in " + toStr(g.inDegree(v)) + ", out " + toStr(g.outDegree(v)); C[n++] = Theme::TEXT_DIM;
    L[n] = "Waiting at lights " + toStr(waiting) + ", waiting to enter " + toStr(entering);
    C[n++] = waiting + entering > 10 ? Theme::WARN : Theme::TEXT_DIM;
    const TrafficLight* T = sim.signals().getLight(v);
    if (T->count > 1) {
        int gr = T->phase == PHASE_GREEN ? T->currentGreen : T->pendingGreen;
        L[n] = std::string(T->phase == PHASE_GREEN ? "Green for traffic from " : "Next green: from ") +
               g.getIntersection(g.getRoad(gr)->from).name;
        C[n++] = Theme::GOOD;
        L[n] = "Phase changes so far " + toStr(T->switches); C[n++] = Theme::TEXT_DIM;
    } else {
        L[n] = "No signal (single approach)"; C[n++] = Theme::TEXT_DIM;
    }
    return n;
}

// ---------------------------------------------------------------
//  Map clicks: what happens depends on the selected tool
// ---------------------------------------------------------------
void handleMapClick(AppState& st, Simulation& sim, MapView& map) {
    Graph& g = sim.graph();
    int r = map.getHoverRoad(), v = map.getHoverNode();
    if (st.tab == TAB_DSA && st.dsa.pickEvac) {            // choosing an evacuation hotspot
        if (v == -1) { notify(st, "Click on an intersection to use as the hotspot."); return; }
        st.dsa.evacSource = v;
        st.dsa.pickEvac = false;
        st.dsa.showEvac = true;
        notify(st, "Evacuation order from " + g.getIntersection(v).name + " shown as numbered badges");
        return;
    }
    switch (st.tool) {
        case TOOL_INSPECT:
            map.setSelectedRoad(r);
            map.setSelectedNode(v);
            break;
        case TOOL_CLOSE:
            if (r == -1) { notify(st, "Click on a road to close or reopen it."); break; }
            if (sim.chaos().isClosed(r)) sim.reopenRoad(g.getRoad(r)->from, g.getRoad(r)->to, true);
            else sim.closeRoad(g.getRoad(r)->from, g.getRoad(r)->to, true);
            notify(st, lastEventText(sim));
            if (st.routeB >= 0) recomputeRoute(st, sim, map);
            break;
        case TOOL_ACCIDENT:
            if (r == -1) { notify(st, "Click on a lane to cause an accident there."); break; }
            sim.accident(r, ACCIDENT_SECONDS);
            notify(st, lastEventText(sim));
            if (st.routeB >= 0) recomputeRoute(st, sim, map);
            break;
        case TOOL_ROUTE:
            if (v == -1) { notify(st, "Click on an intersection (blue circle)."); break; }
            if (st.routeA == -1 || st.routeB != -1) {          // start a new route
                clearRoute(st, map);
                st.routeA = v;
                map.setRouteEnds(v, -1);
                notify(st, "Start: " + g.getIntersection(v).name + ". Now click the destination.");
            } else if (v != st.routeA) {
                st.routeB = v;
                recomputeRoute(st, sim, map);
                notify(st, st.dijkstraTime == INF ? std::string("No route - destination is cut off!")
                         : "Fastest route: " + Simulation::formatTime(st.dijkstraTime) + " via Dijkstra");
            }
            break;
        case TOOL_SURGE:
            if (v == -1) { notify(st, "Click on an intersection to send a surge there."); break; }
            sim.surgeTo(v, SURGE_SIZE);
            notify(st, lastEventText(sim));
            break;
    }
}

// ---------------------------------------------------------------
//  Side panel
// ---------------------------------------------------------------
float sectionTitle(sf::RenderWindow& w, const sf::Font* f, const std::string& s, float x, float y, float width) {
    drawText(w, f, s, x, y, 12, Theme::ACCENT, true);
    drawRect(w, x, y + 18, width, 1, Theme::BORDER);
    return y + 25;
}

float drawHeader(sf::RenderWindow& win, const sf::Font* f, Simulation& sim, float x, float w, float seconds) {
    SimSummary s = sim.summary();
    float y = 12;
    drawText(win, f, "CHAOS CITY", x, y, 20, Theme::TEXT, true);
    drawText(win, f, "Intelligent Traffic Simulation", x + 140, y + 6, 11, Theme::TEXT_DIM);
    y += 32;
    drawRect(win, x, y, w, 42, Theme::PANEL_LIGHT);
    drawText(win, f, Simulation::formatTime(s.time), x + 10, y + 7, 22, Theme::TEXT, true);
    if (s.paused) {
        bool on = ((int)(seconds * 2)) % 2 == 0;
        drawRect(win, x + w - 82, y + 10, 72, 22, on ? Theme::WARN : withAlpha(Theme::WARN, 110));
        drawText(win, f, "PAUSED", x + w - 73, y + 12, 13, sf::Color::Black, true);
    } else {
        std::string sp = "x" + (s.speed < 1 ? toStr1(s.speed) : toStr((int)s.speed));
        float tw = textWidth(f, sp, 20, true);
        drawText(win, f, sp, x + w - tw - 10, y + 8, 20, Theme::GOOD, true);
    }
    y += 48;
    // four mini stats
    std::string vals[4] = { toStr(s.active), toStr(s.arrived), shortTime(s.avgTrip),
                            toStr((int)s.avgWait) + "s" };
    const char* labs[4] = { "in city", "arrived", "avg trip", "avg wait" };
    float cw = w / 4;
    for (int i = 0; i < 4; i++) {
        drawText(win, f, vals[i], x + i * cw, y, 15, Theme::TEXT, true);
        drawText(win, f, labs[i], x + i * cw, y + 19, 11, Theme::TEXT_DIM);
    }
    return y + 40;
}

void drawControlTab(sf::RenderWindow& win, const sf::Font* f, UiInput& ui, AppState& st,
                    Simulation& sim, MapView& map, float x, float y, float w, float bottom) {
    SimSummary s = sim.summary();

    // ---- simulation ----
    y = sectionTitle(win, f, "SIMULATION", x, y, w);
    float bw = (w - 12) / 4;
    if (button(win, f, ui, x, y, bw, 28, s.paused ? "Resume" : "Pause", s.paused, true, Theme::WARN)) sim.togglePause();
    if (button(win, f, ui, x + (bw + 4), y, bw, 28, "Slower", false, s.speed > MIN_SPEED)) sim.slower();
    if (button(win, f, ui, x + 2 * (bw + 4), y, bw, 28, "Faster", false, s.speed < MAX_SPEED)) sim.faster();
    if (button(win, f, ui, x + 3 * (bw + 4), y, bw, 28, "Reset", false, true, Theme::BAD)) {
        sim.reset();
        clearRoute(st, map);
        st.followId = -1;
        map.setFollow(-1);
        notify(st, "City reset.");
    }
    y += 34;
    drawText(win, f, "New vehicles", x, y + 6, 13, Theme::TEXT_DIM);
    if (button(win, f, ui, x + w - 150, y, 32, 26, "-", false, s.spawnRate > 0)) sim.setSpawnRate(s.spawnRate - 5 < 0 ? 0 : s.spawnRate - 5);
    centeredText(win, f, toStr((int)sim.getSpawnRate()) + " / min", x + w - 116, y, 82, 26, 13, Theme::TEXT, true);
    if (button(win, f, ui, x + w - 32, y, 32, 26, "+", false, s.spawnRate < 120)) sim.setSpawnRate(s.spawnRate + 5);
    y += 36;

    // ---- signals ----
    y = sectionTitle(win, f, "TRAFFIC SIGNALS", x, y, w);
    const char* sig[2] = { "Fixed Timer (20 s)", "Adaptive (smart)" };
    int ns = segmented(win, f, ui, x, y, w, 28, sig, 2, s.strategy == ADAPTIVE ? 1 : 0, Theme::GOOD);
    if (ns != (s.strategy == ADAPTIVE ? 1 : 0)) {
        sim.setStrategy(ns == 1 ? ADAPTIVE : FIXED_TIMER);
        notify(st, std::string("Signals: ") + SignalController::strategyName(sim.getStrategy()));
    }
    y += 38;

    // ---- scenarios ----
    y = sectionTitle(win, f, "SCENARIO  (restarts the city)", x, y, w);
    const char* scen[5] = { "Normal", "Rush", "Night", "Stadium", "Evacuate" };
    float sw = w / 5;
    for (int i = 0; i < 5; i++)
        if (button(win, f, ui, x + i * sw, y, sw - 3, 26, scen[i], sim.getScenario() == i, true, Theme::ACCENT, 11)) {
            sim.loadScenario((Scenario)i);
            clearRoute(st, map);
            st.followId = -1;
            map.setFollow(-1);
            notify(st, std::string("Scenario: ") + scenarioName((Scenario)i));
        }
    y += 36;

    // ---- chaos ----
    y = sectionTitle(win, f, "CHAOS", x, y, w);
    float cw = (w - 12) / 4;
    if (button(win, f, ui, x, y, cw, 28, s.chaosMode ? "Chaos ON" : "Chaos off", s.chaosMode, true, Theme::BAD)) {
        sim.toggleChaosMode();
        notify(st, sim.chaos().isChaosMode() ? "CHAOS MODE ON: random events every 2 minutes" : "Chaos Mode off");
    }
    if (button(win, f, ui, x + (cw + 4), y, cw, 28, "Surge +30")) { sim.surge(30); notify(st, lastEventText(sim)); }
    if (button(win, f, ui, x + 2 * (cw + 4), y, cw, 28, "Undo", false, sim.chaos().canUndo())) {
        sim.undo(); notify(st, "Undo: " + lastEventText(sim));
        if (st.routeB >= 0) recomputeRoute(st, sim, map);
    }
    if (button(win, f, ui, x + 3 * (cw + 4), y, cw, 28, "Redo", false, sim.chaos().canRedo())) {
        sim.redo(); notify(st, "Redo: " + lastEventText(sim));
        if (st.routeB >= 0) recomputeRoute(st, sim, map);
    }
    y += 38;

    // ---- map tool ----
    y = sectionTitle(win, f, "MAP TOOL  (left-click on the map)", x, y, w);
    int nt = segmented(win, f, ui, x, y, w, 28, TOOL_NAMES, TOOL_COUNT, st.tool, Theme::WARN, 11);
    if (nt != st.tool) {
        st.tool = nt;
        if (nt != TOOL_INSPECT) { map.setSelectedRoad(-1); map.setSelectedNode(-1); }
    }
    y += 38;

    // ---- tool details ----
    Graph& g = sim.graph();
    if (st.tool == TOOL_INSPECT) {
        std::string L[8];
        sf::Color C[8];
        int n = 0;
        if (map.getSelectedRoad() != -1) n = roadLines(sim, map.getSelectedRoad(), L, C);
        else if (map.getSelectedNode() != -1) n = nodeLines(sim, map.getSelectedNode(), L, C);
        if (n == 0) {
            drawText(win, f, "Click a road or intersection to pin its", x, y, 12, Theme::TEXT_DIM);
            drawText(win, f, "live details here. Hover for a quick look.", x, y + 17, 12, Theme::TEXT_DIM);
        }
        for (int i = 0; i < n; i++)
            drawText(win, f, L[i], x, y + i * 19.0f, i == 0 ? 14 : 12, C[i], i == 0);
    } else if (st.tool == TOOL_CLOSE) {
        drawText(win, f, "Click a road to CLOSE both directions.", x, y, 12, Theme::TEXT);
        drawText(win, f, "Click a closed road again to reopen it.", x, y + 17, 12, Theme::TEXT_DIM);
        drawText(win, f, "Affected vehicles are rerouted instantly.", x, y + 34, 12, Theme::TEXT_DIM);
        drawText(win, f, "Closed lanes now: " + toStr(s.closedRoads), x, y + 58, 13,
                 s.closedRoads ? Theme::BAD : Theme::TEXT, true);
    } else if (st.tool == TOOL_ACCIDENT) {
        drawText(win, f, "Click a lane: accident for 3 minutes.", x, y, 12, Theme::TEXT);
        drawText(win, f, "Vehicles on that lane stop; others reroute.", x, y + 17, 12, Theme::TEXT_DIM);
        drawText(win, f, "It clears by itself (or press Undo).", x, y + 34, 12, Theme::TEXT_DIM);
        drawText(win, f, "Active accidents: " + toStr(s.accidents), x, y + 58, 13,
                 s.accidents ? Theme::BAD : Theme::TEXT, true);
    } else if (st.tool == TOOL_ROUTE) {
        if (st.routeA == -1) {
            drawText(win, f, "Click the START intersection (A).", x, y, 12, Theme::TEXT);
        } else if (st.routeB == -1) {
            drawText(win, f, "From " + g.getIntersection(st.routeA).name + ": now click the destination (B).", x, y, 12, Theme::TEXT);
        } else {
            drawText(win, f, g.getIntersection(st.routeA).name + "  ->  " + g.getIntersection(st.routeB).name, x, y, 14, Theme::TEXT, true);
            float ry = y + 24;
            drawText(win, f, "Algorithm", x, ry, 11, Theme::TEXT_DIM);
            drawText(win, f, "Roads", x + 110, ry, 11, Theme::TEXT_DIM);
            drawText(win, f, "Time now", x + 165, ry, 11, Theme::TEXT_DIM);
            drawText(win, f, "Expanded", x + 245, ry, 11, Theme::TEXT_DIM);
            ry += 17;
            const char* an[3] = { "Dijkstra", "BFS", "DFS" };
            int rd[3] = { st.dijkstraRoads, st.bfsRoads, st.dfsRoads };
            int tm[3] = { st.dijkstraTime, st.bfsTime, st.dfsTime };
            int ex[3] = { st.dijkstraVisited, st.bfsVisited, st.dfsVisited };
            for (int i = 0; i < 3; i++) {
                sf::Color c = i == 0 ? Theme::ACCENT : Theme::TEXT;
                drawText(win, f, an[i], x, ry, 13, c, i == 0);
                if (tm[i] >= INF || rd[i] <= 0) drawText(win, f, "no route", x + 110, ry, 13, Theme::BAD);
                else {
                    drawText(win, f, toStr(rd[i]), x + 110, ry, 13, c);
                    drawText(win, f, shortTime(tm[i]), x + 165, ry, 13, c);
                    drawText(win, f, toStr(ex[i]), x + 245, ry, 13, c);
                }
                ry += 19;
            }
            ry += 6;
            bool ok = st.dijkstraTime < INF;
            float hw = (w - 8) / 3;
            if (button(win, f, ui, x, ry, hw, 26, "Send ambulance", false, ok, Theme::BAD, 11)) {
                int id = sim.spawnVehicle(st.routeA, st.routeB, AMBULANCE);
                if (id != -1) { st.followId = id; map.setFollow(id); notify(st, "Ambulance #" + toStr(id) + " dispatched - follow the white ring"); }
            }
            if (button(win, f, ui, x + hw + 4, ry, hw, 26, "Send car", false, ok, Theme::ACCENT, 11)) {
                int id = sim.spawnVehicle(st.routeA, st.routeB, CAR);
                if (id != -1) { st.followId = id; map.setFollow(id); notify(st, "Car #" + toStr(id) + " sent - follow the white ring"); }
            }
            if (button(win, f, ui, x + 2 * (hw + 4), ry, hw, 26, "Clear", false, true, Theme::ACCENT, 11)) clearRoute(st, map);
        }
    } else {
        drawText(win, f, "Click an intersection: " + toStr(SURGE_SIZE) + " vehicles", x, y, 12, Theme::TEXT);
        drawText(win, f, "head there at once (match at a stadium).", x, y + 17, 12, Theme::TEXT_DIM);
        drawText(win, f, "Watch queues and '+N' badges grow.", x, y + 34, 12, Theme::TEXT_DIM);
    }

    // followed vehicle status at the bottom
    if (st.followId != -1) {
        Vehicle* v = sim.vehicles().getVehicle(st.followId);
        if (v != NULL) {
            float fy = bottom - 46;
            drawRect(win, x, fy, w, 38, Theme::PANEL_LIGHT, Theme::BORDER, 1);
            drawText(win, f, Vehicle::typeName(v->type) + " #" + toStr(v->id) + "  " + Vehicle::stateName(v->state),
                     x + 8, fy + 3, 12, Theme::TEXT, true);
            drawText(win, f, "on " + sim.roadLabel(v->currentRoad) + ", " +
                     toStr((int)(sim.summary().time - v->spawnTime)) + " s so far", x + 8, fy + 20, 11, Theme::TEXT_DIM);
        }
    }
}

// ---------------------------------------------------------------
//  Map overlays: legend, hover tooltip, toast
// ---------------------------------------------------------------
void drawLegend(sf::RenderWindow& win, const sf::Font* f, const sf::FloatRect& area) {
    float w = 300, h = 86, x = area.left + 10, y = area.top + area.height - h - 10;
    drawRect(win, x, y, w, h, sf::Color(18, 20, 26, 215), Theme::BORDER, 1);
    x += 10; y += 8;
    for (int i = 0; i < 60; i++) drawRect(win, x + i * 2.0f, y + 4, 2, 8, congestionColor(i / 59.0));
    drawText(win, f, "road: free -> full   (L hides legend)", x + 128, y, 11, Theme::TEXT_DIM);
    y += 20;
    const char* names[4] = { "Car", "Bus", "Truck", "Ambulance" };
    sf::Color cols[4] = { Theme::CAR, Theme::BUS, Theme::TRUCK, Theme::BAD };
    float lx = x;
    for (int i = 0; i < 4; i++) {
        drawRect(win, lx, y + 4, 12, 7, cols[i]);
        lx += 16 + drawText(win, f, names[i], lx + 16, y, 11, Theme::TEXT_DIM) + 14;
    }
    y += 20;
    drawCircle(win, sf::Vector2f(x + 4, y + 7), 4, Theme::GOOD);
    drawCircle(win, sf::Vector2f(x + 14, y + 7), 4, Theme::WARN);
    drawCircle(win, sf::Vector2f(x + 24, y + 7), 4, Theme::BAD);
    drawText(win, f, "signal", x + 33, y, 11, Theme::TEXT_DIM);
    drawDashedLine(win, sf::Vector2f(x + 80, y + 7), sf::Vector2f(x + 112, y + 7), 3, 4, Theme::BAD);
    drawText(win, f, "closed", x + 117, y, 11, Theme::TEXT_DIM);
    drawRect(win, x + 165, y, 22, 15, withAlpha(Theme::WARN, 230));
    drawText(win, f, "+N", x + 168, y, 11, sf::Color::Black, true);
    drawText(win, f, "entering", x + 192, y, 11, Theme::TEXT_DIM);
}

void drawToast(sf::RenderWindow& win, const sf::Font* f, const AppState& st, const sf::FloatRect& area) {
    if (st.toastAge > 4.5f || st.toast.empty()) return;
    float alpha = st.toastAge < 3.5f ? 1.0f : 1.0f - (st.toastAge - 3.5f);
    float tw = textWidth(f, st.toast, 13);
    if (tw > area.width - 60) tw = area.width - 60;
    float x = area.left + (area.width - tw) / 2 - 14, y = area.top + 12;
    drawRect(win, x, y, tw + 28, 30, sf::Color(18, 20, 26, (sf::Uint8)(235 * alpha)),
             withAlpha(Theme::ACCENT, (sf::Uint8)(220 * alpha)), 1);
    drawText(win, f, st.toast, x + 14, y + 6, 13, withAlpha(Theme::TEXT, (sf::Uint8)(255 * alpha)));
}

// ---------------------------------------------------------------
int main() {
    Simulation sim;
    std::string err;
    if (!sim.init("city_map.txt", err)) {
        std::cout << "Could not load the map: " << err << "\n"
                  << "Put city_map.txt in the same folder as the .exe file.\nPress Enter to close...";
        std::cin.get();
        return 1;
    }
    sim.setSpeed(5);
    sim.setSpawnRate(20);

    sf::RenderWindow window(sf::VideoMode(1300, 780), "Chaos City - Intelligent Traffic Simulation");
    window.setFramerateLimit(60);
    sf::Font font;
    const sf::Font* f = loadGuiFont(font) ? &font : NULL;

    MapView map(sim);
    map.setArea(sf::FloatRect(0, 0, 1300 - PANEL_WIDTH, 780));
    map.fitToArea();
    AppState st;
    notify(st, "Welcome! Pick a map tool on the right, then click the map.");

    bool dragging = false;
    sf::Vector2f lastMouse;
    sf::Clock frameClock, totalClock;

#ifdef CHAOS_SCREENSHOT
    int shotFrames = 0;
    {
        Graph& g = sim.graph();
        sim.setSpawnRate(30);
        sim.runFor(900);
        sim.closeRoad(g.findIntersection("Saddar"), g.findIntersection("Karsaz"));
        sim.accident(g.findRoad(g.findIntersection("Karsaz"), g.findIntersection("Airport")), 600);
        sim.runFor(60);
        st.tool = TOOL_ROUTE;
        st.routeA = g.findIntersection("Tower");
        st.routeB = g.findIntersection("Malir");
        recomputeRoute(st, sim, map);
        st.followId = sim.spawnVehicle(st.routeA, st.routeB, AMBULANCE);
        map.setFollow(st.followId);
        sim.runFor(400);
        recomputeRoute(st, sim, map);
#ifdef CHAOS_SHOT_DSA
        sim.setChaosMode(true);
        sim.runFor(3000);
        st.tab = TAB_DSA;
        st.dsa.sub = CHAOS_SHOT_DSA;
        clearRoute(st, map);
        if (CHAOS_SHOT_DSA == 0) { st.dsa.sortKind = 0; st.dsa.sortSizeIdx = 3; st.dsa.sortPending = true; runPendingDsa(sim, st.dsa); }
        if (CHAOS_SHOT_DSA == 1) {
            st.dsa.searchPending = true; runPendingDsa(sim, st.dsa);
            st.dsa.routeSrc = sim.graph().findIntersection("Tower"); st.dsa.routeDst = sim.graph().findIntersection("Malir");
            st.dsa.routePending = true; runPendingDsa(sim, st.dsa);
        }
        if (CHAOS_SHOT_DSA == 2) { st.dsa.showMST = true; st.dsa.evacSource = sim.graph().findIntersection("Saddar"); st.dsa.showEvac = true; }
        if (CHAOS_SHOT_DSA == 3) { st.dsa.pattern = "accident"; }
#endif
#ifdef CHAOS_SHOT_DASH
        sim.setChaosMode(true);
        sim.runFor(3000);
        st.tab = TAB_DASHBOARD;
        st.dashSub = CHAOS_SHOT_DASH;
        st.dashMetric = 2;
        clearRoute(st, map);
#endif
        notify(st, "Ambulance #" + toStr(st.followId) + " dispatched - follow the white ring");
        sim.pause();
    }
#endif

    while (window.isOpen()) {
        UiInput ui;
        sf::Vector2i mp = sf::Mouse::getPosition(window);
        ui.mouse = sf::Vector2f((float)mp.x, (float)mp.y);
        bool mapClick = false;

        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed) window.close();
            else if (e.type == sf::Event::Resized) {
                float w = (float)e.size.width, h = (float)e.size.height;
                window.setView(sf::View(sf::FloatRect(0, 0, w, h)));
                map.setArea(sf::FloatRect(0, 0, w - PANEL_WIDTH, h));
                map.fitToArea();
            }
            else if (e.type == sf::Event::TextEntered && st.dsa.textFocus) {
                sf::Uint32 c = e.text.unicode;
                if (c == 8) { if (!st.dsa.pattern.empty()) st.dsa.pattern.erase(st.dsa.pattern.length() - 1); }
                else if (c == 13 || c == 27) st.dsa.textFocus = false;
                else if (c >= 32 && c < 127 && st.dsa.pattern.length() < 30) st.dsa.pattern += (char)c;
            }
            else if (e.type == sf::Event::KeyPressed && st.dsa.textFocus) {
                if (e.key.code == sf::Keyboard::Escape) st.dsa.textFocus = false;   // typing: no shortcuts
            }
            else if (e.type == sf::Event::KeyPressed) {
                switch (e.key.code) {
                    case sf::Keyboard::Space:    sim.togglePause(); break;
                    case sf::Keyboard::Add:
                    case sf::Keyboard::Equal:    sim.faster(); break;
                    case sf::Keyboard::Subtract:
                    case sf::Keyboard::Dash:     sim.slower(); break;
                    case sf::Keyboard::F:        map.fitToArea(); break;
                    case sf::Keyboard::L:        st.showLegend = !st.showLegend; break;
                    case sf::Keyboard::Z:        if (sim.undo()) notify(st, "Undo: " + lastEventText(sim)); break;
                    case sf::Keyboard::Y:        if (sim.redo()) notify(st, "Redo: " + lastEventText(sim)); break;
                    case sf::Keyboard::C:        sim.toggleChaosMode(); notify(st, lastEventText(sim)); break;
                    case sf::Keyboard::S:        sim.toggleStrategy();
                                                 notify(st, std::string("Signals: ") + SignalController::strategyName(sim.getStrategy()));
                                                 break;
                    case sf::Keyboard::Num1: case sf::Keyboard::Num2: case sf::Keyboard::Num3:
                    case sf::Keyboard::Num4: case sf::Keyboard::Num5:
                        st.tool = e.key.code - sf::Keyboard::Num1;
                        st.tab = TAB_CONTROL;
                        break;
                    case sf::Keyboard::Escape:
                        clearRoute(st, map);
                        map.setSelectedRoad(-1);
                        map.setSelectedNode(-1);
                        break;
                    default: break;
                }
            }
            else if (e.type == sf::Event::MouseWheelScrolled) {
                sf::Vector2f p((float)e.mouseWheelScroll.x, (float)e.mouseWheelScroll.y);
                if (map.contains(p)) map.zoomAt(e.mouseWheelScroll.delta > 0 ? 1.15f : 1 / 1.15f, p);
            }
            else if (e.type == sf::Event::MouseButtonPressed) {
                sf::Vector2f p((float)e.mouseButton.x, (float)e.mouseButton.y);
                if (e.mouseButton.button == sf::Mouse::Left) {
                    ui.clicked = true;
                    ui.mouse = p;
                    if (map.contains(p)) mapClick = true;
                } else if (map.contains(p)) { dragging = true; lastMouse = p; }
            }
            else if (e.type == sf::Event::MouseButtonReleased) dragging = false;
            else if (e.type == sf::Event::MouseMoved && dragging) {
                sf::Vector2f p((float)e.mouseMove.x, (float)e.mouseMove.y);
                map.panBy(p.x - lastMouse.x, p.y - lastMouse.y);
                lastMouse = p;
            }
        }

        float dt = frameClock.restart().asSeconds();
        sim.advance(dt);
        st.toastAge += dt;

        // keep the route live (traffic changes every second)
        if (st.routeB >= 0) {
            st.routeRefresh -= dt;
            if (st.routeRefresh <= 0) recomputeRoute(st, sim, map);
        }
        // followed vehicle arrived?
        if (st.followId != -1 && sim.vehicles().getVehicle(st.followId) == NULL) {
            std::string msg = "Vehicle #" + toStr(st.followId) + " left the city.";
            for (ListNode<TripRecord>* n = sim.vehicles().getTrips().getHead(); n != NULL; n = n->next)
                if (n->data.vehicleId == st.followId)
                    msg = Vehicle::typeName(n->data.type) + " #" + toStr(st.followId) + " ARRIVED in " +
                          shortTime(n->data.travelTime) + " (waited " +
                          toStr((int)n->data.waitTime) + " s, " + toStr(n->data.reroutes) + " reroutes)";
            notify(st, msg);
            st.followId = -1;
            map.setFollow(-1);
        }

        map.setHoverTint(st.tool == TOOL_CLOSE ? sf::Color(232, 72, 60, 200)
                       : st.tool == TOOL_ACCIDENT ? sf::Color(242, 190, 50, 210)
                       : sf::Color(255, 255, 255, 170));
        map.updateHover(ui.mouse);
        if (mapClick) handleMapClick(st, sim, map);

        float seconds = totalClock.getElapsedTime().asSeconds();
        sf::FloatRect area = map.getArea();
        updateDsaOverlays(sim, map, st.dsa, st.tab == TAB_DSA);
        window.clear(Theme::BG);
        map.draw(window, f, seconds);
        if (st.showLegend) drawLegend(window, f, area);
        drawToast(window, f, st, area);

        // hover tooltip on the map
        if (map.contains(ui.mouse)) {
            std::string L[8];
            sf::Color C[8];
            int n = 0;
            if (map.getHoverRoad() != -1) n = roadLines(sim, map.getHoverRoad(), L, C);
            else if (map.getHoverNode() != -1) n = nodeLines(sim, map.getHoverNode(), L, C);
            drawTooltip(window, f, ui.mouse, L, C, n, area);
        }

        // side panel
        float px = area.width, ph = area.height;
        drawRect(window, px, 0, PANEL_WIDTH, ph, Theme::PANEL);
        drawRect(window, px, 0, 1, ph, Theme::BORDER);
        float x = px + 16, w = PANEL_WIDTH - 32;
        float y = drawHeader(window, f, sim, x, w, seconds);
        st.tab = segmented(window, f, ui, x, y, w, 28, TAB_NAMES, 3, st.tab, Theme::ACCENT, 11);
        y += 40;
        if (st.tab == TAB_CONTROL) drawControlTab(window, f, ui, st, sim, map, x, y, w, ph);
        else if (st.tab == TAB_DASHBOARD) {
            int road = drawDashboardTab(window, f, ui, sim, st.dashSub, st.dashMetric, x, y, w, ph);
            if (road != -1) {                       // clicked in the congestion ranking
                st.tool = TOOL_INSPECT;
                map.setSelectedNode(-1);
                map.setSelectedRoad(road);
                notify(st, "Selected " + sim.roadLabel(road) + " - see it outlined on the map");
            }
        }
        else drawDsaTab(window, f, ui, sim, st.dsa, st.routeA, st.routeB, x, y, w, ph, seconds);

        window.display();

        // benchmarks run AFTER the frame showing "Running..." is on screen
        if (runPendingDsa(sim, st.dsa)) frameClock.restart();

#ifdef CHAOS_SCREENSHOT
        if (shotFrames == 2) {
            Graph& g = sim.graph();
            const Intersection& h = g.getIntersection(g.findIntersection("Airport"));
            sf::Mouse::setPosition(sf::Vector2i((int)map.toScreen(h.x, h.y).x, (int)map.toScreen(h.x, h.y).y), window);
        }
        if (++shotFrames == 6) {
            sf::Texture tx;
            tx.create(window.getSize().x, window.getSize().y);
            tx.update(window);
            tx.copyToImage().saveToFile(CHAOS_SCREENSHOT);
            window.close();
        }
#endif
    }
    return 0;
}
