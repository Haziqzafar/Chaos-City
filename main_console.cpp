// =====================================================================
//  CHAOS CITY - Console version
//  Interactive menu over the Simulation class. Works without SFML, so it
//  is also a backup for the demo. Keep city_map.txt in the same folder.
// =====================================================================
#include <iostream>
#include <string>
#include <sstream>
#include <limits>
#include "Simulation.h"

using namespace std;

Simulation sim;

// ---------------------------------------------------------------
//  Input helpers (never crash on bad input)
// ---------------------------------------------------------------
int readInt(const string& prompt, int lo, int hi) {
    while (true) {
        cout << prompt;
        int x;
        if (cin >> x && x >= lo && x <= hi) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return x;
        }
        if (cin.eof()) return lo;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "  Please enter a number from " << lo << " to " << hi << ".\n";
    }
}

string readLine(const string& prompt) {
    cout << prompt;
    string s;
    getline(cin, s);
    return s;
}

void listPlaces() {
    Graph& g = sim.graph();
    for (int v = 0; v < g.getNodeCount(); v++) {
        cout << "  ";
        cout.width(2); cout << v << ". ";
        cout.width(15); cout << left << g.getIntersection(v).name << right;
        if (v % 3 == 2) cout << "\n";
    }
    cout << "\n";
}

int pickPlace(const string& prompt) {
    return readInt(prompt + " (number): ", 0, sim.graph().getNodeCount() - 1);
}

// Choose a road by its two ends; returns road id or -1
int pickRoad(int& u, int& v) {
    listPlaces();
    u = pickPlace("  From");
    v = pickPlace("  To  ");
    int r = sim.graph().findRoad(u, v);
    if (r == -1) cout << "  There is no road " << sim.graph().getIntersection(u).name
                      << " -> " << sim.graph().getIntersection(v).name << ".\n";
    return r;
}

void line() { cout << "------------------------------------------------------------\n"; }

// ---------------------------------------------------------------
//  Screens
// ---------------------------------------------------------------
void showStatus() {
    SimSummary s = sim.summary();
    line();
    cout << " Time " << Simulation::formatTime(s.time)
         << " | Signals: " << SignalController::strategyName(s.strategy)
         << " | Chaos Mode: " << (s.chaosMode ? "ON" : "off")
         << " | Spawn " << s.spawnRate << "/min\n";
    cout << " Vehicles: " << s.active << " active, " << s.arrived << " arrived, "
         << s.queued << " at lights, " << s.entering << " waiting to enter\n";
    cout << " Trips: avg " << (int)s.avgTrip << " s, avg wait " << (int)s.avgWait
         << " s | Speed now " << (int)s.avgSpeedKmh << " km/h | Congestion "
         << (int)(s.avgCongestion * 100) << "%\n";
    cout << " Chaos: " << s.accidents << " accidents, " << s.closedRoads
         << " closed road directions, " << s.rerouted << " reroutes so far\n";
    line();
}

void showMap() {
    Graph& g = sim.graph();
    SignalController& sc = sim.signals();
    VehicleManager& vm = sim.vehicles();
    ChaosManager& cm = sim.chaos();
    cout << "  Road                              Cars  Full  Queue  Light  State\n";
    for (int r = 0; r < g.getRoadCount(); r++) {
        const Road* road = g.getRoad(r);
        string label = sim.roadLabel(r);
        cout << "  ";
        cout.width(32); cout << left << label << right;
        cout.width(6); cout << road->vehiclesOnRoad;
        cout.width(5); cout << (int)(road->congestionRatio() * 100) << "%";
        cout.width(7); cout << vm.queueLength(r);
        LightColor c = sc.colorOf(r);
        cout << "  " << (c == LIGHT_GREEN ? "GREEN " : c == LIGHT_YELLOW ? "YELLOW" : "red   ");
        if (cm.hasAccident(r)) cout << " ACCIDENT";
        else if (cm.isClosed(r)) cout << " CLOSED";
        cout << "\n";
    }
}

