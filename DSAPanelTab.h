// =====================================================================
//  Chaos City - DSAPanelTab.h
//  The DSA PANEL tab of the side panel. Four sub-views:
//
//   SORTING   - run the six sorting algorithms on real trip times or
//               random / sorted / reversed data; bars for time,
//               comparisons or moves (log scale).
//   SEARCH    - linear vs binary search vs hash table lookup;
//               BFS vs DFS vs Dijkstra on one trip;
//               all-pairs: V x Dijkstra vs Floyd-Warshall.
//   GRAPHS    - minimum spanning tree (Prim + Kruskal) drawn on the map;
//               evacuation order from a hotspot (topological sort)
//               shown as numbered badges; cycle check of the city.
//   STRINGS   - type a pattern: Naive / KMP / Rabin-Karp search the
//               event log and place names; KMP prefix table shown;
//               matching places glow on the map.
//
//  Benchmarks take a moment, so a button only sets a "pending" flag;
//  main() draws the frame (showing "Running...") and then calls
//  runPendingDsa() after window.display().
// =====================================================================
#ifndef DSAPANELTAB_H
#define DSAPANELTAB_H

#include "Simulation.h"
#include "GuiCommon.h"
#include "Widgets.h"
#include "Charts.h"
#include "MapView.h"

const char* const DSA_SUBS[4] = { "Sorting", "Search", "Graphs", "Strings" };
const int SORT_SIZES[6] = { 200, 500, 1000, 2000, 5000, 10000 };
const int SEARCH_SIZES[3] = { 1000, 10000, 100000 };

struct DsaTabState {
    int sub;
    // sorting
    int sortKind, sortSizeIdx, sortMetric;
    SortBenchResult sortRes[SORT_ALGORITHM_COUNT];
    bool sortValid, sortPending;
    BenchData sortRanKind;
    // searching + paths
    int searchSizeIdx;
    SearchBenchResult searchRes;
    bool searchValid, searchPending;
    RoutingCompareResult routeRes;
    int routeSrc, routeDst;
    bool routeValid, routePending;
    // graphs
    bool showMST, showEvac, pickEvac;
    int evacSource;
    // strings
    std::string pattern;
    bool textFocus;

    DsaTabState() : sub(0), sortKind(0), sortSizeIdx(3), sortMetric(0), sortValid(false),
        sortPending(false), sortRanKind(DATA_TRIP_TIMES), searchSizeIdx(1), searchValid(false),
        searchPending(false), routeSrc(-1), routeDst(-1), routeValid(false), routePending(false),
        showMST(false), showEvac(false), pickEvac(false), evacSource(-1), textFocus(false) {}
};

inline float dsaTitle(sf::RenderTarget& w, const sf::Font* f, const std::string& s, float x, float y, float width) {
    drawText(w, f, s, x, y, 12, Theme::ACCENT, true);
    drawRect(w, x, y + 18, width, 1, Theme::BORDER);
    return y + 25;
}

inline std::string fmtMicros(double us) {
    if (us >= 1000000) return toStr1(us / 1000000) + " s";
    if (us >= 1000) return toStr1(us / 1000) + " ms";
    if (us >= 10) return toStr((int)(us + 0.5)) + " us";
    std::stringstream ss;
    ss.setf(std::ios::fixed);
    ss.precision(us >= 0.1 ? 2 : 3);
    ss << us << " us";
    return ss.str();
}

inline std::string fmtCount(long long n) {          // 1234567 -> 1,234,567
    std::string s = toStr(n), r;
    int c = 0;
    for (int i = (int)s.length() - 1; i >= 0; i--) {
        r = s[i] + r;
        if (++c % 3 == 0 && i > 0 && s[i - 1] != '-') r = "," + r;
    }
    return r;
}

