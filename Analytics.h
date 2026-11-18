// =====================================================================
//  Chaos City - Analytics.h
//  Everything the dashboard shows.
//
//   Trip statistics   - every finished trip goes into an AVL TripTree:
//                       min / max / median / 90th percentile / histogram
//                       in O(log n) each.
//   Rankings          - most congested roads and busiest intersections,
//                       sorted with any of the six sorting algorithms.
//   Trip lookup       - trips copied to an array, sorted by vehicle id
//                       (quick sort) and found with BINARY search.
//   History           - a snapshot every SAMPLE_INTERVAL seconds kept in a
//                       fixed-size circular ARRAY (oldest overwritten), used
//                       for the live line charts in the GUI.
// =====================================================================
#ifndef ANALYTICS_H
#define ANALYTICS_H

#include "Graph.h"
#include "VehicleManager.h"
#include "TripTree.h"
#include "Sorting.h"
#include "Searching.h"

const double SAMPLE_INTERVAL = 30.0;   // seconds between history snapshots
const int    HISTORY_SIZE    = 240;    // 240 x 30 s = last 2 hours

struct Snapshot {
    double time;
    int active;            // vehicles in the city
    int queued;            // waiting at intersections
    int entering;          // waiting at their origin
    double avgSpeedKmh;    // average speed of moving vehicles
    double avgCongestion;  // average vehicles/capacity over used roads
    int arrived;           // total arrivals so far
    Snapshot() : time(0), active(0), queued(0), entering(0), avgSpeedKmh(0),
                 avgCongestion(0), arrived(0) {}
};

// ---------- comparison functors for sorting ----------
struct TripByTime {
    bool operator()(const TripRecord& a, const TripRecord& b) const {
        if (a.travelTime != b.travelTime) return a.travelTime < b.travelTime;
        return a.vehicleId < b.vehicleId;
    }
};

struct TripById {
    bool operator()(const TripRecord& a, const TripRecord& b) const { return a.vehicleId < b.vehicleId; }
};

// Road ids ordered by congestion, MOST congested first
struct RoadByCongestionDesc {
    const Graph* g;
    RoadByCongestionDesc(const Graph* graph) : g(graph) {}
    bool operator()(int a, int b) const {
        double ca = g->getRoad(a)->congestionRatio(), cb = g->getRoad(b)->congestionRatio();
        if (ca != cb) return ca > cb;
        return a < b;
    }
};

// Index/value pairs ordered by value, BIGGEST first
struct ScoredItem {
    int id;
    int score;
    ScoredItem() : id(-1), score(0) {}
    ScoredItem(int i, int s) : id(i), score(s) {}
};
struct ScoreDesc {
    bool operator()(const ScoredItem& a, const ScoredItem& b) const {
        if (a.score != b.score) return a.score > b.score;
        return a.id < b.id;
    }
};

class Analytics {
private:
    Graph& graph;
    VehicleManager& vm;
    TripTree tree;

    ListNode<TripRecord>* lastSeen;   // last trip already added to the tree
    double totalWait, totalDistance, totalTravel;
    int typeCount[4];
    double typeTime[4];
    int totalReroutes;

    Snapshot* history;                // circular array
    int histStart, histCount;
    double sampleTimer;

    Analytics(const Analytics&);
    Analytics& operator=(const Analytics&);

    void addTrip(const TripRecord& t) {
        tree.insert(t);
        totalWait += t.waitTime;
        totalDistance += t.distance;
        totalTravel += t.travelTime;
        typeCount[t.type]++;
        typeTime[t.type] += t.travelTime;
        totalReroutes += t.reroutes;
    }

    // Pull trips that finished since the last call (the trip list only grows)
    void ingestNewTrips() {
        ListNode<TripRecord>* n = (lastSeen == NULL) ? vm.getTrips().getHead() : lastSeen->next;
        while (n != NULL) {
            addTrip(n->data);
            lastSeen = n;
            n = n->next;
        }
    }

    void takeSnapshot() {
        Snapshot s;
        s.time = vm.getSimTime();
        s.active = vm.activeCount();
        s.queued = vm.totalQueued();
        s.entering = vm.totalEntering();
        s.avgSpeedKmh = averageSpeedKmh();
        s.avgCongestion = averageCongestion();
        s.arrived = vm.arrivedCount();
        int idx = (histStart + histCount) % HISTORY_SIZE;
        history[idx] = s;
        if (histCount < HISTORY_SIZE) histCount++;
        else histStart = (histStart + 1) % HISTORY_SIZE;   // overwrite oldest
    }

public:
    Analytics(Graph& g, VehicleManager& v)
        : graph(g), vm(v), tree(true), lastSeen(NULL), totalWait(0), totalDistance(0),
          totalTravel(0), totalReroutes(0), histStart(0), histCount(0), sampleTimer(0) {
        for (int i = 0; i < 4; i++) { typeCount[i] = 0; typeTime[i] = 0; }
        history = new Snapshot[HISTORY_SIZE];
    }

    ~Analytics() { delete[] history; }

    // Once per tick (after the other managers)
    void update(double dt) {
        ingestNewTrips();
        sampleTimer += dt;
        if (sampleTimer >= SAMPLE_INTERVAL) {
            sampleTimer = 0;
            takeSnapshot();
        }
    }