void routeFinder() {
    listPlaces();
    int a = pickPlace("  From"), b = pickPlace("  To  ");
    RoutingCompareResult rc = sim.dsa().compareRouting(a, b);
    LinkedList<int> route;
    SearchStats st;
    Graph& g = sim.graph();
    cout << "\n  Algorithm  Roads  Travel time  Intersections expanded\n";
    cout << "  BFS        "; cout.width(5); cout << rc.bfsRoads; cout.width(12); cout << rc.bfsTime << " s"; cout.width(10); cout << rc.bfs.nodesVisited << "\n";
    cout << "  DFS        "; cout.width(5); cout << rc.dfsRoads; cout.width(12); cout << rc.dfsTime << " s"; cout.width(10); cout << rc.dfs.nodesVisited << "\n";
    cout << "  Dijkstra   "; cout.width(5); cout << rc.dijkstraRoads; cout.width(12); cout << rc.dijkstraTime << " s"; cout.width(10); cout << rc.dijkstra.nodesVisited << "\n";
    int t = GraphAlgorithms::dijkstraRoute(g, a, b, true, route, st);
    if (t == INF) cout << "\n  No route: the destination is cut off.\n";
    else cout << "\n  Best route now (live traffic): " << GraphAlgorithms::routeToString(g, route)
              << "\n  " << GraphAlgorithms::routeLength(g, route) / 1000.0 << " km, about "
              << t / 60 << " min " << t % 60 << " s\n";
}

void analyticsScreen() {
    Analytics& an = sim.analytics();
    Graph& g = sim.graph();
    if (an.tripCount() == 0) { cout << "  No finished trips yet. Run the simulation first.\n"; return; }
    cout << "  Finished trips: " << an.tripCount() << " (AVL tree height " << an.getTree().height() << ")\n";
    cout << "  Average " << (int)an.avgTravelTime() << " s | median " << (int)an.medianTrip()
         << " s | 90th percentile " << (int)an.percentile(90) << " s\n";
    cout << "  Fastest " << (int)an.fastestTrip() << " s | slowest " << (int)an.slowestTrip()
         << " s | avg wait " << (int)an.avgWaitTime() << " s | avg " << an.avgTripKm() << " km\n";
    cout << "  By type:  Car " << (int)an.avgTimeByType(CAR) << " s (" << an.countByType(CAR)
         << ")  Bus " << (int)an.avgTimeByType(BUS) << " s (" << an.countByType(BUS)
         << ")  Truck " << (int)an.avgTimeByType(TRUCK) << " s (" << an.countByType(TRUCK)
         << ")  Ambulance " << (int)an.avgTimeByType(AMBULANCE) << " s (" << an.countByType(AMBULANCE) << ")\n";

    int buckets[6];
    an.histogram(buckets, 6, 600);
    int maxB = 1;
    for (int b = 0; b < 6; b++) if (buckets[b] > maxB) maxB = buckets[b];
    cout << "\n  Trip time histogram:\n";
    const char* labels[6] = { " 0-10 min", "10-20 min", "20-30 min", "30-40 min", "40-50 min", "  50+ min" };
    for (int b = 0; b < 6; b++) {
        cout << "  " << labels[b] << " |" << string(buckets[b] * 40 / maxB, '#') << " " << buckets[b] << "\n";
    }

    int top[5];
    SortStats st;
    int k = an.topCongestedRoads(top, 5, QUICK_SORT, st);
    cout << "\n  Most congested roads right now:\n";
    for (int i = 0; i < k; i++)
        cout << "   " << i + 1 << ". " << sim.roadLabel(top[i]) << "  "
             << (int)(g.getRoad(top[i])->congestionRatio() * 100) << "% full\n";

    int id = readInt("\n  Look up a finished trip by vehicle id (0 to skip): ", 0, 100000000);
    if (id > 0) {
        TripRecord tr;
        int comps;
        if (an.findTrip(id, tr, comps))
            cout << "  #" << id << " " << Vehicle::typeName(tr.type) << ": " << g.getIntersection(tr.origin).name
                 << " -> " << g.getIntersection(tr.destination).name << ", " << (int)tr.travelTime << " s, waited "
                 << (int)tr.waitTime << " s, " << tr.reroutes << " reroutes (binary search: " << comps << " comparisons)\n";
        else cout << "  No finished trip with id " << id << " (" << comps << " comparisons).\n";
    }
}

