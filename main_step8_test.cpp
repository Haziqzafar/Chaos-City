// =====================================================================
//  Chaos City - Step 8 test driver: DSA panel algorithms
//  String matching, Union-Find, MST (Prim + Kruskal), topological sort,
//  benchmarks. Keep city_map.txt in the same folder.
// =====================================================================
#include <iostream>
#include <string>
#include "Graph.h"
#include "VehicleManager.h"
#include "SignalController.h"
#include "ChaosManager.h"
#include "StringMatch.h"
#include "UnionFind.h"
#include "MST.h"
#include "TopoSort.h"
#include "DSAPanel.h"

using namespace std;

int passed = 0, failed = 0;

void check(bool condition, const string& testName) {
    if (condition) { passed++; cout << "  [PASS] " << testName << "\n"; }
    else           { failed++; cout << "  [FAIL] " << testName << "\n"; }
}

void testStringMatching() {
    cout << "\n== String matching ==\n";
    int lps[9];
    StringMatch::buildLPS("ABABCABAB", lps);
    int expected[9] = { 0, 0, 1, 2, 0, 1, 2, 3, 4 };
    bool lpsOk = true;
    for (int i = 0; i < 9; i++) if (lps[i] != expected[i]) lpsOk = false;
    check(lpsOk, "KMP LPS table of ABABCABAB = 0 0 1 2 0 1 2 3 4");

    MatchStats s;
    int pos[10];
    check(StringMatch::kmp("aaaaa", "aaa", pos, 10, s) == 3 && pos[2] == 2, "overlapping matches: 'aaa' in 'aaaaa' = 3");

    // Random texts over {a,b}: lots of partial matches. All three must agree.
    SimpleRandom rng(9);
    bool agree = true;
    for (int trial = 0; trial < 300; trial++) {
        string text, pat;
        int n = 50 + rng.nextInt(200), m = 1 + rng.nextInt(6);
        for (int i = 0; i < n; i++) text += (char)('a' + rng.nextInt(2));
        for (int i = 0; i < m; i++) pat += (char)('a' + rng.nextInt(2));
        int p1[300], p2[300], p3[300];
        MatchStats a, b, c;
        int c1 = StringMatch::naive(text, pat, p1, 300, a);
        int c2 = StringMatch::kmp(text, pat, p2, 300, b);
        int c3 = StringMatch::rabinKarp(text, pat, p3, 300, c);
        if (c1 != c2 || c2 != c3) agree = false;
        for (int i = 0; i < c1 && agree; i++) if (p1[i] != p2[i] || p2[i] != p3[i]) agree = false;
    }
    check(agree, "Naive, KMP and Rabin-Karp agree on 300 random texts (counts and positions)");

    // Worst case for naive: text aaaa...a, pattern aaa...ab
    string text(5000, 'a'), pat(50, 'a');
    pat[49] = 'b';
    MatchStats sn, sk, sr;
    StringMatch::naive(text, pat, NULL, 0, sn);
    StringMatch::kmp(text, pat, NULL, 0, sk);
    StringMatch::rabinKarp(text, pat, NULL, 0, sr);
    cout << "  worst case (n=5000, m=50): naive " << sn.comparisons << ", KMP " << sk.comparisons
         << ", Rabin-Karp " << sr.comparisons << " comparisons\n";
    check(sk.comparisons <= 2 * 5000, "KMP stays linear (<= 2n comparisons)");
    check(sn.comparisons > 40 * 5000, "naive is ~n*m on this input");

    MatchStats tiny;
    int found = StringMatch::rabinKarp("abcabcabcxyzabc", "abc", NULL, 0, tiny, 7);   // tiny modulus
    cout << "  Rabin-Karp with modulus 7: " << found << " matches, " << tiny.spuriousHits << " spurious hash hits\n";
    check(found == 4 && tiny.spuriousHits > 0, "tiny modulus causes spurious hits but answer stays correct");

    MatchStats ci;
    check(StringMatch::search(KMP_MATCH, "ACCIDENT on Saddar", "accident", NULL, 0, ci) == 1,
          "case-insensitive search");
}

void testUnionFind() {
    cout << "\n== Union-Find ==\n";
    UnionFind uf(10);
    check(uf.componentCount() == 10, "starts with 10 groups");
    uf.unite(0, 1); uf.unite(2, 3); uf.unite(1, 3);
    check(uf.connected(0, 2) && !uf.connected(0, 4), "0-1-3-2 connected, 4 separate");
    check(!uf.unite(0, 2), "uniting same group returns false (cycle)");
    check(uf.componentCount() == 7, "7 groups left");
}

