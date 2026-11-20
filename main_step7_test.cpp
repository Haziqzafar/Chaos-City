// =====================================================================
//  Chaos City - Step 7 test driver: AVL tree, sorting, searching, analytics
//  Keep city_map.txt in the same folder.
// =====================================================================
#include <iostream>
#include <string>
#include <cmath>
#include "Graph.h"
#include "VehicleManager.h"
#include "SignalController.h"
#include "ChaosManager.h"
#include "TripTree.h"
#include "Sorting.h"
#include "Searching.h"
#include "Analytics.h"

using namespace std;

int passed = 0, failed = 0;

void check(bool condition, const string& testName) {
    if (condition) { passed++; cout << "  [PASS] " << testName << "\n"; }
    else           { failed++; cout << "  [FAIL] " << testName << "\n"; }
}

TripRecord makeTrip(int id, double time) {
    TripRecord t;
    t.vehicleId = id;
    t.travelTime = time;
    return t;
}

void testAVL() {
    cout << "\n== AVL tree vs plain BST ==\n";
    TripTree avl(true), bst(false);
    for (int i = 0; i < 1000; i++) {               // worst case: increasing order
        avl.insert(makeTrip(i, 100 + i));
        bst.insert(makeTrip(i, 100 + i));
    }
    cout << "  1000 trips inserted in increasing time order:\n";
    cout << "    plain BST height = " << bst.height() << "  (it became a linked list)\n";
    cout << "    AVL height       = " << avl.height() << "  (" << avl.getRotations() << " rotations)\n";
    check(bst.height() == 1000, "plain BST degenerates to height n");
    check(avl.height() <= 15, "AVL height <= 1.44 log2(n) ~ 14.4");
    check(avl.isValid() && bst.isValid(), "both trees valid (order, sizes, heights, balance)");

    for (int i = 0; i < 1000; i += 2) avl.remove(100 + i, i);   // remove every even trip
    check(avl.size() == 500 && avl.isValid(), "remove 500 trips -> still a valid AVL");
    check(!avl.remove(12345, 1), "removing a missing trip returns false");

    TripRecord t;
    avl.minTrip(t);
    check(t.vehicleId == 1, "min is fastest remaining trip");
    avl.kth(249, t);
    check(t.vehicleId == 499, "kth(249) = 250th fastest");
    check(avl.countLess(200) == 50, "countLess(200 s) = 50");
    check(avl.countBetween(300, 400) == 50, "countBetween(300, 400) = 50");
    LinkedList<TripRecord> r;
    avl.range(300, 320, r);
    check(r.size() == 10, "range [300, 320] collects 10 trips");

    // random data vs brute force
    TripTree rnd(true);
    SimpleRandom rng(42);
    double times[2000];
    for (int i = 0; i < 2000; i++) { times[i] = rng.nextInt(5000); rnd.insert(makeTrip(i, times[i])); }
    int brute = 0;
    for (int i = 0; i < 2000; i++) if (times[i] >= 1000 && times[i] < 2500) brute++;
    check(rnd.countBetween(1000, 2500) == brute, "countBetween matches brute force (with duplicate times)");
    check(rnd.isValid() && rnd.height() <= 16, "random 2000: valid AVL, height <= 16");
}

struct Pair { int key; int order; };
struct PairLess { bool operator()(const Pair& a, const Pair& b) const { return a.key < b.key; } };

