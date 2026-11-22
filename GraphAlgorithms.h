// =====================================================================
//  Chaos City - GraphAlgorithms.h
//  Traversal and route-finding on the city Graph.
//
//   BFS       - Queue    - traversal order + route with FEWEST roads
//   DFS       - recursion AND Stack versions - traversal order, any route
//   Dijkstra  - min-Heap - route with SHORTEST TRAVEL TIME (main router)
//   Floyd-Warshall - adjacency matrix - all-pairs times (verification
//                    + DSA panel comparison)
//   Strong connectivity check - warns when closures cut the city apart
//
//  Every routing function SKIPS BLOCKED ROADS, so Chaos Mode closures
//  automatically change the answers.
//
//  Routes are returned as a LinkedList<int> of ROAD ids in travel order
//  (vehicles need roads, not just intersections). Path reconstruction
//  walks the parent array backwards and uses a Stack to reverse it.
// =====================================================================
#ifndef GRAPHALGORITHMS_H
#define GRAPHALGORITHMS_H

#include <string>
#include "Graph.h"
#include "LinkedList.h"
#include "Stack.h"
#include "Queue.h"
#include "Heap.h"

// Operation counters so the dashboard can compare algorithms fairly
struct SearchStats {
    int nodesVisited;     // vertices removed from queue/stack/heap and expanded
    int edgesChecked;     // roads examined
    int heapPushes;       // Dijkstra only
    SearchStats() : nodesVisited(0), edgesChecked(0), heapPushes(0) {}
    void reset() { nodesVisited = edgesChecked = heapPushes = 0; }
};

// (distance, vertex) pair stored in Dijkstra's priority queue
struct HeapEntry {
    int dist;
    int vertex;
    HeapEntry() : dist(0), vertex(-1) {}
    HeapEntry(int d, int v) : dist(d), vertex(v) {}
    bool operator<(const HeapEntry& o) const { return dist < o.dist; }
};

class GraphAlgorithms {
private:
    // Road weight used by Dijkstra / Floyd
    static int weight(const Road* r, bool live) {
        return live ? r->currentTime() : r->baseTime();
    }

    // Rebuild route src -> dst from parentRoad[] using a STACK.
    // parentRoad[v] = id of the road used to arrive at v.
    static void buildRoute(const Graph& g, const int* parentRoad, int src, int dst,
                           LinkedList<int>& route) {
        route.clear();
        Stack<int> st;
        int v = dst;
        while (v != src) {                 // walk backwards dst -> src
            int r = parentRoad[v];
            st.push(r);
            v = g.getRoad(r)->from;
        }
        int r;
        while (st.pop(r)) route.pushBack(r);   // pop gives src -> dst order
    }

    static void dfsRec(const Graph& g, int v, bool* visited,
                       LinkedList<int>& order, SearchStats& stats) {
        visited[v] = true;
        order.pushBack(v);
        stats.nodesVisited++;
        for (ListNode<int>* n = g.outRoads(v); n != NULL; n = n->next) {
            const Road* r = g.getRoad(n->data);
            stats.edgesChecked++;
            if (!r->blocked && !visited[r->to])
                dfsRec(g, r->to, visited, order, stats);    // RECURSION
        }
    }

    // Recursive DFS that stops as soon as dst is found.
    static bool dfsPathRec(const Graph& g, int v, int dst, bool* visited,
                           int* parentRoad, SearchStats& stats) {
        visited[v] = true;
        stats.nodesVisited++;
        if (v == dst) return true;
        for (ListNode<int>* n = g.outRoads(v); n != NULL; n = n->next) {
            const Road* r = g.getRoad(n->data);
            stats.edgesChecked++;
            if (!r->blocked && !visited[r->to]) {
                parentRoad[r->to] = r->id;
                if (dfsPathRec(g, r->to, dst, visited, parentRoad, stats)) return true;
            }
        }
        return false;
    }