// MST check (cycle property): for every road NOT in the tree, every tree
// edge on the path between its ends must be no longer than it.
bool maxOnTreePath(int V, MSTEdge* t, int k, int a, int b, int cur, int prev, int best, int& out) {
    if (cur == b) { out = best; return true; }
    for (int i = 0; i < k; i++) {
        int nxt = -1;
        if (t[i].u == cur) nxt = t[i].v; else if (t[i].v == cur) nxt = t[i].u;
        if (nxt == -1 || nxt == prev) continue;
        if (maxOnTreePath(V, t, k, a, b, nxt, cur, best > t[i].weight ? best : t[i].weight, out)) return true;
    }
    return false;
}

void testMST(Graph& city) {
    cout << "\n== Minimum spanning tree ==\n";
    int V = city.getNodeCount();
    MSTEdge* p = new MSTEdge[V];
    MSTEdge* k = new MSTEdge[V];
    MSTResult rp = MST::prim(city, p);
    MSTResult rk = MST::kruskal(city, k);
    int network = MST::networkLength(city);
    cout << "  Prim: " << rp.totalWeight << " m (" << rp.work << " matrix cells), Kruskal: "
         << rk.totalWeight << " m (" << rk.work << " edges examined)\n";
    cout << "  MST keeps " << rk.totalWeight / 1000.0 << " km of " << network / 1000.0
         << " km network (" << (100 * rk.totalWeight / network) << "%):\n";
    for (int i = 0; i < rk.edgeCount; i++)
        cout << "    " << city.getIntersection(k[i].u).name << " - " << city.getIntersection(k[i].v).name
             << "  " << k[i].weight << " m\n";
    check(rp.totalWeight == rk.totalWeight, "Prim and Kruskal give the same total");
    check(rp.edgeCount == V - 1 && rk.edgeCount == V - 1 && rk.components == 1, "V-1 = 12 edges, one component");

    bool optimal = true;
    for (int r = 0; r < city.getRoadCount(); r++) {
        const Road* road = city.getRoad(r);
        int mx = 0;
        if (maxOnTreePath(V, k, rk.edgeCount, road->from, road->to, road->from, -1, 0, mx) && mx > road->length)
            optimal = false;
    }
    check(optimal, "cycle property holds for every road (tree is truly minimum)");

    // Close both roads into Tower -> spanning forest with 2 components
    int tower = city.findIntersection("Tower");
    for (int r = 0; r < city.getRoadCount(); r++) {
        Road* road = city.getRoad(r);
        if (road->from == tower || road->to == tower) road->blocked = true;
    }
    rp = MST::prim(city, p);
    rk = MST::kruskal(city, k);
    check(rp.components == 2 && rk.components == 2 && rp.totalWeight == rk.totalWeight,
          "Tower cut off: both find a 2-component spanning forest");
    for (int r = 0; r < city.getRoadCount(); r++) city.getRoad(r)->blocked = false;
    delete[] p;
    delete[] k;
}

void testTopoSort(Graph& city) {
    cout << "\n== Topological sort ==\n";
    DirectedGraph dag(6);                    // textbook example
    dag.addEdge(5, 2); dag.addEdge(5, 0); dag.addEdge(4, 0);
    dag.addEdge(4, 1); dag.addEdge(2, 3); dag.addEdge(3, 1);
    int o1[6], o2[6];
    check(TopoSort::kahn(dag, o1) && TopoSort::isValidOrder(dag, o1), "Kahn: valid order on 6-node DAG");
    check(TopoSort::dfs(dag, o2) && TopoSort::isValidOrder(dag, o2), "DFS: valid order on 6-node DAG");
    dag.addEdge(1, 5);                       // creates cycle 5->2->3->1->5
    check(!TopoSort::kahn(dag, o1) && !TopoSort::dfs(dag, o2), "both detect the cycle");

    DirectedGraph cityGraph;
    TopoSort::fromCity(city, cityGraph);
    int* order = new int[city.getNodeCount()];
    check(!TopoSort::kahn(cityGraph, order) && !TopoSort::dfs(cityGraph, order),
          "city road network has cycles (two-way roads) -> no topological order");

    int saddar = city.findIntersection("Saddar");
    int* dist = new int[city.getNodeCount()];
    DirectedGraph evac;
    TopoSort::evacuationDag(city, saddar, false, evac, dist);
    bool ok = TopoSort::kahn(evac, order) && TopoSort::isValidOrder(evac, order);
    cout << "  Evacuation order from Saddar (" << evac.edgeCount() << " outward roads):\n    ";
    for (int i = 0; i < city.getNodeCount(); i++)
        cout << city.getIntersection(order[i]).name << "(" << dist[order[i]] << "s) ";
    cout << "\n";
    check(ok && order[0] == saddar, "evacuation DAG sorts, Saddar first");
    bool nondecreasing = true;
    for (int i = 1; i < city.getNodeCount(); i++) if (dist[order[i]] < dist[order[i - 1]]) nondecreasing = false;
    int o3[13];
    check(TopoSort::dfs(evac, o3) && TopoSort::isValidOrder(evac, o3), "DFS version also valid");
    cout << "  (Kahn order " << (nondecreasing ? "also" : "is not") << " sorted by distance)\n";
    delete[] order;
    delete[] dist;
}