void testSorting() {
    cout << "\n== Six sorting algorithms ==\n";
    const int N = 2000;
    int base[N];
    SimpleRandom rng(7);
    const char* kinds[4] = { "random", "sorted", "reversed", "few distinct" };
    bool allCorrect = true;
    long long cmp[6][4];
    for (int kind = 0; kind < 4; kind++) {
        for (int i = 0; i < N; i++) {
            if (kind == 0) base[i] = rng.nextInt(100000);
            if (kind == 1) base[i] = i;
            if (kind == 2) base[i] = N - i;
            if (kind == 3) base[i] = rng.nextInt(5);
        }
        for (int a = 0; a < SORT_ALGORITHM_COUNT; a++) {
            int arr[N];
            for (int i = 0; i < N; i++) arr[i] = base[i];
            SortStats s;
            Sorting::sort((SortAlgorithm)a, arr, N, DefaultLess(), s);
            if (!Sorting::isSorted(arr, N, DefaultLess())) allCorrect = false;
            cmp[a][kind] = s.comparisons;
        }
    }
    check(allCorrect, "all 6 algorithms sort random, sorted, reversed and duplicate data");

    cout << "  Comparisons for n = " << N << ":\n                    ";
    for (int k = 0; k < 4; k++) { cout.width(13); cout << kinds[k]; }
    cout << "\n";
    for (int a = 0; a < SORT_ALGORITHM_COUNT; a++) {
        cout << "    ";
        cout.width(16); cout << left << sortName((SortAlgorithm)a) << right;
        for (int k = 0; k < 4; k++) { cout.width(13); cout << cmp[a][k]; }
        cout << "\n";
    }
    check(cmp[BUBBLE_SORT][1] == N - 1, "bubble sort on sorted data: n-1 comparisons (early exit)");
    check(cmp[INSERTION_SORT][1] == N - 1, "insertion sort on sorted data: n-1 comparisons");
    check(cmp[QUICK_SORT][1] < 3 * N * 11, "quick sort on sorted data stays O(n log n) (median of 3)");
    check(cmp[SELECTION_SORT][0] == (long long)N * (N - 1) / 2, "selection sort always n(n-1)/2");
    check(cmp[QUICK_SORT][3] < 3 * N * 11, "quick sort with many duplicates stays O(n log n) (Hoare partition)");

    // Stability: equal keys keep their original order
    Pair p[500];
    for (int i = 0; i < 500; i++) { p[i].key = rng.nextInt(10); p[i].order = i; }
    SortAlgorithm stable[3] = { BUBBLE_SORT, INSERTION_SORT, MERGE_SORT };
    bool stableOk = true;
    for (int a = 0; a < 3; a++) {
        Pair q[500];
        for (int i = 0; i < 500; i++) q[i] = p[i];
        SortStats s;
        Sorting::sort(stable[a], q, 500, PairLess(), s);
        for (int i = 1; i < 500; i++)
            if (q[i].key == q[i - 1].key && q[i].order < q[i - 1].order) stableOk = false;
    }
    check(stableOk, "bubble, insertion and merge sort are stable");
}

void testSearching() {
    cout << "\n== Linear vs binary search ==\n";
    const int N = 4096;
    int a[N];
    for (int i = 0; i < N; i++) a[i] = i * 3;             // sorted, multiples of 3
    bool agree = true;
    int maxBinary = 0;
    long long totalLinear = 0;
    for (int key = -3; key < N * 3 + 3; key++) {
        int c1 = 0, c2 = 0, c3 = 0;
        int i1 = Searching::linearSearch(a, N, key, DefaultLess(), c1);
        int i2 = Searching::binarySearch(a, N, key, DefaultLess(), c2);
        int i3 = Searching::binarySearchRecursive(a, N, key, DefaultLess(), c3);
        if (i1 != i2 || i2 != i3) agree = false;
        if (c2 > maxBinary) maxBinary = c2;
        totalLinear += c1;
    }
    cout << "  n = " << N << ": binary search worst case " << maxBinary
         << " comparisons, linear search average " << totalLinear / (N * 3 + 6) << "\n";
    check(agree, "linear, binary and recursive binary give identical answers");
    check(maxBinary <= 2 * 13, "binary search <= 2(log2 n + 1) comparisons");
    int c = 0;
    check(Searching::lowerBound(a, N, 100, DefaultLess(), c) == 34, "lowerBound(100) -> first value 102 at index 34");
}

