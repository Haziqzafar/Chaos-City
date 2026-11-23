// =====================================================================
//  Chaos City - TopoSort.h
//  Topological sorting of a directed graph (order where every edge u->v
//  has u before v). Only possible if the graph has NO directed cycle.
//
//   Kahn's algorithm - repeatedly take a vertex with in-degree 0 (QUEUE)
//   DFS algorithm    - recursive DFS, push each vertex on a STACK when it
//                      finishes; colours WHITE/GRAY/BLACK detect cycles
//                      (reaching a GRAY vertex = back edge = cycle).
//
//  Uses in Chaos City:
//   1) Cycle check on the city itself: real road networks have cycles,
//      so both algorithms must report "not a DAG".
//   2) Evacuation / dispatch plan from a hotspot: keep only roads that
//      lead strictly FURTHER from the hotspot (by Dijkstra time). That
//      graph is always a DAG; its topological order is the order in which
//      intersections should be cleared / signals set to green (a "green
//      wave" moving outwards).
// =====================================================================
#ifndef TOPOSORT_H
#define TOPOSORT_H

#include "Graph.h"
#include "GraphAlgorithms.h"
#include "LinkedList.h"
#include "Queue.h"
#include "Stack.h"

// Small stand-alone directed graph (adjacency list)
class DirectedGraph {
private:
    int V;
    LinkedList<int>** adj;
    int edges;

    DirectedGraph(const DirectedGraph&);
    DirectedGraph& operator=(const DirectedGraph&);

    void freeAll() {
        for (int i = 0; i < V; i++) delete adj[i];
        delete[] adj;
        adj = NULL;
    }

public:
    DirectedGraph(int n = 0) : V(0), adj(NULL), edges(0) { reset(n); }
    ~DirectedGraph() { freeAll(); }

    void reset(int n) {
        if (adj) freeAll();
        V = n; edges = 0;
        adj = new LinkedList<int>*[V > 0 ? V : 1];
        for (int i = 0; i < V; i++) adj[i] = new LinkedList<int>();
    }

    void addEdge(int u, int v) { adj[u]->pushBack(v); edges++; }
    int vertexCount() const { return V; }
    int edgeCount() const { return edges; }
    ListNode<int>* neighbours(int u) const { return adj[u]->getHead(); }
};

class TopoSort {
private:
    enum Colour { WHITE = 0, GRAY = 1, BLACK = 2 };

    static bool dfsVisit(const DirectedGraph& g, int u, int* colour, Stack<int>& finished) {
        colour[u] = GRAY;                                // on the current path
        for (ListNode<int>* n = g.neighbours(u); n != NULL; n = n->next) {
            int v = n->data;
            if (colour[v] == GRAY) return false;         // back edge -> cycle
            if (colour[v] == WHITE && !dfsVisit(g, v, colour, finished)) return false;
        }
        colour[u] = BLACK;
        finished.push(u);                                // all descendants done
        return true;
    }

public:
    // Kahn. Returns true and fills order[0..V-1] if the graph is a DAG.
    static bool kahn(const DirectedGraph& g, int* order) {
        int V = g.vertexCount();
        int* indeg = new int[V];
        for (int i = 0; i < V; i++) indeg[i] = 0;
        for (int u = 0; u < V; u++)
            for (ListNode<int>* n = g.neighbours(u); n != NULL; n = n->next) indeg[n->data]++;

        Queue<int> q;
        for (int i = 0; i < V; i++) if (indeg[i] == 0) q.enqueue(i);
        int k = 0, u;
        while (q.dequeue(u)) {
            order[k++] = u;
            for (ListNode<int>* n = g.neighbours(u); n != NULL; n = n->next)
                if (--indeg[n->data] == 0) q.enqueue(n->data);
        }
        delete[] indeg;
        return k == V;                                   // fewer -> a cycle blocked some
    }

    // DFS version. Returns false as soon as a cycle is found.
    static bool dfs(const DirectedGraph& g, int* order) {
        int V = g.vertexCount();
        int* colour = new int[V];
        for (int i = 0; i < V; i++) colour[i] = WHITE;
        Stack<int> finished;
        bool ok = true;
        for (int i = 0; i < V && ok; i++)
            if (colour[i] == WHITE) ok = dfsVisit(g, i, colour, finished);
        delete[] colour;
        if (!ok) return false;
        int k = 0, u;
        while (finished.pop(u)) order[k++] = u;          // reverse finishing order
        return true;
    }

    // Every edge u->v must have u earlier in 'order'
    static bool isValidOrder(const DirectedGraph& g, const int* order) {
        int V = g.vertexCount();
        int* pos = new int[V];
        for (int i = 0; i < V; i++) pos[order[i]] = i;
        bool ok = true;
        for (int u = 0; u < V && ok; u++)
            for (ListNode<int>* n = g.neighbours(u); n != NULL; n = n->next)
                if (pos[u] > pos[n->data]) { ok = false; break; }
        delete[] pos;
        return ok;
    }

    // ---------- builders from the city ----------
    static void fromCity(const Graph& city, DirectedGraph& out) {
        out.reset(city.getNodeCount());
        for (int r = 0; r < city.getRoadCount(); r++) {
            const Road* road = city.getRoad(r);
            if (!road->blocked) out.addEdge(road->from, road->to);
        }
    }

    // Roads leading strictly further (in travel time) from the source.
    // Unreachable intersections get no edges. dist must have size V.
    static void evacuationDag(const Graph& city, int source, bool liveTraffic,
                              DirectedGraph& out, int* dist) {
        int V = city.getNodeCount();
        int* parent = new int[V];
        GraphAlgorithms::dijkstraAll(city, source, liveTraffic, dist, parent);
        delete[] parent;
        out.reset(V);
        for (int r = 0; r < city.getRoadCount(); r++) {
            const Road* road = city.getRoad(r);
            if (road->blocked) continue;
            int u = road->from, v = road->to;
            if (dist[u] != INF && dist[v] != INF && dist[u] < dist[v]) out.addEdge(u, v);
        }
    }
};

#endif
