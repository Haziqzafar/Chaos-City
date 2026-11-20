// =====================================================================
//  Chaos City - Step 2 test driver: the city graph
//  Keep city_map.txt in the same folder as this file.
// =====================================================================
#include <iostream>
#include <string>
#include <sstream>
#include "Graph.h"

using namespace std;

int passed = 0, failed = 0;

void check(bool condition, const string& testName) {
    if (condition) { passed++; cout << "  [PASS] " << testName << "\n"; }
    else           { failed++; cout << "  [FAIL] " << testName << "\n"; }
}

void testLoadMap(Graph& city) {
    cout << "\n== Load city_map.txt ==\n";
    string err;
    bool ok = city.loadFromFile("city_map.txt", err);
    if (!ok) cout << "  Error: " << err << "\n";
    check(ok, "map file loads");
    check(city.getNodeCount() == 13, "13 intersections");
    // 19 two-way roads x 2 + 2 one-way roads = 40 directed roads
    check(city.getRoadCount() == 40, "40 directed roads (19 two-way, 2 one-way)");
}

void testLookups(Graph& city) {
    cout << "\n== Lookups ==\n";
    int saddar = city.findIntersection("Saddar");
    int karsaz = city.findIntersection("Karsaz");
    int tower  = city.findIntersection("Tower");
    int clifton= city.findIntersection("Clifton");
    check(saddar != -1 && karsaz != -1, "hash lookup by name");
    check(city.findIntersection("Hyderabad") == -1, "unknown name returns -1");

    int r = city.findRoad(saddar, karsaz);
    check(r != -1 && city.getRoad(r)->speedLimit == 70, "Saddar->Karsaz is 70 km/h");
    check(city.findRoad(karsaz, saddar) != -1, "two-way road has reverse direction");
    check(city.findRoad(tower, clifton) != -1, "one-way Tower->Clifton exists");
    check(city.findRoad(clifton, tower) == -1, "one-way has NO reverse road");
    check(city.outDegree(saddar) == 4 && city.inDegree(saddar) == 4, "Saddar degree in=4 out=4");

    // 6000 m at 70 km/h = 6000*3.6/70 = 308 s
    check(city.getRoad(r)->baseTime() == 308, "travel time = distance / speed");
}

void testCongestionAndBlocking(Graph& city) {
    cout << "\n== Congestion & blocking ==\n";
    int saddar = city.findIntersection("Saddar");
    int karsaz = city.findIntersection("Karsaz");
    Road* r = city.getRoad(city.findRoad(saddar, karsaz));

    int freeFlow = r->currentTime();
    r->vehiclesOnRoad = r->capacity();          // road exactly at capacity
    int full = r->currentTime();
    r->vehiclesOnRoad = 2 * r->capacity();      // gridlock: double capacity
    int jammed = r->currentTime();
    cout << "  capacity = " << r->capacity() << " cars, free = " << freeFlow
         << "s, at capacity = " << full << "s, gridlock = " << jammed << "s\n";
    check(r->capacity() == 72, "capacity = (6000/250) x 3 lanes = 72");
    check(full > freeFlow && jammed > full, "more cars -> slower (BPR formula)");
    r->vehiclesOnRoad = 0;

    check(city.setBlockedBetween(saddar, karsaz, true) == 2, "block both directions");
    check(r->currentTime() == INF, "blocked road time = INF");
    check(city.countBlocked() == 2, "2 roads blocked");

    city.buildMatrix(true);
    check(city.matrixAt(saddar, karsaz) == 0, "blocked road missing from matrix");
    city.setBlockedBetween(saddar, karsaz, false);
    city.buildMatrix(true);
    check(city.matrixAt(saddar, karsaz) == 308, "reopened road back in matrix");
}

void testBadInput() {
    cout << "\n== Error handling ==\n";
    Graph g;
    string err;
    check(!g.loadFromFile("no_such_file.txt", err), "missing file rejected");

    stringstream badRoad("INTERSECTION A 0 0\nROAD A Z 100 1 40 TWOWAY\n");
    check(!g.loadFromStream(badRoad, err), "unknown intersection rejected");
    cout << "    -> " << err << "\n";

    stringstream dup("INTERSECTION A 0 0\nINTERSECTION A 5 5\n");
    check(!g.loadFromStream(dup, err), "duplicate intersection rejected");
    cout << "    -> " << err << "\n";

    stringstream selfLoop("INTERSECTION A 0 0\nROAD A A 100 1 40 ONEWAY\n");
    check(!g.loadFromStream(selfLoop, err), "self-loop rejected");

    // Growth: start tiny so arrays must resize many times
    Graph big(1, 1);
    for (int i = 0; i < 50; i++) {
        stringstream name; name << "N" << i;
        big.addIntersection(name.str(), (float)i, 0);
    }
    for (int i = 0; i < 49; i++) big.addRoad(i, i + 1, 100, 1, 40);
    check(big.getNodeCount() == 50 && big.getRoadCount() == 49, "dynamic arrays grow correctly");
    check(big.findIntersection("N37") == 37, "hash still correct after growth");
}

int main() {
    cout << "===== CHAOS CITY : STEP 2 CITY GRAPH TESTS =====\n";
    Graph city;
    testLoadMap(city);
    if (city.getNodeCount() > 0) {
        testLookups(city);
        testCongestionAndBlocking(city);
        cout << "\n";
        city.printAdjList(cout);
        cout << "\n";
        city.buildMatrix(false);
        city.printMatrix(cout);
    }
    testBadInput();
    cout << "\nResult: " << passed << " passed, " << failed << " failed\n";
    cout << "\nPress Enter to exit...";
    cin.get();
    return 0;
}

