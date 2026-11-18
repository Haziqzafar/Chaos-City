// =====================================================================
//  Chaos City - DSAPanel.h
//  Back-end of the "DSA Panel" screen: runs algorithms side by side on
//  REAL simulation data and reports operation counts and timings.
//
//   benchmarkSorts   - six sorting algorithms on trip times / random /
//                      sorted / reversed data (comparisons, moves, time)
//   benchmarkSearch  - linear vs binary search vs hash table lookup
//   compareRouting   - BFS vs DFS vs Dijkstra on the same trip,
//                      Dijkstra (all) vs Floyd-Warshall timing
//   searchEvents     - find Chaos log entries with Naive / KMP / Rabin-Karp
//   compareMatchers  - same search with all three, counting comparisons
//   findPlaces       - intersections whose name contains a pattern
//   (MST and topological sort are called directly from MST.h / TopoSort.h)
//
//  Timing uses clock() and repeats each test until at least 50 ms have
//  passed, because Windows clock() only has ~1 ms resolution.
// =====================================================================
#ifndef DSAPANEL_H
#define DSAPANEL_H

#include <ctime>
#include <string>
#include "Graph.h"
#include "GraphAlgorithms.h"
#include "VehicleManager.h"
#include "ChaosManager.h"
#include "Sorting.h"
#include "Searching.h"
#include "HashTable.h"
#include "StringMatch.h"

enum BenchData { DATA_TRIP_TIMES = 0, DATA_RANDOM = 1, DATA_SORTED = 2, DATA_REVERSED = 3 };

inline const char* benchDataName(BenchData d) {
    switch (d) {
        case DATA_TRIP_TIMES: return "Real trip times";
        case DATA_RANDOM:     return "Random";
        case DATA_SORTED:     return "Already sorted";
        default:              return "Reversed";
    }
}

struct SortBenchResult {
    SortAlgorithm algo;
    int n;
    long long comparisons, moves;
    double microseconds;       // per sort
    bool correct;
    SortBenchResult() : algo(BUBBLE_SORT), n(0), comparisons(0), moves(0), microseconds(0), correct(false) {}
};

struct SearchBenchResult {
    int n, lookups;
    double linearAvgComparisons, binaryAvgComparisons;
    double linearMicro, binaryMicro, hashMicro;    // per lookup
    bool agree;
    SearchBenchResult() : n(0), lookups(0), linearAvgComparisons(0), binaryAvgComparisons(0),
        linearMicro(0), binaryMicro(0), hashMicro(0), agree(false) {}
};

struct RoutingCompareResult {
    int bfsRoads, dfsRoads, dijkstraRoads;
    int bfsTime, dfsTime, dijkstraTime;           // travel seconds of each route
    SearchStats bfs, dfs, dijkstra;
    double dijkstraAllMicro, floydMicro;          // all-pairs: V x Dijkstra vs Floyd
    RoutingCompareResult() : bfsRoads(0), dfsRoads(0), dijkstraRoads(0), bfsTime(0),
        dfsTime(0), dijkstraTime(0), dijkstraAllMicro(0), floydMicro(0) {}
};

const long MIN_BENCH_CLOCKS = CLOCKS_PER_SEC / 20;   // 50 ms

class DSAPanel {
private:
    Graph& graph;
    VehicleManager& vm;
    ChaosManager& chaos;
    SimpleRandom rng;

    DSAPanel(const DSAPanel&);
    DSAPanel& operator=(const DSAPanel&);

    static double microsPer(clock_t clocks, long reps) {
        if (reps <= 0) return 0;
        double us = (double)clocks * 1000000.0 / CLOCKS_PER_SEC / reps;
        return us < 0 ? 0 : us;
    }