    // Marks every vertex reachable from src. reverse = follow roads backwards.
    static int markReachable(const Graph& g, int src, bool* visited, bool reverse) {
        int V = g.getNodeCount(), count = 0;
        for (int i = 0; i < V; i++) visited[i] = false;
        Queue<int> q;
        q.enqueue(src);
        visited[src] = true;
        int v;
        while (q.dequeue(v)) {
            count++;
            if (!reverse) {
                for (ListNode<int>* n = g.outRoads(v); n != NULL; n = n->next) {
                    const Road* r = g.getRoad(n->data);
                    if (!r->blocked && !visited[r->to]) { visited[r->to] = true; q.enqueue(r->to); }
                }
            } else {
                // no incoming lists stored, so scan all roads (O(E) per vertex)
                for (int id = 0; id < g.getRoadCount(); id++) {
                    const Road* r = g.getRoad(id);
                    if (r->to == v && !r->blocked && !visited[r->from]) {
                        visited[r->from] = true; q.enqueue(r->from);
                    }
                }
            }
        }
        return count;
    }

public:
    // ------------------------------------------------------------
    //  BFS traversal order (level by level from src)
    // ------------------------------------------------------------
    static void bfsOrder(const Graph& g, int src, LinkedList<int>& order, SearchStats& stats) {
        order.clear(); stats.reset();
        if (!g.validNode(src)) return;
        int V = g.getNodeCount();
        bool* visited = new bool[V];
        for (int i = 0; i < V; i++) visited[i] = false;

        Queue<int> q;
        q.enqueue(src);
        visited[src] = true;
        int v;
        while (q.dequeue(v)) {
            order.pushBack(v);
            stats.nodesVisited++;
            for (ListNode<int>* n = g.outRoads(v); n != NULL; n = n->next) {
                const Road* r = g.getRoad(n->data);
                stats.edgesChecked++;
                if (!r->blocked && !visited[r->to]) {
                    visited[r->to] = true;
                    q.enqueue(r->to);
                }
            }
        }
        delete[] visited;
    }

    // ------------------------------------------------------------
    //  BFS route: fewest roads (ignores length / speed)
    //  Returns number of roads, or -1 if unreachable.
    // ------------------------------------------------------------
    static int bfsRoute(const Graph& g, int src, int dst, LinkedList<int>& route, SearchStats& stats) {
        route.clear(); stats.reset();
        if (!g.validNode(src) || !g.validNode(dst)) return -1;
        if (src == dst) return 0;
        int V = g.getNodeCount();
        bool* visited = new bool[V];
        int* parentRoad = new int[V];
        for (int i = 0; i < V; i++) { visited[i] = false; parentRoad[i] = -1; }

        Queue<int> q;
        q.enqueue(src);
        visited[src] = true;
        bool found = false;
        int v;
        while (!found && q.dequeue(v)) {
            stats.nodesVisited++;
            for (ListNode<int>* n = g.outRoads(v); n != NULL; n = n->next) {
                const Road* r = g.getRoad(n->data);
                stats.edgesChecked++;
                if (!r->blocked && !visited[r->to]) {
                    visited[r->to] = true;
                    parentRoad[r->to] = r->id;
                    if (r->to == dst) { found = true; break; }
                    q.enqueue(r->to);
                }
            }
        }
        if (found) buildRoute(g, parentRoad, src, dst, route);
        delete[] visited;
        delete[] parentRoad;
        return found ? route.size() : -1;
    }

    // ------------------------------------------------------------
    //  DFS traversal - recursive version
    // ------------------------------------------------------------
    static void dfsOrderRecursive(const Graph& g, int src, LinkedList<int>& order, SearchStats& stats) {
        order.clear(); stats.reset();
        if (!g.validNode(src)) return;
        int V = g.getNodeCount();
        bool* visited = new bool[V];
        for (int i = 0; i < V; i++) visited[i] = false;
        dfsRec(g, src, visited, order, stats);
        delete[] visited;
    }

    // ------------------------------------------------------------
    //  DFS traversal - iterative version with explicit STACK
    //  Neighbours are pushed in reverse so the visiting order matches
    //  the recursive version.
    // ------------------------------------------------------------
    static void dfsOrderIterative(const Graph& g, int src, LinkedList<int>& order, SearchStats& stats) {
        order.clear(); stats.reset();
        if (!g.validNode(src)) return;
        int V = g.getNodeCount();
        bool* visited = new bool[V];
        for (int i = 0; i < V; i++) visited[i] = false;

        Stack<int> st;
        st.push(src);
        int v;
        while (st.pop(v)) {
            if (visited[v]) continue;
            visited[v] = true;
            order.pushBack(v);
            stats.nodesVisited++;

            Stack<int> reversed;             // reverse neighbour order
            for (ListNode<int>* n = g.outRoads(v); n != NULL; n = n->next) {
                const Road* r = g.getRoad(n->data);
                stats.edgesChecked++;
                if (!r->blocked && !visited[r->to]) reversed.push(r->to);
            }
            int w;
            while (reversed.pop(w)) st.push(w);
        }
        delete[] visited;
    }