// ---------------- SORTING ----------------
inline void dsaSorting(sf::RenderTarget& win, const sf::Font* f, UiInput& ui, Simulation& sim,
                       DsaTabState& d, float x, float y, float w) {
    y = dsaTitle(win, f, "SORTING BENCHMARK", x, y, w);
    const char* kinds[4] = { "Real trips", "Random", "Sorted", "Reversed" };
    d.sortKind = segmented(win, f, ui, x, y, w, 26, kinds, 4, d.sortKind, Theme::ACCENT, 11);
    y += 34;
    drawText(win, f, "Items n", x, y + 5, 13, Theme::TEXT_DIM);
    if (button(win, f, ui, x + 70, y, 28, 26, "-", false, d.sortSizeIdx > 0)) d.sortSizeIdx--;
    centeredText(win, f, fmtCount(SORT_SIZES[d.sortSizeIdx]), x + 100, y, 64, 26, 13, Theme::TEXT, true);
    if (button(win, f, ui, x + 166, y, 28, 26, "+", false, d.sortSizeIdx < 5)) d.sortSizeIdx++;
    if (button(win, f, ui, x + w - 110, y, 110, 26, d.sortPending ? "Running..." : "Run all 6",
               d.sortPending, !d.sortPending, Theme::GOOD))
        d.sortPending = true;
    y += 34;
    if (d.sortKind == 0 && sim.vehicles().getTrips().size() < 50)
        drawText(win, f, "Few trips yet: values repeat to reach n.", x, y - 2, 11, Theme::WARN), y += 14;

    const char* metrics[3] = { "Time", "Comparisons", "Moves" };
    d.sortMetric = segmented(win, f, ui, x, y, w, 24, metrics, 3, d.sortMetric, Theme::WARN, 11);
    y += 32;

    const char* bigO[6] = { "O(n^2)", "O(n^2)", "O(n^2)", "O(n log n)", "O(n log n)", "O(n log n)" };
    sf::Color cols[6] = { Theme::BAD, Theme::BAD, Theme::WARN, Theme::GOOD, Theme::GOOD, Theme::ACCENT };
    if (!d.sortValid) {
        drawText(win, f, "Press 'Run all 6' to time Bubble, Selection,", x, y, 12, Theme::TEXT_DIM);
        drawText(win, f, "Insertion, Merge, Quick and Heap sort.", x, y + 17, 12, Theme::TEXT_DIM);
        return;
    }
    double mx = 0;
    for (int a = 0; a < SORT_ALGORITHM_COUNT; a++) {
        double v = d.sortMetric == 0 ? d.sortRes[a].microseconds : d.sortMetric == 1 ?
                   (double)d.sortRes[a].comparisons : (double)d.sortRes[a].moves;
        if (v > mx) mx = v;
    }
    int fastest = 0, slowest = 0;
    for (int a = 0; a < SORT_ALGORITHM_COUNT; a++) {
        const SortBenchResult& r = d.sortRes[a];
        double v = d.sortMetric == 0 ? r.microseconds : d.sortMetric == 1 ? (double)r.comparisons : (double)r.moves;
        std::string val = d.sortMetric == 0 ? fmtMicros(r.microseconds) : fmtCount((long long)v);
        drawText(win, f, sortName(r.algo), x, y, 12, Theme::TEXT, true);
        drawText(win, f, bigO[a], x + 112, y + 1, 11, Theme::TEXT_DIM);
        float vw = textWidth(f, val, 12, true);
        drawText(win, f, r.correct ? val : "WRONG!", x + w - vw, y, 12, r.correct ? Theme::TEXT : Theme::BAD, true);
        logBar(win, x, y + 18, w, 8, v, mx, cols[a]);
        y += 32;
        if (r.microseconds < d.sortRes[fastest].microseconds) fastest = a;
        if (r.microseconds > d.sortRes[slowest].microseconds) slowest = a;
    }
    double ratio = d.sortRes[fastest].microseconds > 0 ?
                   d.sortRes[slowest].microseconds / d.sortRes[fastest].microseconds : 0;
    y += 2;
    drawText(win, f, std::string("Fastest: ") + sortName(d.sortRes[fastest].algo), x, y, 13, Theme::GOOD, true);
    drawText(win, f, toStr((int)(ratio + 0.5)) + "x faster than " + sortName(d.sortRes[slowest].algo),
             x, y + 18, 12, Theme::GOOD);
    drawText(win, f, std::string(benchDataName(d.sortRanKind)) + ", n = " + fmtCount(d.sortRes[0].n) +
             ".  Log-scale bars (1 step = 10x).", x, y + 38, 11, Theme::TEXT_DIM);
}