    // Fill 'a' with n values of the requested kind
    void makeData(BenchData kind, double* a, int n) {
        if (kind == DATA_TRIP_TIMES) {
            // real finished-trip times in arrival order (repeated if too few)
            ListNode<TripRecord>* head = vm.getTrips().getHead();
            ListNode<TripRecord>* p = head;
            for (int i = 0; i < n; i++) {
                if (p == NULL) {
                    if (head == NULL) { a[i] = rng.nextInt(3600); continue; }
                    p = head;
                }
                a[i] = p->data.travelTime + (i / (vm.getTrips().size() + 1)) * 0.001;
                p = p->next;
            }
            return;
        }
        for (int i = 0; i < n; i++) {
            if (kind == DATA_RANDOM)      a[i] = rng.nextInt(100000);
            else if (kind == DATA_SORTED) a[i] = i;
            else                          a[i] = n - i;
        }
    }

public:
    DSAPanel(Graph& g, VehicleManager& v, ChaosManager& c, unsigned int seed = 2024)
        : graph(g), vm(v), chaos(c), rng(seed) {}

    // ------------------------------------------------------------
    //  Sorting benchmark: out must hold SORT_ALGORITHM_COUNT results
    // ------------------------------------------------------------
    void benchmarkSorts(BenchData kind, int n, SortBenchResult* out) {
        double* base = new double[n];
        double* work = new double[n];
        makeData(kind, base, n);
        for (int a = 0; a < SORT_ALGORITHM_COUNT; a++) {
            SortStats stats;
            long reps = 0;
            clock_t start = clock();
            do {
                for (int i = 0; i < n; i++) work[i] = base[i];
                stats.reset();
                Sorting::sort((SortAlgorithm)a, work, n, DefaultLess(), stats);
                reps++;
            } while (clock() - start < MIN_BENCH_CLOCKS && reps < 100000);
            clock_t total = clock() - start;

            clock_t copyStart = clock();                 // subtract copying cost
            for (long r = 0; r < reps; r++)
                for (int i = 0; i < n; i++) work[i] = base[i];
            clock_t copyTime = clock() - copyStart;

            // one clean run for the counters and the correctness check
            for (int i = 0; i < n; i++) work[i] = base[i];
            stats.reset();
            Sorting::sort((SortAlgorithm)a, work, n, DefaultLess(), stats);
            out[a].algo = (SortAlgorithm)a;
            out[a].n = n;
            out[a].comparisons = stats.comparisons;
            out[a].moves = stats.moves;
            out[a].microseconds = microsPer(total - copyTime, reps);
            out[a].correct = Sorting::isSorted(work, n, DefaultLess());
        }
        delete[] base;
        delete[] work;
    }

    // ------------------------------------------------------------
    //  Searching benchmark on n vehicle ids (half the lookups miss)
    // ------------------------------------------------------------
    SearchBenchResult benchmarkSearch(int n, int lookups) {
        SearchBenchResult res;
        res.n = n; res.lookups = lookups;
        int* ids = new int[n];
        for (int i = 0; i < n; i++) ids[i] = 1001 + 2 * i;       // sorted, odd numbers only
        int* keys = new int[lookups];
        for (int i = 0; i < lookups; i++) {
            int k = 1001 + rng.nextInt(2 * n);
            keys[i] = k;                                         // even keys = misses
        }
        HashTable<int, int> table(16);
        for (int i = 0; i < n; i++) table.put(ids[i], i);

        // correctness + comparison counts (one pass)
        long long cl = 0, cb = 0;
        res.agree = true;
        for (int i = 0; i < lookups; i++) {
            int c1 = 0, c2 = 0, h;
            int a = Searching::linearSearch(ids, n, keys[i], DefaultLess(), c1);
            int b = Searching::binarySearch(ids, n, keys[i], DefaultLess(), c2);
            bool inHash = table.get(keys[i], h);
            if (a != b || (a != -1) != inHash || (inHash && h != a)) res.agree = false;
            cl += c1; cb += c2;
        }
        res.linearAvgComparisons = (double)cl / lookups;
        res.binaryAvgComparisons = (double)cb / lookups;

        long sink = 0, reps;
        clock_t start;
        reps = 0; start = clock();
        do { for (int i = 0; i < lookups; i++) { int c = 0; sink += Searching::linearSearch(ids, n, keys[i], DefaultLess(), c); } reps++; }
        while (clock() - start < MIN_BENCH_CLOCKS);
        res.linearMicro = microsPer(clock() - start, reps * lookups);

        reps = 0; start = clock();
        do { for (int i = 0; i < lookups; i++) { int c = 0; sink += Searching::binarySearch(ids, n, keys[i], DefaultLess(), c); } reps++; }
        while (clock() - start < MIN_BENCH_CLOCKS);
        res.binaryMicro = microsPer(clock() - start, reps * lookups);

        reps = 0; start = clock();
        do { for (int i = 0; i < lookups; i++) { int h = 0; table.get(keys[i], h); sink += h; } reps++; }
        while (clock() - start < MIN_BENCH_CLOCKS);
        res.hashMicro = microsPer(clock() - start, reps * lookups);

        if (sink == 42) res.n++;          // keeps the optimiser from deleting the loops
        delete[] ids;
        delete[] keys;
        return res;
    }

