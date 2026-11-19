// =====================================================================
//  Chaos City - MST.h
//  Minimum Spanning Tree of the road network.
//  Question it answers: "What is the cheapest set of roads (by total length)
//  that keeps every intersection connected?" -> the core network to keep
//  open and maintain first during Chaos Mode / emergencies.
//
//  Roads are treated as UNDIRECTED here (a road connects two places);
//  if both directions exist, the shorter length is used. Blocked roads
//  are ignored, so after closures the result may be a spanning FOREST.
//
//   Prim    - grows one tree from intersection 0, always adding the
//             cheapest edge leaving the tree. Uses the ADJACENCY MATRIX,
//             O(V^2) - ideal for small dense graphs.
//   Kruskal - sorts all edges (merge sort) and adds them shortest-first,
//             skipping any that would create a cycle (Union-Find).
//             O(E log E).
//  Both must give the same total length.
// =====================================================================
#ifndef MST_H
#define MST_H

#include "Graph.h"
#include "Sorting.h"
#include "UnionFind.h"

struct MSTEdge {
    int u, v;          // intersections (u < v)
    int weight;        // meters
    MSTEdge() : u(-1), v(-1), weight(0) {}
    MSTEdge(int a, int b, int w) : u(a < b ? a : b), v(a < b ? b : a), weight(w) {}
    bool operator<(const MSTEdge& o) const {
        if (weight != o.weight) return weight < o.weight;
        if (u != o.u) return u < o.u;
        return v < o.v;
    }
};

struct MSTResult {
    int totalWeight;       // meters
    int edgeCount;
    int components;        // 1 = whole city connected
    long long work;        // matrix cells scanned (Prim) / edges examined (Kruskal)
    MSTResult() : totalWeight(0), edgeCount(0), components(0), work(0) {}
};

class MST {
private:
    // Undirected weight matrix from unblocked roads (0 = no road)
    static int** buildMatrix(const Graph& g) {
        int V = g.getNodeCount();
        int** w = new int*[V];
        for (int i = 0; i < V; i++) {
            w[i] = new int[V];
            for (int j = 0; j < V; j++) w[i][j] = 0;
        }
        for (int r = 0; r < g.getRoadCount(); r++) {
            const Road* road = g.getRoad(r);
            if (road->blocked) continue;
            int a = road->from, b = road->to;
            if (w[a][b] == 0 || road->length < w[a][b]) w[a][b] = w[b][a] = road->length;
        }
        return w;
    }

    static void freeMatrix(int** w, int V) {
        for (int i = 0; i < V; i++) delete[] w[i];
        delete[] w;
    }

public:
    // out must have room for V-1 edges
    static MSTResult prim(const Graph& g, MSTEdge* out) {
        MSTResult res;
        int V = g.getNodeCount();
        if (V == 0) return res;
        int** w = buildMatrix(g);
        bool* inTree = new bool[V];
        int* best = new int[V];          // cheapest edge connecting v to the tree
        int* bestFrom = new int[V];
        for (int i = 0; i < V; i++) { inTree[i] = false; best[i] = INF; bestFrom[i] = -1; }

        for (int added = 0; added < V; added++) {
            int u = -1;                  // cheapest vertex not yet in tree
            for (int v = 0; v < V; v++) {
                res.work++;
                if (!inTree[v] && (u == -1 || best[v] < best[u])) u = v;
            }
            if (best[u] == INF) res.components++;        // starts a new tree (forest)
            else {
                out[res.edgeCount++] = MSTEdge(bestFrom[u], u, best[u]);
                res.totalWeight += best[u];
            }
            inTree[u] = true;
            for (int v = 0; v < V; v++) {                // update neighbours via matrix row
                res.work++;
                if (!inTree[v] && w[u][v] != 0 && w[u][v] < best[v]) {
                    best[v] = w[u][v];
                    bestFrom[v] = u;
                }
            }
        }
        delete[] inTree; delete[] best; delete[] bestFrom;
        freeMatrix(w, V);
        return res;
    }

    static MSTResult kruskal(const Graph& g, MSTEdge* out) {
        MSTResult res;
        int V = g.getNodeCount();
        if (V == 0) return res;
        int** w = buildMatrix(g);
        int E = 0;
        for (int i = 0; i < V; i++) for (int j = i + 1; j < V; j++) if (w[i][j]) E++;
        MSTEdge* edges = new MSTEdge[E > 0 ? E : 1];
        int k = 0;
        for (int i = 0; i < V; i++) for (int j = i + 1; j < V; j++)
            if (w[i][j]) edges[k++] = MSTEdge(i, j, w[i][j]);
        freeMatrix(w, V);

        SortStats s;
        Sorting::mergeSort(edges, E, DefaultLess(), s);   // shortest first

        UnionFind uf(V);
        for (int i = 0; i < E && res.edgeCount < V - 1; i++) {
            res.work++;
            if (uf.unite(edges[i].u, edges[i].v)) {       // false -> would make a cycle
                out[res.edgeCount++] = edges[i];
                res.totalWeight += edges[i].weight;
            }
        }
        res.components = uf.componentCount();
        delete[] edges;
        return res;
    }

    // Total length of ALL usable roads counted once (for "MST keeps x% of network")
    static int networkLength(const Graph& g) {
        int** w = buildMatrix(g);
        int V = g.getNodeCount(), total = 0;
        for (int i = 0; i < V; i++) for (int j = i + 1; j < V; j++) total += w[i][j];
        freeMatrix(w, V);
        return total;
    }
};

#endif