// ---------------- SEARCH & PATHS ----------------
inline void dsaSearch(sf::RenderTarget& win, const sf::Font* f, UiInput& ui, Simulation& sim,
                      DsaTabState& d, int routeA, int routeB, float x, float y, float w) {
    Graph& g = sim.graph();
    y = dsaTitle(win, f, "FIND ONE VEHICLE ID AMONG n", x, y, w);
    const char* sizes[3] = { "1,000", "10,000", "100,000" };
    d.searchSizeIdx = segmented(win, f, ui, x, y, w - 104, 26, sizes, 3, d.searchSizeIdx, Theme::ACCENT, 11);
    if (button(win, f, ui, x + w - 96, y, 96, 26, d.searchPending ? "Running..." : "Run", d.searchPending,
               !d.searchPending, Theme::GOOD))
        d.searchPending = true;
    y += 36;
    if (d.searchValid) {
        const SearchBenchResult& r = d.searchRes;
        const char* nm[3] = { "Linear search", "Binary search", "Hash table" };
        const char* cx[3] = { "O(n)", "O(log n)", "O(1) avg" };
        double us[3] = { r.linearMicro, r.binaryMicro, r.hashMicro };
        std::string cmp[3] = { toStr((long long)r.linearAvgComparisons) + " comparisons",
                               toStr1(r.binaryAvgComparisons) + " comparisons", "1 hash + short chain" };
        sf::Color cols[3] = { Theme::BAD, Theme::WARN, Theme::GOOD };
        for (int i = 0; i < 3; i++) {
            drawText(win, f, nm[i], x, y, 12, Theme::TEXT, true);
            drawText(win, f, cx[i], x + 104, y + 1, 11, Theme::TEXT_DIM);
            std::string v = fmtMicros(us[i]);
            drawText(win, f, v, x + w - textWidth(f, v, 12, true), y, 12, Theme::TEXT, true);
            logBar(win, x, y + 18, w, 7, us[i], us[0] > us[1] ? us[0] : us[1], cols[i]);
            drawText(win, f, cmp[i], x, y + 27, 10, Theme::TEXT_DIM);
            y += 44;
        }
        drawText(win, f, "Per lookup, n = " + fmtCount(r.n) + (r.agree ? ". All three agree." : ". MISMATCH!"),
                 x, y, 11, r.agree ? Theme::TEXT_DIM : Theme::BAD);
        y += 22;
    } else {
        drawText(win, f, "Press Run to compare the three methods.", x, y, 12, Theme::TEXT_DIM);
        y += 26;
    }

    y = dsaTitle(win, f, "SHORTEST PATHS", x, y, w);
    int a = routeA, b = routeB;
    if (a < 0 || b < 0) { a = g.findIntersection("Tower"); b = g.findIntersection("Malir"); }
    std::string trip = g.getIntersection(a).name + " -> " + g.getIntersection(b).name;
    drawText(win, f, fitText(f, trip, 12, w - 110), x, y + 5, 12, Theme::TEXT, true);
    if (button(win, f, ui, x + w - 96, y, 96, 26, d.routePending ? "Running..." : "Run", d.routePending,
               !d.routePending, Theme::GOOD)) {
        d.routePending = true;
        d.routeSrc = a; d.routeDst = b;
    }
    y += 32;
    drawText(win, f, "(uses the Route tool's A and B if set)", x, y - 4, 10, Theme::TEXT_DIM);
    y += 12;
    if (!d.routeValid) return;
    const RoutingCompareResult& r = d.routeRes;
    drawText(win, f, "Algorithm", x, y, 11, Theme::TEXT_DIM);
    drawText(win, f, "Roads", x + 100, y, 11, Theme::TEXT_DIM);
    drawText(win, f, "Time now", x + 150, y, 11, Theme::TEXT_DIM);
    drawText(win, f, "Expanded", x + 230, y, 11, Theme::TEXT_DIM);
    y += 16;
    const char* an[3] = { "BFS", "DFS", "Dijkstra" };
    int rd[3] = { r.bfsRoads, r.dfsRoads, r.dijkstraRoads };
    int tm[3] = { r.bfsTime, r.dfsTime, r.dijkstraTime };
    int ex[3] = { r.bfs.nodesVisited, r.dfs.nodesVisited, r.dijkstra.nodesVisited };
    for (int i = 0; i < 3; i++) {
        sf::Color c = i == 2 ? Theme::ACCENT : Theme::TEXT;
        drawText(win, f, an[i], x, y, 12, c, i == 2);
        if (rd[i] <= 0 || tm[i] >= INF) drawText(win, f, "no route", x + 100, y, 12, Theme::BAD);
        else {
            drawText(win, f, toStr(rd[i]), x + 100, y, 12, c);
            std::string t = Simulation::formatTime(tm[i]);
            drawText(win, f, tm[i] < 3600 ? t.substr(2) : t, x + 150, y, 12, c);
            drawText(win, f, toStr(ex[i]), x + 230, y, 12, c);
        }
        y += 17;
    }
    y += 6;
    drawText(win, f, "All pairs (" + toStr(g.getNodeCount()) + " x " + toStr(g.getNodeCount()) + "):", x, y, 12, Theme::TEXT, true);
    y += 20;
    double ap[2] = { r.dijkstraAllMicro, r.floydMicro };
    const char* apn[2] = { "Dijkstra from every node", "Floyd-Warshall O(V^3)" };
    for (int i = 0; i < 2; i++) {
        drawText(win, f, apn[i], x, y, 11, Theme::TEXT_DIM);
        std::string v = fmtMicros(ap[i]);
        drawText(win, f, v, x + w - textWidth(f, v, 12, true), y, 12, Theme::TEXT, true);
        logBar(win, x, y + 16, w, 6, ap[i], ap[0] > ap[1] ? ap[0] : ap[1], i == 0 ? Theme::ACCENT : Theme::TRUCK);
        y += 28;
    }
}