void testAnalytics(Graph& city) {
    cout << "\n== Analytics on a 1-hour simulation with Chaos Mode ==\n";
    VehicleManager vm(city, 31);
    SignalController sc(city, ADAPTIVE);
    ChaosManager cm(city, vm, 5);
    Analytics an(city, vm);
    cm.setChaosMode(true);
    int ranked = 0;
    bool rankingSorted = true;
    for (int t = 0; t < 3600; t++) {
        if (t % 8 == 0) vm.spawnMany(1);
        vm.update(SIM_TICK);
        sc.update(SIM_TICK, vm);
        cm.update(SIM_TICK);
        an.update(SIM_TICK);
        if (t == 1800) {                          // check a live ranking mid-run
            int top[5];
            SortStats s;
            ranked = an.topCongestedRoads(top, 5, MERGE_SORT, s);
            cout << "  Top congested roads at t = 30 min (merge sort, " << s.comparisons << " comparisons):\n";
            for (int i = 0; i < ranked; i++) {
                const Road* r = city.getRoad(top[i]);
                cout << "    " << i + 1 << ". " << city.getIntersection(r->from).name << " -> "
                     << city.getIntersection(r->to).name << "  " << (int)(r->congestionRatio() * 100) << "% full\n";
                if (i > 0 && city.getRoad(top[i - 1])->congestionRatio() < r->congestionRatio()) rankingSorted = false;
            }
            ScoredItem busy[3];
            an.busiestIntersections(busy, 3, HEAP_SORT, s);
            cout << "  Busiest intersections: ";
            for (int i = 0; i < 3; i++) cout << city.getIntersection(busy[i].id).name << " (" << busy[i].score << ") ";
            cout << "\n";
        }
    }

    cout << "  trips " << an.tripCount() << ", avg " << (int)an.avgTravelTime() << "s, median "
         << (int)an.medianTrip() << "s, 90th pct " << (int)an.percentile(90) << "s, fastest "
         << (int)an.fastestTrip() << "s, slowest " << (int)an.slowestTrip() << "s\n";
    cout << "  avg wait " << (int)an.avgWaitTime() << "s, avg trip " << an.avgTripKm()
         << " km, avg speed now " << (int)an.averageSpeedKmh() << " km/h, tree height "
         << an.getTree().height() << "\n";
    cout << "  by type: Car " << (int)an.avgTimeByType(CAR) << "s, Bus " << (int)an.avgTimeByType(BUS)
         << "s, Truck " << (int)an.avgTimeByType(TRUCK) << "s, Ambulance " << (int)an.avgTimeByType(AMBULANCE) << "s\n";

    check(an.tripCount() == vm.arrivedCount(), "every arrival is in the tree");
    check(fabs(an.avgTravelTime() - vm.averageTravelTime()) < 0.001, "average matches VehicleManager");
    check(an.getTree().isValid(), "trip tree is a valid AVL");
    check(ranked == 5 && rankingSorted, "top-5 congested roads in descending order");

    // median & percentile vs a sorted array
    int n = an.tripCount();
    TripRecord* arr = new TripRecord[n];
    an.getTree().toSortedArray(arr);
    check(Sorting::isSorted(arr, n, TripByTime()), "in-order traversal is sorted");
    check(an.medianTrip() == arr[n / 2].travelTime, "median from tree == middle of sorted array");

    // histogram adds up
    int buckets[8], sum = 0;
    an.histogram(buckets, 8, 300);
    cout << "  histogram (5-min buckets): ";
    for (int b = 0; b < 8; b++) { cout << buckets[b] << " "; sum += buckets[b]; }
    cout << "\n";
    check(sum == n, "histogram buckets add up to all trips");

    // find by id with binary search
    int comps = 0;
    TripRecord found;
    int target = arr[n / 3].vehicleId;
    bool ok = an.findTrip(target, found, comps);
    cout << "  findTrip(" << target << "): " << comps << " comparisons in " << n << " trips\n";
    check(ok && found.vehicleId == target && found.travelTime == arr[n / 3].travelTime, "binary search finds trip by id");
    check(!an.findTrip(1, found, comps), "unknown id not found");
    delete[] arr;

    check(an.historyCount() == 120, "120 snapshots (one per 30 s)");
    check(an.historyAt(0).time < an.historyAt(119).time, "history oldest -> newest");
    for (int i = 0; i < 300; i++) an.forceSnapshot();
    check(an.historyCount() == HISTORY_SIZE, "circular history caps at 240 entries");
}

int main() {
    cout << "===== CHAOS CITY : STEP 7 ANALYTICS TESTS =====\n";
    Graph city;
    string err;
    if (!city.loadFromFile("city_map.txt", err)) {
        cout << "Cannot load map: " << err << "\n";
        cin.get();
        return 1;
    }
    testAVL();
    testSorting();
    testSearching();
    testAnalytics(city);
    cout << "\nResult: " << passed << " passed, " << failed << " failed\n";
    cout << "\nPress Enter to exit...";
    cin.get();
    return 0;
}