    // ------------------------------------------------------------
    //  Routing comparison on one trip (live traffic)
    // ------------------------------------------------------------
    RoutingCompareResult compareRouting(int src, int dst) {
        RoutingCompareResult res;
        LinkedList<int> route;
        res.bfsRoads = GraphAlgorithms::bfsRoute(graph, src, dst, route, res.bfs);
        res.bfsTime = GraphAlgorithms::routeTime(graph, route, true);
        res.dfsRoads = GraphAlgorithms::dfsRoute(graph, src, dst, route, res.dfs);
        res.dfsTime = GraphAlgorithms::routeTime(graph, route, true);
        res.dijkstraTime = GraphAlgorithms::dijkstraRoute(graph, src, dst, true, route, res.dijkstra);
        res.dijkstraRoads = route.size();

        int V = graph.getNodeCount();
        int* dist = new int[V];
        int* parent = new int[V];
        long reps = 0;
        clock_t start = clock();
        do { for (int s = 0; s < V; s++) GraphAlgorithms::dijkstraAll(graph, s, true, dist, parent); reps++; }
        while (clock() - start < MIN_BENCH_CLOCKS);
        res.dijkstraAllMicro = microsPer(clock() - start, reps);

        int** d = GraphAlgorithms::allocMatrix(V);
        reps = 0; start = clock();
        do { GraphAlgorithms::floydWarshall(graph, true, d); reps++; }
        while (clock() - start < MIN_BENCH_CLOCKS);
        res.floydMicro = microsPer(clock() - start, reps);
        GraphAlgorithms::freeMatrix(d, V);
        delete[] dist;
        delete[] parent;
        return res;
    }

    // ------------------------------------------------------------
    //  String matching on the Chaos event log and place names
    // ------------------------------------------------------------
    int searchEvents(const std::string& pattern, MatchAlgorithm algo,
                     LinkedList<ChaosEvent>& out, MatchStats& stats) {
        out.clear();
        stats.reset();
        for (ListNode<ChaosEvent>* n = chaos.eventLogHead(); n != NULL; n = n->next) {
            MatchStats one;
            if (StringMatch::search(algo, n->data.text, pattern, NULL, 0, one) > 0) out.pushBack(n->data);
            stats.add(one);
        }
        return out.size();
    }

    // stats must hold MATCH_ALGORITHM_COUNT entries; returns matching entries
    int compareMatchers(const std::string& pattern, MatchStats* stats) {
        int entries = 0;
        for (int a = 0; a < MATCH_ALGORITHM_COUNT; a++) {
            LinkedList<ChaosEvent> found;
            entries = searchEvents(pattern, (MatchAlgorithm)a, found, stats[a]);
        }
        return entries;
    }

    int findPlaces(const std::string& pattern, MatchAlgorithm algo, LinkedList<int>& out) {
        out.clear();
        for (int v = 0; v < graph.getNodeCount(); v++) {
            MatchStats s;
            if (StringMatch::contains(algo, graph.getIntersection(v).name, pattern, s)) out.pushBack(v);
        }
        return out.size();
    }
};

#endif
