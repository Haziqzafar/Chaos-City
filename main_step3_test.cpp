// =====================================================================
//  Chaos City - Step 3 test driver: BFS, DFS, Dijkstra, Floyd-Warshall
//  Keep city_map.txt in the same folder.
// =====================================================================
#include <iostream>
#include <string>
#include "Graph.h"
#include "GraphAlgorithms.h"

using namespace std;

int passed = 0, failed = 0;

void check(bool condition, const string& testName) {
    if (condition) { passed++; cout << "  [PASS] " << testName << "\n"; }
    else           { failed++; cout << "  [FAIL] " << testName << "\n"; }
}

typedef GraphAlgorithms GA;

void testTraversals(Graph& city) {
    cout << "\n== Traversals from Tower ==\n";
    int tower = city.findIntersection("Tower");
    LinkedList<int> order;
    SearchStats s;

    GA::bfsOrder(city, tower, order, s);
    cout << "  BFS: " << GA::orderToString(city, order) << "\n";
    check(order.size() == 13, "BFS reaches all 13 intersections");
    ListNode<int>* n = order.getHead();
    check(n->data == tower && city.getIntersection(n->next->data).name == "Saddar",
          "BFS visits Tower, then its first neighbour Saddar");

    LinkedList<int> rec, itr;
    GA::dfsOrderRecursive(city, tower, rec, s);
    GA::dfsOrderIterative(city, tower, itr, s);
    cout << "  DFS (recursive): " << GA::orderToString(city, rec) << "\n";
    cout << "  DFS (stack):     " << GA::orderToString(city, itr) << "\n";
    check(rec.size() == 13 && itr.size() == 13, "both DFS versions reach all 13");
    check(GA::orderToString(city, rec) == GA::orderToString(city, itr),
          "recursive and stack DFS give identical order");
}

void testRouting(Graph& city) {
    cout << "\n== Routing Tower -> Malir ==\n";
    int tower = city.findIntersection("Tower");
    int malir = city.findIntersection("Malir");
    LinkedList<int> route;
    SearchStats s;

    int roads = GA::bfsRoute(city, tower, malir, route, s);
    cout << "  BFS      : " << GA::routeToString(city, route) << "  (" << roads
         << " roads, " << GA::routeTime(city, route, false) << "s, visited "
         << s.nodesVisited << ")\n";
    check(roads == 4, "BFS: fewest roads = 4");

    int dfsRoads = GA::dfsRoute(city, tower, malir, route, s);
    cout << "  DFS      : " << GA::routeToString(city, route) << "  (" << dfsRoads
         << " roads, " << GA::routeTime(city, route, false) << "s, visited "
         << s.nodesVisited << ")\n";
    check(dfsRoads > 0, "DFS finds some route");

    int t = GA::dijkstraRoute(city, tower, malir, false, route, s);
    cout << "  Dijkstra : " << GA::routeToString(city, route) << "  (" << t
         << "s, " << GA::routeLength(city, route) << "m, visited " << s.nodesVisited
         << ", heap pushes " << s.heapPushes << ")\n";
    // Tower-Saddar 225 + Saddar-Karsaz 308 + Karsaz-Airport 411 + Airport-Malir 360
    check(t == 1304, "Dijkstra fastest = 1304s");
    check(GA::routeTime(city, route, false) == t, "route's summed time equals returned time");
    check(GA::routeToString(city, route) == "Tower -> Saddar -> Karsaz -> Airport -> Malir",
          "Dijkstra path via Shahrah-e-Faisal");
}

void testAgainstFloyd(Graph& city) {
    cout << "\n== Dijkstra vs Floyd-Warshall (all 169 pairs) ==\n";
    int V = city.getNodeCount();
    int** d = GA::allocMatrix(V);
    GA::floydWarshall(city, false, d);
    LinkedList<int> route;
    SearchStats s;
    int mismatches = 0;
    for (int i = 0; i < V; i++)
        for (int j = 0; j < V; j++)
            if (GA::dijkstraRoute(city, i, j, false, route, s) != d[i][j]) mismatches++;
    check(mismatches == 0, "Dijkstra matches Floyd-Warshall for every pair");

    int* dist = new int[V];
    int* parent = new int[V];
    GA::dijkstraAll(city, 0, false, dist, parent);
    bool same = true;
    for (int j = 0; j < V; j++) if (dist[j] != d[0][j]) same = false;
    check(same, "dijkstraAll matches Floyd-Warshall row");
    delete[] dist; delete[] parent;
    GA::freeMatrix(d, V);
}

void testChaos(Graph& city) {
    cout << "\n== Chaos: closures & congestion ==\n";
    int tower = city.findIntersection("Tower");
    int malir = city.findIntersection("Malir");
    int karsaz = city.findIntersection("Karsaz");
    int airport = city.findIntersection("Airport");
    int saddar = city.findIntersection("Saddar");
    LinkedList<int> route;
    SearchStats s;

    // 1) Accident closes Karsaz <-> Airport
    city.setBlockedBetween(karsaz, airport, true);
    int t = GA::dijkstraRoute(city, tower, malir, false, route, s);
    cout << "  Karsaz-Airport closed : " << GA::routeToString(city, route) << " (" << t << "s)\n";
    check(t > 1304 && t != INF, "reroute found, slower than 1304s");
    check(!GA::routeUsesRoad(route, city.findRoad(karsaz, airport)), "new route avoids closed road");
    city.setBlockedBetween(karsaz, airport, false);

    // 2) Gridlock on Saddar -> Karsaz, live-traffic routing avoids it
    Road* sk = city.getRoad(city.findRoad(saddar, karsaz));
    sk->vehiclesOnRoad = 2 * sk->capacity();
    int liveT = GA::dijkstraRoute(city, tower, malir, true, route, s);
    cout << "  Saddar->Karsaz jammed : " << GA::routeToString(city, route) << " (" << liveT << "s live)\n";
    check(!GA::routeUsesRoad(route, sk->id), "live routing avoids the jammed road");
    sk->vehiclesOnRoad = 0;

    // 3) Malir completely cut off (block every road INTO Malir)
    for (int id = 0; id < city.getRoadCount(); id++)
        if (city.getRoad(id)->to == malir) city.setBlocked(id, true);
    t = GA::dijkstraRoute(city, tower, malir, false, route, s);
    check(t == INF && route.isEmpty(), "unreachable destination -> INF, empty route");
    check(GA::bfsRoute(city, tower, malir, route, s) == -1, "BFS also reports unreachable");
    check(!GA::isStronglyConnected(city), "city no longer strongly connected");
    cout << "  reachable from Tower: " << GA::reachableCount(city, tower) << " / 13\n";
    for (int id = 0; id < city.getRoadCount(); id++) city.setBlocked(id, false);
    check(GA::isStronglyConnected(city), "all reopened -> strongly connected again");

    // 4) Same source and destination
    check(GA::dijkstraRoute(city, tower, tower, false, route, s) == 0 && route.isEmpty(),
          "src == dst gives 0s and empty route");
}

int main() {
    cout << "===== CHAOS CITY : STEP 3 GRAPH ALGORITHM TESTS =====\n";
    Graph city;
    string err;
    if (!city.loadFromFile("city_map.txt", err)) {
        cout << "Cannot load map: " << err << "\n";
        cin.get();
        return 1;
    }
    check(GA::isStronglyConnected(city), "original map is strongly connected");
    testTraversals(city);
    testRouting(city);
    testAgainstFloyd(city);
    testChaos(city);
    cout << "\nResult: " << passed << " passed, " << failed << " failed\n";
    cout << "\nPress Enter to exit...";
    cin.get();
    return 0;
}