void testPanel(Graph& city) {
    cout << "\n== DSA panel on live simulation data ==\n";
    VehicleManager vm(city, 77);
    SignalController sc(city, ADAPTIVE);
    ChaosManager cm(city, vm, 13);
    DSAPanel panel(city, vm, cm);
    cm.setChaosMode(true);
    for (int t = 0; t < 2400; t++) {
        if (t % 10 == 0) vm.spawnMany(1);
        vm.update(SIM_TICK); sc.update(SIM_TICK, vm); cm.update(SIM_TICK);
    }
    cout << "  simulated 40 min: " << vm.getTrips().size() << " trips, " << cm.eventCount() << " log entries\n";

    SortBenchResult res[SORT_ALGORITHM_COUNT];
    panel.benchmarkSorts(DATA_TRIP_TIMES, 3000, res);
    cout << "  Sorting 3000 real trip times:\n";
    bool allOk = true;
    for (int a = 0; a < SORT_ALGORITHM_COUNT; a++) {
        cout << "    ";
        cout.width(15); cout << left << sortName(res[a].algo) << right;
        cout.width(10); cout << res[a].comparisons << " cmp ";
        cout.width(10); cout << res[a].moves << " moves ";
        cout.width(10); cout << (long)res[a].microseconds << " us\n";
        if (!res[a].correct) allOk = false;
    }
    check(allOk, "all six correct on real data");
    check(res[QUICK_SORT].comparisons < res[BUBBLE_SORT].comparisons / 20, "quick sort uses far fewer comparisons than bubble");

    SearchBenchResult sr = panel.benchmarkSearch(5000, 2000);
    cout << "  Search in 5000 ids: linear " << sr.linearAvgComparisons << " cmp / " << sr.linearMicro
         << " us, binary " << sr.binaryAvgComparisons << " cmp / " << sr.binaryMicro
         << " us, hash " << sr.hashMicro << " us\n";
    check(sr.agree, "linear, binary and hash give the same answers");
    check(sr.binaryAvgComparisons < 30 && sr.linearAvgComparisons > 2000, "binary ~log n, linear ~n");

    RoutingCompareResult rc = panel.compareRouting(city.findIntersection("Tower"), city.findIntersection("Malir"));
    cout << "  Tower -> Malir: BFS " << rc.bfsRoads << " roads/" << rc.bfsTime << "s, DFS " << rc.dfsRoads
         << " roads/" << rc.dfsTime << "s, Dijkstra " << rc.dijkstraRoads << " roads/" << rc.dijkstraTime << "s\n";
    cout << "  All pairs: 13 x Dijkstra " << rc.dijkstraAllMicro << " us, Floyd-Warshall " << rc.floydMicro << " us\n";
    check(rc.dijkstraTime <= rc.bfsTime && rc.dijkstraTime <= rc.dfsTime, "Dijkstra route is never slower");

    // Log search: all three algorithms find the same entries
    MatchStats ms[MATCH_ALGORITHM_COUNT];
    int hits = panel.compareMatchers("accident", ms);
    int expected = 0;
    for (ListNode<ChaosEvent>* n = cm.eventLogHead(); n; n = n->next) {
        string low = StringMatch::toLower(n->data.text);
        if (low.find("accident") != string::npos) expected++;           // std::string check as reference
    }
    cout << "  log search 'accident': " << hits << " entries; comparisons naive " << ms[0].comparisons
         << ", KMP " << ms[1].comparisons << ", Rabin-Karp " << ms[2].comparisons << "\n";
    check(hits == expected && ms[0].matches == ms[1].matches && ms[1].matches == ms[2].matches,
          "Naive, KMP, Rabin-Karp find the same log entries");

    LinkedList<int> places;
    panel.findPlaces("KAR", KMP_MATCH, places);
    check(places.size() == 2, "places containing 'KAR': Karsaz, North_Karachi");
    panel.findPlaces("ab", RABIN_KARP_MATCH, places);
    check(places.size() == 2, "places containing 'ab': Nazimabad, Sohrab_Goth");
}

int main() {
    cout << "===== CHAOS CITY : STEP 8 DSA PANEL TESTS =====\n";
    Graph city;
    string err;
    if (!city.loadFromFile("city_map.txt", err)) {
        cout << "Cannot load map: " << err << "\n";
        cin.get();
        return 1;
    }
    testStringMatching();
    testUnionFind();
    testMST(city);
    testTopoSort(city);
    testPanel(city);
    cout << "\nResult: " << passed << " passed, " << failed << " failed\n";
    cout << "\nPress Enter to exit...";
    cin.get();
    return 0;
}