void dsaPanel() {
    cout << "  1. Sorting benchmark      2. Searching benchmark\n"
         << "  3. Search the event log   4. Minimum spanning tree\n"
         << "  5. Evacuation order (topological sort)\n";
    int c = readInt("  Choice: ", 1, 5);
    Graph& g = sim.graph();
    if (c == 1) {
        cout << "  Data: 0 real trip times, 1 random, 2 sorted, 3 reversed\n";
        BenchData d = (BenchData)readInt("  Data: ", 0, 3);
        int n = readInt("  How many items (100-20000): ", 100, 20000);
        SortBenchResult res[SORT_ALGORITHM_COUNT];
        cout << "  Running...\n";
        sim.dsa().benchmarkSorts(d, n, res);
        cout << "\n  " << benchDataName(d) << ", n = " << n << "\n";
        cout << "  Algorithm         Comparisons       Moves     Time (us)\n";
        for (int a = 0; a < SORT_ALGORITHM_COUNT; a++) {
            cout << "  ";
            cout.width(15); cout << left << sortName(res[a].algo) << right;
            cout.width(14); cout << res[a].comparisons;
            cout.width(12); cout << res[a].moves;
            cout.width(14); cout << (long)res[a].microseconds << (res[a].correct ? "" : "  WRONG!") << "\n";
        }
    } else if (c == 2) {
        int n = readInt("  How many vehicle ids (100-100000): ", 100, 100000);
        SearchBenchResult r = sim.dsa().benchmarkSearch(n, 1000);
        cout << "  Linear search: " << r.linearAvgComparisons << " comparisons, " << r.linearMicro << " us per lookup\n";
        cout << "  Binary search: " << r.binaryAvgComparisons << " comparisons, " << r.binaryMicro << " us per lookup\n";
        cout << "  Hash table   : " << r.hashMicro << " us per lookup\n";
    } else if (c == 3) {
        string p = readLine("  Search for: ");
        MatchStats ms[MATCH_ALGORITHM_COUNT];
        int hits = sim.dsa().compareMatchers(p, ms);
        LinkedList<ChaosEvent> found;
        MatchStats s;
        sim.dsa().searchEvents(p, KMP_MATCH, found, s);
        for (ListNode<ChaosEvent>* n = found.getHead(); n; n = n->next)
            cout << "  [" << Simulation::formatTime(n->data.time) << "] " << n->data.text << "\n";
        cout << "  " << hits << " entries. Character comparisons: Naive " << ms[0].comparisons
             << ", KMP " << ms[1].comparisons << ", Rabin-Karp " << ms[2].comparisons
             << " (" << ms[2].spuriousHits << " spurious hash hits)\n";
    } else if (c == 4) {
        int V = g.getNodeCount();
        MSTEdge* e1 = new MSTEdge[V];
        MSTEdge* e2 = new MSTEdge[V];
        MSTResult p = MST::prim(g, e1), k = MST::kruskal(g, e2);
        cout << "  Prim (matrix): " << p.totalWeight << " m | Kruskal (sort + union-find): " << k.totalWeight << " m\n";
        cout << "  Keeps " << k.totalWeight / 1000.0 << " km of " << MST::networkLength(g) / 1000.0
             << " km open roads, " << k.components << " component(s):\n";
        for (int i = 0; i < k.edgeCount; i++)
            cout << "   " << g.getIntersection(e2[i].u).name << " - " << g.getIntersection(e2[i].v).name
                 << " (" << e2[i].weight << " m)\n";
        delete[] e1;
        delete[] e2;
    } else {
        listPlaces();
        int src = pickPlace("  Hotspot");
        int V = g.getNodeCount();
        int* dist = new int[V];
        int* order = new int[V];
        DirectedGraph dag;
        TopoSort::evacuationDag(g, src, true, dag, dist);
        if (TopoSort::kahn(dag, order)) {
            cout << "  Clear intersections in this order (" << dag.edgeCount() << " outward roads):\n  ";
            for (int i = 0; i < V; i++) {
                if (dist[order[i]] == INF) continue;
                cout << g.getIntersection(order[i]).name << " ";
            }
            cout << "\n";
        }
        delete[] dist;
        delete[] order;
    }
}