// ---------------- GRAPHS ----------------
inline void dsaGraphs(sf::RenderTarget& win, const sf::Font* f, UiInput& ui, Simulation& sim,
                      DsaTabState& d, float x, float y, float w) {
    Graph& g = sim.graph();
    int V = g.getNodeCount();
    y = dsaTitle(win, f, "MINIMUM SPANNING TREE  (core road network)", x, y, w);
    if (button(win, f, ui, x, y, w, 26, d.showMST ? "Hide MST on map" : "Show MST on map", d.showMST, true,
               sf::Color(0, 200, 175)))
        d.showMST = !d.showMST;
    y += 34;
    MSTEdge* e1 = new MSTEdge[V];
    MSTEdge* e2 = new MSTEdge[V];
    MSTResult p = MST::prim(g, e1), k = MST::kruskal(g, e2);
    int net = MST::networkLength(g);
    delete[] e1;
    delete[] e2;
    drawText(win, f, "Prim (adjacency matrix)", x, y, 12, Theme::TEXT);
    std::string pv = toStr1(p.totalWeight / 1000.0) + " km, " + fmtCount(p.work) + " cells";
    drawText(win, f, pv, x + w - textWidth(f, pv, 12, true), y, 12, Theme::TEXT, true);
    y += 18;
    drawText(win, f, "Kruskal (sort + union-find)", x, y, 12, Theme::TEXT);
    std::string kv = toStr1(k.totalWeight / 1000.0) + " km, " + toStr(k.work) + " edges";
    drawText(win, f, kv, x + w - textWidth(f, kv, 12, true), y, 12, Theme::TEXT, true);
    y += 20;
    bool same = p.totalWeight == k.totalWeight;
    drawText(win, f, same ? "Both give the same total length." : "Totals differ!", x, y, 11, same ? Theme::GOOD : Theme::BAD);
    y += 16;
    std::string keep = "Keeps " + toStr1(k.totalWeight / 1000.0) + " of " + toStr1(net / 1000.0) + " km open road (" +
                       toStr(net ? 100 * k.totalWeight / net : 0) + "%)";
    drawText(win, f, keep, x, y, 11, Theme::TEXT_DIM);
    y += 16;
    drawText(win, f, k.components == 1 ? "City connected: 1 component." :
             "Closures split the city: " + toStr(k.components) + " components (a forest).",
             x, y, 11, k.components == 1 ? Theme::TEXT_DIM : Theme::BAD);
    y += 28;

    y = dsaTitle(win, f, "EVACUATION ORDER  (topological sort)", x, y, w);
    std::string hs = d.evacSource >= 0 ? g.getIntersection(d.evacSource).name : std::string("(none)");
    drawText(win, f, "Hotspot:", x, y + 5, 12, Theme::TEXT_DIM);
    drawText(win, f, hs, x + 62, y + 5, 13, d.evacSource >= 0 ? Theme::BAD : Theme::TEXT_DIM, true);
    if (button(win, f, ui, x + w - 112, y, 112, 26, d.pickEvac ? "Click the map..." : "Pick on map", d.pickEvac,
               true, Theme::WARN, 11))
        d.pickEvac = !d.pickEvac;
    y += 32;
    if (button(win, f, ui, x, y, w, 26, d.showEvac ? "Hide order on map" : "Show order on map", d.showEvac,
               d.evacSource >= 0, Theme::BAD))
        d.showEvac = !d.showEvac;
    y += 34;
    if (d.evacSource >= 0) {
        int* dist = new int[V];
        int* order = new int[V];
        int* order2 = new int[V];
        DirectedGraph dag;
        TopoSort::evacuationDag(g, d.evacSource, true, dag, dist);
        bool okK = TopoSort::kahn(dag, order) && TopoSort::isValidOrder(dag, order);
        bool okD = TopoSort::dfs(dag, order2) && TopoSort::isValidOrder(dag, order2);
        std::string seq;
        int rank = 1;
        for (int i = 0; i < V; i++) {
            if (dist[order[i]] == INF) continue;
            if (!seq.empty()) seq += "  ";
            seq += toStr(rank++) + "." + g.getIntersection(order[i]).name;
        }
        y = drawWrapped(win, f, seq, x, y, w, 12, Theme::TEXT);
        y += 4;
        drawText(win, f, toStr(dag.edgeCount()) + " outward roads.  Kahn " + (okK ? "valid" : "FAILED") +
                 ", DFS " + (okD ? "valid" : "FAILED"), x, y, 11, okK && okD ? Theme::GOOD : Theme::BAD);
        y += 16;
        delete[] dist; delete[] order; delete[] order2;
    } else {
        drawText(win, f, "Pick a hotspot: intersections are then ordered", x, y, 11, Theme::TEXT_DIM);
        drawText(win, f, "so traffic always flows away from it.", x, y + 15, 11, Theme::TEXT_DIM);
        y += 32;
    }
    DirectedGraph whole;
    TopoSort::fromCity(g, whole);
    int* tmp = new int[V];
    bool dag = TopoSort::kahn(whole, tmp);
    delete[] tmp;
    drawText(win, f, dag ? "Whole city: no cycles (unusual!)" : "Whole city: cycle found -> no topological order",
             x, y + 4, 11, Theme::TEXT_DIM);
}