    // ------------------------------------------------------------
    //  Trip statistics (AVL tree)
    // ------------------------------------------------------------
    int tripCount() const { return tree.size(); }
    double avgTravelTime() const { return tree.size() ? totalTravel / tree.size() : 0; }
    double avgWaitTime() const { return tree.size() ? totalWait / tree.size() : 0; }
    double avgTripKm() const { return tree.size() ? totalDistance / tree.size() / 1000.0 : 0; }
    int totalTripReroutes() const { return totalReroutes; }

    double avgTimeByType(VehicleType t) const {
        return typeCount[t] ? typeTime[t] / typeCount[t] : 0;
    }
    int countByType(VehicleType t) const { return typeCount[t]; }

    double fastestTrip() const { TripRecord t; return tree.minTrip(t) ? t.travelTime : 0; }
    double slowestTrip() const { TripRecord t; return tree.maxTrip(t) ? t.travelTime : 0; }

    // p in [0,100]; nearest-rank percentile
    double percentile(double p) const {
        int n = tree.size();
        if (n == 0) return 0;
        int k = (int)(p / 100.0 * n);
        if (k >= n) k = n - 1;
        if (k < 0) k = 0;
        TripRecord t;
        tree.kth(k, t);
        return t.travelTime;
    }
    double medianTrip() const { return percentile(50); }

    int tripsFasterThan(double seconds) const { return tree.countLess(seconds); }

    // Histogram: buckets of 'width' seconds starting at 0; last bucket = "and above"
    void histogram(int* buckets, int bucketCount, double width) const {
        for (int b = 0; b < bucketCount; b++) {
            double lo = b * width;
            double hi = (b == bucketCount - 1) ? 1e18 : (b + 1) * width;
            buckets[b] = tree.countBetween(lo, hi);
        }
    }

    // k slowest trips (slowest first) using kth from the end
    int slowestTrips(TripRecord* out, int k) const {
        int n = tree.size(), c = 0;
        for (int i = n - 1; i >= 0 && c < k; i--, c++) tree.kth(i, out[c]);
        return c;
    }

    const TripTree& getTree() const { return tree; }

    // ------------------------------------------------------------
    //  Find a finished trip by vehicle id: sort by id + binary search
    //  (The hash table only holds ACTIVE vehicles; finished ones live here.)
    // ------------------------------------------------------------
    bool findTrip(int vehicleId, TripRecord& out, int& comparisons) const {
        comparisons = 0;
        int n = tree.size();
        if (n == 0) return false;
        TripRecord* arr = new TripRecord[n];
        tree.toSortedArray(arr);
        SortStats s;
        Sorting::quickSort(arr, n, TripById(), s);
        TripRecord key;
        key.vehicleId = vehicleId;
        int idx = Searching::binarySearch(arr, n, key, TripById(), comparisons);
        if (idx != -1) out = arr[idx];
        delete[] arr;
        return idx != -1;
    }

    // ------------------------------------------------------------
    //  Live network state
    // ------------------------------------------------------------
    double averageSpeedKmh() const {
        double sum = 0;
        int n = 0;
        for (ListNode<Vehicle*>* p = vm.activeHead(); p != NULL; p = p->next) {
            const Vehicle* v = p->data;
            if (v->state != MOVING) continue;
            const Road* r = graph.getRoad(v->currentRoad);
            double kmh = 0;
            if (!r->incident) {
                int lim = v->maxSpeed() < r->speedLimit ? v->maxSpeed() : r->speedLimit;
                kmh = lim / r->slowdown();
            }
            sum += kmh;
            n++;
        }
        return n ? sum / n : 0;
    }

    // Average fullness of roads that currently carry vehicles
    double averageCongestion() const {
        double sum = 0;
        int n = 0;
        for (int r = 0; r < graph.getRoadCount(); r++) {
            const Road* road = graph.getRoad(r);
            if (road->vehiclesOnRoad == 0) continue;
            sum += road->congestionRatio();
            n++;
        }
        return n ? sum / n : 0;
    }

    // Top-k most congested road ids, using the chosen sorting algorithm
    int topCongestedRoads(int* out, int k, SortAlgorithm algo, SortStats& stats) const {
        int n = graph.getRoadCount();
        int* ids = new int[n];
        for (int i = 0; i < n; i++) ids[i] = i;
        Sorting::sort(algo, ids, n, RoadByCongestionDesc(&graph), stats);
        int c = 0;
        for (int i = 0; i < n && c < k; i++) out[c++] = ids[i];
        delete[] ids;
        return c;
    }

    // Top-k intersections by vehicles waiting on all incoming roads
    int busiestIntersections(ScoredItem* out, int k, SortAlgorithm algo, SortStats& stats) const {
        int V = graph.getNodeCount();
        ScoredItem* items = new ScoredItem[V];
        for (int v = 0; v < V; v++) items[v] = ScoredItem(v, 0);
        for (int r = 0; r < graph.getRoadCount(); r++)
            items[graph.getRoad(r)->to].score += vm.queueLength(r);
        Sorting::sort(algo, items, V, ScoreDesc(), stats);
        int c = 0;
        for (int i = 0; i < V && c < k; i++) out[c++] = items[i];
        delete[] items;
        return c;
    }

    // ------------------------------------------------------------
    //  History (circular array) - i = 0 is the OLDEST snapshot
    // ------------------------------------------------------------
    int historyCount() const { return histCount; }
    const Snapshot& historyAt(int i) const { return history[(histStart + i) % HISTORY_SIZE]; }
    void forceSnapshot() { takeSnapshot(); }
};

#endif