void showLog() {
    int n = 0;
    for (ListNode<ChaosEvent>* p = sim.chaos().eventLogHead(); p; p = p->next, n++)
        cout << "  [" << Simulation::formatTime(p->data.time) << "] " << p->data.text << "\n";
    if (n == 0) cout << "  (no events yet)\n";
}

int main() {
    string err;
    if (!sim.init("city_map.txt", err)) {
        cout << "Cannot start: " << err << "\nPut city_map.txt next to the program.\n";
        cin.get();
        return 1;
    }
    cout << "=============================================\n"
         << "   CHAOS CITY - Intelligent Traffic Simulation\n"
         << "=============================================\n";
    while (true) {
        showStatus();
        cout << "  1 Run          2 Road table     3 Route finder   4 Analytics\n"
             << "  5 Close road   6 Reopen road    7 Accident       8 Surge\n"
             << "  9 Undo        10 Redo          11 Chaos Mode    12 Signal strategy\n"
             << " 13 Spawn rate  14 DSA panel     15 Event log     16 Scenario\n"
             << " 17 Report file 18 Reset          0 Exit\n";
        int c = readInt("Choice: ", 0, 18);
        if (cin.eof() || c == 0) break;
        switch (c) {
            case 1: {
                int m = readInt("  Minutes to simulate (1-240): ", 1, 240);
                sim.runFor(m * 60.0);
                break;
            }
            case 2: showMap(); break;
            case 3: routeFinder(); break;
            case 4: analyticsScreen(); break;
            case 5: {
                int u, v;
                if (pickRoad(u, v) != -1) {
                    bool both = readInt("  1 = both directions, 0 = this direction only: ", 0, 1) == 1;
                    cout << (sim.closeRoad(u, v, both) ? "  Closed.\n" : "  Already closed.\n");
                }
                break;
            }
            case 6: {
                int u, v;
                if (pickRoad(u, v) != -1) cout << (sim.reopenRoad(u, v, true) ? "  Reopened.\n" : "  It was not closed.\n");
                break;
            }
            case 7: {
                int u, v, r = pickRoad(u, v);
                if (r != -1) sim.accident(r, readInt("  Duration in seconds (30-1800): ", 30, 1800));
                break;
            }
            case 8: {
                int kind = readInt("  0 random, 1 everyone TO a place, 2 everyone FROM a place: ", 0, 2);
                int place = 0;
                if (kind > 0) { listPlaces(); place = pickPlace("  Place"); }
                int n = readInt("  How many vehicles (1-500): ", 1, 500);
                int made = kind == 0 ? sim.surge(n) : kind == 1 ? sim.surgeTo(place, n) : sim.surgeFrom(place, n);
                cout << "  " << made << " vehicles added.\n";
                break;
            }
            case 9:  cout << (sim.undo() ? "  Undone.\n" : "  Nothing to undo.\n"); break;
            case 10: cout << (sim.redo() ? "  Redone.\n" : "  Nothing to redo.\n"); break;
            case 11: sim.toggleChaosMode(); break;
            case 12: sim.toggleStrategy(); break;
            case 13: sim.setSpawnRate(readInt("  Vehicles per minute (0-120): ", 0, 120)); break;
            case 14: dsaPanel(); break;
            case 15: showLog(); break;
            case 16: {
                for (int s = 0; s <= SCENARIO_EVACUATION; s++) cout << "  " << s << ". " << scenarioName((Scenario)s) << "\n";
                sim.loadScenario((Scenario)readInt("  Scenario: ", 0, SCENARIO_EVACUATION));
                break;
            }
            case 17:
                cout << (sim.writeReport("chaos_city_report.txt") ? "  Saved chaos_city_report.txt\n" : "  Could not write the file.\n");
                break;
            case 18: sim.reset(); cout << "  City reset.\n"; break;
        }
    }
    cout << "Goodbye!\n";
    return 0;
}