// ---------------- STRINGS ----------------
inline void dsaStrings(sf::RenderTarget& win, const sf::Font* f, UiInput& ui, Simulation& sim,
                       DsaTabState& d, float x, float y, float w, float bottom, float seconds) {
    Graph& g = sim.graph();
    y = dsaTitle(win, f, "SEARCH EVENT LOG & PLACE NAMES", x, y, w);
    textBox(win, f, ui, x, y, w, 30, d.pattern, "Click and type, e.g. accident, Saddar, kar", d.textFocus, seconds);
    y += 38;
    if (d.pattern.empty()) {
        drawText(win, f, "Three string-matching algorithms run on every", x, y, 11, Theme::TEXT_DIM);
        drawText(win, f, "event-log line as you type. Try 'accident'.", x, y + 15, 11, Theme::TEXT_DIM);
        return;
    }
    // KMP prefix table
    std::string low = StringMatch::toLower(d.pattern);
    int m = (int)low.length();
    int* lps = new int[m];
    StringMatch::buildLPS(low, lps);
    std::string lp = "KMP prefix table:";
    for (int i = 0; i < m && i < 20; i++) lp += " " + toStr(lps[i]);
    delete[] lps;
    drawText(win, f, lp, x, y, 11, Theme::TEXT_DIM);
    y += 20;

    MatchStats ms[MATCH_ALGORITHM_COUNT];
    int hits = sim.dsa().compareMatchers(d.pattern, ms);
    drawText(win, f, "Algorithm", x, y, 11, Theme::TEXT_DIM);
    drawText(win, f, "Char comparisons", x + 100, y, 11, Theme::TEXT_DIM);
    drawText(win, f, "Spurious", x + 230, y, 11, Theme::TEXT_DIM);
    y += 16;
    long long mx = 1;
    for (int a = 0; a < MATCH_ALGORITHM_COUNT; a++) if (ms[a].comparisons > mx) mx = ms[a].comparisons;
    sf::Color cols[3] = { Theme::BAD, Theme::GOOD, Theme::ACCENT };
    for (int a = 0; a < MATCH_ALGORITHM_COUNT; a++) {
        drawText(win, f, matchName((MatchAlgorithm)a), x, y, 12, Theme::TEXT, true);
        drawText(win, f, fmtCount(ms[a].comparisons), x + 100, y, 12, Theme::TEXT);
        drawText(win, f, a == 2 ? toStr(ms[a].spuriousHits) : std::string("-"), x + 230, y, 12, Theme::TEXT_DIM);
        hBar(win, x, y + 17, w, 5, ms[a].comparisons / (double)mx, cols[a]);
        y += 28;
    }
    drawText(win, f, toStr(hits) + " of " + toStr(sim.chaos().eventCount()) + " log lines match; " +
             toStr(ms[0].matches) + " occurrences.", x, y, 11, Theme::TEXT_DIM);
    y += 15;
    drawText(win, f, "KMP guarantees O(n+m); naive is only slow on", x, y, 11, Theme::TEXT_DIM);
    drawText(win, f, "repetitive text. Rabin-Karp checks hash hits only.", x, y + 14, 11, Theme::TEXT_DIM);
    y += 36;

    LinkedList<int> places;
    sim.dsa().findPlaces(d.pattern, KMP_MATCH, places);
    std::string pl;
    for (ListNode<int>* n = places.getHead(); n != NULL; n = n->next)
        pl += (pl.empty() ? "" : ", ") + g.getIntersection(n->data).name;
    drawText(win, f, "Places:", x, y, 12, Theme::TEXT_DIM);
    y = drawWrapped(win, f, pl.empty() ? std::string("none") : pl + "  (glowing on map)", x + 52, y, w - 52, 12,
                    pl.empty() ? Theme::TEXT_DIM : Theme::WARN);
    y += 8;

    y = dsaTitle(win, f, "MATCHING EVENTS (newest first)", x, y, w);
    LinkedList<ChaosEvent> found;
    MatchStats s;
    sim.dsa().searchEvents(d.pattern, KMP_MATCH, found, s);
    int room = (int)((bottom - y - 8) / 17);
    int total = found.size(), skip = total > room ? total - room : 0, i = 0;
    const ChaosEvent** rows = new const ChaosEvent*[room > 0 ? room : 1];
    int cnt = 0;
    for (ListNode<ChaosEvent>* n = found.getHead(); n != NULL; n = n->next, i++)
        if (i >= skip && cnt < room) rows[cnt++] = &n->data;
    for (int r = cnt - 1; r >= 0; r--) {
        std::string t = Simulation::formatTime(rows[r]->time);
        drawText(win, f, rows[r]->time < 3600 ? t.substr(2) : t, x, y, 11, Theme::TEXT_DIM);
        drawText(win, f, fitText(f, rows[r]->text, 11, w - 46), x + 44, y, 11, Theme::TEXT);
        y += 17;
    }
    delete[] rows;
}