    // ------------------------------------------------------------
    //  DFS route: finds A route, usually not the best one.
    //  Shown on the dashboard to contrast with BFS / Dijkstra.
    // ------------------------------------------------------------
    static int dfsRoute(const Graph& g, int src, int dst, LinkedList<int>& route, SearchStats& stats) {
        route.clear(); stats.reset();
        if (!g.validNode(src) || !g.validNode(dst)) return -1;
        if (src == dst) return 0;
        int V = g.getNodeCount();
        bool* visited = new bool[V];
        int* parentRoad = new int[V];
        for (int i = 0; i < V; i++) { visited[i] = false; parentRoad[i] = -1; }
        bool found = dfsPathRec(g, src, dst, visited, parentRoad, stats);
        if (found) buildRoute(g, parentRoad, src, dst, route);
        delete[] visited;
        delete[] parentRoad;
        return found ? route.size() : -1;
    }

    // ------------------------------------------------------------
    //  Dijkstra: fastest route by travel time.
    //  live = true  -> uses current congestion (BPR formula)
    //  live = false -> free-flow times
    //  Returns total seconds, or INF if unreachable.
    //  Lazy deletion: outdated heap entries are skipped when popped.
    //  Complexity: O((V + E) log V)
    // ------------------------------------------------------------
    static int dijkstraRoute(const Graph& g, int src, int dst, bool live,
                             LinkedList<int>& route, SearchStats& stats) {
        route.clear(); stats.reset();
        if (!g.validNode(src) || !g.validNode(dst)) return INF;
        int V = g.getNodeCount();
        int* dist = new int[V];
        int* parentRoad = new int[V];
        bool* done = new bool[V];
        for (int i = 0; i < V; i++) { dist[i] = INF; parentRoad[i] = -1; done[i] = false; }

        Heap<HeapEntry> pq(true);
        dist[src] = 0;
        pq.push(HeapEntry(0, src));
        stats.heapPushes++;

        HeapEntry cur;
        while (pq.pop(cur)) {
            int u = cur.vertex;
            if (done[u]) continue;           // stale entry (lazy deletion)
            done[u] = true;
            stats.nodesVisited++;
            if (u == dst) break;             // dst finalised: stop early

            for (ListNode<int>* n = g.outRoads(u); n != NULL; n = n->next) {
                const Road* r = g.getRoad(n->data);
                stats.edgesChecked++;
                if (r->blocked || done[r->to]) continue;
                int nd = dist[u] + weight(r, live);
                if (nd < dist[r->to]) {      // RELAXATION
                    dist[r->to] = nd;
                    parentRoad[r->to] = r->id;
                    pq.push(HeapEntry(nd, r->to));
                    stats.heapPushes++;
                }
            }
        }

        int result = dist[dst];
        if (result != INF && src != dst) buildRoute(g, parentRoad, src, dst, route);
        delete[] dist;
        delete[] parentRoad;
        delete[] done;
        return result;
    }

    // ------------------------------------------------------------
    //  Dijkstra from one source to ALL intersections.
    //  Caller provides arrays of size V. Used by analytics/DSA panel.
    // ------------------------------------------------------------
    static void dijkstraAll(const Graph& g, int src, bool live, int* dist, int* parentRoad) {
        int V = g.getNodeCount();
        bool* done = new bool[V];
        for (int i = 0; i < V; i++) { dist[i] = INF; parentRoad[i] = -1; done[i] = false; }
        Heap<HeapEntry> pq(true);
        dist[src] = 0;
        pq.push(HeapEntry(0, src));
        HeapEntry cur;
        while (pq.pop(cur)) {
            int u = cur.vertex;
            if (done[u]) continue;
            done[u] = true;
            for (ListNode<int>* n = g.outRoads(u); n != NULL; n = n->next) {
                const Road* r = g.getRoad(n->data);
                if (r->blocked || done[r->to]) continue;
                int nd = dist[u] + weight(r, live);
                if (nd < dist[r->to]) {
                    dist[r->to] = nd;
                    parentRoad[r->to] = r->id;
                    pq.push(HeapEntry(nd, r->to));
                }
            }
        }
        delete[] done;
    }

    // ------------------------------------------------------------
    //  Floyd-Warshall: all-pairs shortest times on a V x V matrix.
    //  Caller passes a V x V int** (allocate with allocMatrix).
    //  O(V^3) - fine for a city of ~13-50 intersections.
    // ------------------------------------------------------------
    static int** allocMatrix(int V) {
        int** m = new int*[V];
        for (int i = 0; i < V; i++) m[i] = new int[V];
        return m;
    }
    static void freeMatrix(int** m, int V) {
        for (int i = 0; i < V; i++) delete[] m[i];
        delete[] m;
    }

    static void floydWarshall(const Graph& g, bool live, int** d) {
        int V = g.getNodeCount();
        for (int i = 0; i < V; i++)
            for (int j = 0; j < V; j++) d[i][j] = (i == j) ? 0 : INF;
        for (int id = 0; id < g.getRoadCount(); id++) {
            const Road* r = g.getRoad(id);
            if (!r->blocked) d[r->from][r->to] = weight(r, live);
        }
        for (int k = 0; k < V; k++)
            for (int i = 0; i < V; i++) {
                if (d[i][k] == INF) continue;
                for (int j = 0; j < V; j++)
                    if (d[k][j] != INF && d[i][k] + d[k][j] < d[i][j])
                        d[i][j] = d[i][k] + d[k][j];
            }
    }

    // ------------------------------------------------------------
    //  Connectivity
    // ------------------------------------------------------------
    // How many intersections can be reached from src (respecting closures)
    static int reachableCount(const Graph& g, int src) {
        if (!g.validNode(src)) return 0;
        bool* visited = new bool[g.getNodeCount()];
        int c = markReachable(g, src, visited, false);
        delete[] visited;
        return c;
    }

    // Directed graph is strongly connected if every vertex can reach every
    // other. Check: all reachable from vertex 0 forwards AND backwards.
    static bool isStronglyConnected(const Graph& g) {
        int V = g.getNodeCount();
        if (V == 0) return true;
        bool* visited = new bool[V];
        bool ok = markReachable(g, 0, visited, false) == V &&
                  markReachable(g, 0, visited, true) == V;
        delete[] visited;
        return ok;
    }

    // ------------------------------------------------------------
    //  Route helpers
    // ------------------------------------------------------------
    static int routeTime(const Graph& g, const LinkedList<int>& route, bool live) {
        int total = 0;
        for (ListNode<int>* n = route.getHead(); n != NULL; n = n->next) {
            const Road* r = g.getRoad(n->data);
            if (r->blocked) return INF;
            total += weight(r, live);
        }
        return total;
    }

    static int routeLength(const Graph& g, const LinkedList<int>& route) {
        int meters = 0;
        for (ListNode<int>* n = route.getHead(); n != NULL; n = n->next)
            meters += g.getRoad(n->data)->length;
        return meters;
    }

    static bool routeUsesRoad(const LinkedList<int>& route, int roadId) {
        return route.contains(roadId);
    }

    // "Tower -> Saddar -> Karsaz"
    static std::string routeToString(const Graph& g, const LinkedList<int>& route) {
        ListNode<int>* n = route.getHead();
        if (n == NULL) return "(no route)";
        std::string s = g.getIntersection(g.getRoad(n->data)->from).name;
        for (; n != NULL; n = n->next)
            s += " -> " + g.getIntersection(g.getRoad(n->data)->to).name;
        return s;
    }

    static std::string orderToString(const Graph& g, const LinkedList<int>& order) {
        std::string s;
        for (ListNode<int>* n = order.getHead(); n != NULL; n = n->next) {
            if (!s.empty()) s += ", ";
            s += g.getIntersection(n->data).name;
        }
        return s;
    }
};

#endif