// ---------------- tab entry points ----------------
inline void drawDsaTab(sf::RenderTarget& win, const sf::Font* f, UiInput& ui, Simulation& sim,
                       DsaTabState& d, int routeA, int routeB, float x, float y, float w,
                       float bottom, float seconds) {
    d.sub = segmented(win, f, ui, x, y, w, 26, DSA_SUBS, 4, d.sub, Theme::TRUCK, 11);
    y += 36;
    if (d.sub != 3) d.textFocus = false;
    if (d.sub == 0) dsaSorting(win, f, ui, sim, d, x, y, w);
    else if (d.sub == 1) dsaSearch(win, f, ui, sim, d, routeA, routeB, x, y, w);
    else if (d.sub == 2) dsaGraphs(win, f, ui, sim, d, x, y, w);
    else dsaStrings(win, f, ui, sim, d, x, y, w, bottom, seconds);
}

// Runs a benchmark that a button asked for. Returns true if one ran
// (main then restarts its frame clock so the simulation does not jump).
inline bool runPendingDsa(Simulation& sim, DsaTabState& d) {
    if (d.sortPending) {
        d.sortRanKind = (BenchData)d.sortKind;
        sim.dsa().benchmarkSorts(d.sortRanKind, SORT_SIZES[d.sortSizeIdx], d.sortRes);
        d.sortValid = true;
        d.sortPending = false;
        return true;
    }
    if (d.searchPending) {
        d.searchRes = sim.dsa().benchmarkSearch(SEARCH_SIZES[d.searchSizeIdx], 1000);
        d.searchValid = true;
        d.searchPending = false;
        return true;
    }
    if (d.routePending) {
        d.routeRes = sim.dsa().compareRouting(d.routeSrc, d.routeDst);
        d.routeValid = true;
        d.routePending = false;
        return true;
    }
    return false;
}

// Map overlays that belong to this tab (cleared when the tab is closed)
inline void updateDsaOverlays(Simulation& sim, MapView& map, DsaTabState& d, bool tabOpen) {
    Graph& g = sim.graph();
    int V = g.getNodeCount();
    if (tabOpen && d.showMST) {
        MSTEdge* e = new MSTEdge[V];
        MSTResult k = MST::kruskal(g, e);
        map.setMST(e, k.edgeCount);
        delete[] e;
    } else map.clearMST();

    if (tabOpen && d.showEvac && d.evacSource >= 0) {
        int* dist = new int[V];
        int* order = new int[V];
        DirectedGraph dag;
        TopoSort::evacuationDag(g, d.evacSource, true, dag, dist);
        if (TopoSort::kahn(dag, order)) map.setEvacOrder(order, V, dist, d.evacSource);
        delete[] dist;
        delete[] order;
    } else map.clearEvac();

    if (tabOpen && d.sub == 3 && !d.pattern.empty()) {
        LinkedList<int> places;
        sim.dsa().findPlaces(d.pattern, KMP_MATCH, places);
        map.setHighlights(places);
    } else map.clearHighlights();
}

#endif
