// =====================================================================
//  Chaos City - Graph.h
//  The city road network as a DIRECTED WEIGHTED GRAPH.
//
//  Two representations are kept (both are required syllabus topics):
//   1) Adjacency LIST   - array of pointers to LinkedList<int> holding
//                         outgoing road ids. Memory O(V + E).
//                         Used by the simulation, BFS, DFS, Dijkstra.
//   2) Adjacency MATRIX - V x V 2D dynamic array of travel times.
//                         Memory O(V^2), O(1) edge lookup.
//                         Used by Prim's MST and the DSA panel.
//
//  A HashTable<string,int> maps intersection names to ids in O(1).
//  All arrays grow dynamically (new[] / delete[]).
// =====================================================================
#ifndef GRAPH_H
#define GRAPH_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "City.h"
#include "LinkedList.h"
#include "HashTable.h"

class Graph {
private:
    Intersection* nodes;          // dynamic array of vertices
    int nodeCount, nodeCap;

    Road* roads;                  // dynamic array of edges (index == road id)
    int roadCount, roadCap;

    LinkedList<int>** adj;        // adj[v] -> list of outgoing road ids
                                  // (array of POINTERS so growing the array
                                  //  never copies a list)

    int** matrix;                 // adjacency matrix, built on request
    int matrixSize;

    HashTable<std::string, int> nameIndex;

    Graph(const Graph&);
    Graph& operator=(const Graph&);

    void growNodes() {
        int newCap = nodeCap * 2;
        Intersection* n = new Intersection[newCap];
        LinkedList<int>** a = new LinkedList<int>*[newCap];
        for (int i = 0; i < nodeCount; i++) { n[i] = nodes[i]; a[i] = adj[i]; }
        delete[] nodes;
        delete[] adj;
        nodes = n; adj = a; nodeCap = newCap;
    }

    void growRoads() {
        int newCap = roadCap * 2;
        Road* r = new Road[newCap];
        for (int i = 0; i < roadCount; i++) r[i] = roads[i];
        delete[] roads;
        roads = r; roadCap = newCap;
    }

    void freeMatrix() {
        if (matrix == NULL) return;
        for (int i = 0; i < matrixSize; i++) delete[] matrix[i];
        delete[] matrix;
        matrix = NULL;
        matrixSize = 0;
    }

public:
    Graph(int initialNodes = 8, int initialRoads = 16)
        : nodeCount(0), nodeCap(initialNodes),
          roadCount(0), roadCap(initialRoads),
          matrix(NULL), matrixSize(0) {
        nodes = new Intersection[nodeCap];
        adj   = new LinkedList<int>*[nodeCap];
        roads = new Road[roadCap];
    }

    ~Graph() { clear(); delete[] nodes; delete[] adj; delete[] roads; }

    void clear() {
        for (int i = 0; i < nodeCount; i++) delete adj[i];
        nodeCount = 0;
        roadCount = 0;
        nameIndex.clear();
        freeMatrix();
    }

    // ------------------------------------------------------------
    //  Building the graph
    // ------------------------------------------------------------

    // Returns new id, or -1 if the name already exists / is empty.
    int addIntersection(const std::string& name, float x, float y) {
        if (name.empty() || nameIndex.contains(name)) return -1;
        if (nodeCount == nodeCap) growNodes();
        int id = nodeCount;
        nodes[id] = Intersection(id, name, x, y);
        adj[id] = new LinkedList<int>();
        nameIndex.put(name, id);
        nodeCount++;
        return id;
    }

    // One directed road from -> to. Returns road id or -1 if invalid
    // (bad ids, self-loop, non-positive values, or duplicate road).
    int addRoad(int from, int to, int length, int lanes, int speedLimit) {
        if (!validNode(from) || !validNode(to) || from == to) return -1;
        if (length <= 0 || lanes <= 0 || speedLimit <= 0) return -1;
        if (findRoad(from, to) != -1) return -1;
        if (roadCount == roadCap) growRoads();
        int id = roadCount;
        roads[id] = Road(id, from, to, length, lanes, speedLimit);
        adj[from]->pushBack(id);
        roadCount++;
        return id;
    }

    // Convenience: by name, optionally creating both directions.
    bool addRoadByName(const std::string& a, const std::string& b,
                       int length, int lanes, int speed, bool twoWay) {
        int u = findIntersection(a), v = findIntersection(b);
        if (u == -1 || v == -1) return false;
        if (addRoad(u, v, length, lanes, speed) == -1) return false;
        if (twoWay && addRoad(v, u, length, lanes, speed) == -1) return false;
        return true;
    }

    // ------------------------------------------------------------
    //  Lookups
    // ------------------------------------------------------------
    bool validNode(int v) const { return v >= 0 && v < nodeCount; }
    bool validRoad(int r) const { return r >= 0 && r < roadCount; }

    // O(1) average via hash table
    int findIntersection(const std::string& name) const {
        int id;
        return nameIndex.get(name, id) ? id : -1;
    }

    // O(out-degree): walk the adjacency list of 'from'
    int findRoad(int from, int to) const {
        if (!validNode(from)) return -1;
        for (ListNode<int>* n = adj[from]->getHead(); n != NULL; n = n->next)
            if (roads[n->data].to == to) return n->data;
        return -1;
    }

    int getNodeCount() const { return nodeCount; }
    int getRoadCount() const { return roadCount; }

    const Intersection& getIntersection(int v) const { return nodes[v]; }
    Road* getRoad(int r) { return validRoad(r) ? &roads[r] : NULL; }
    const Road* getRoad(int r) const { return validRoad(r) ? &roads[r] : NULL; }

    // Head of the outgoing-road list, for traversal by algorithms:
    // for (ListNode<int>* n = g.outRoads(v); n; n = n->next) { Road* r = g.getRoad(n->data); }
    ListNode<int>* outRoads(int v) const { return validNode(v) ? adj[v]->getHead() : NULL; }

    int outDegree(int v) const { return validNode(v) ? adj[v]->size() : 0; }

    int inDegree(int v) const {
        int d = 0;
        for (int r = 0; r < roadCount; r++) if (roads[r].to == v) d++;
        return d;
    }

    // ------------------------------------------------------------
    //  Road state (used by Chaos Mode)
    // ------------------------------------------------------------
    bool setBlocked(int roadId, bool value) {
        if (!validRoad(roadId)) return false;
        roads[roadId].blocked = value;
        return true;
    }

    // Close/open both directions between two intersections
    int setBlockedBetween(int u, int v, bool value) {
        int changed = 0;
        int r1 = findRoad(u, v), r2 = findRoad(v, u);
        if (r1 != -1) { roads[r1].blocked = value; changed++; }
        if (r2 != -1) { roads[r2].blocked = value; changed++; }
        return changed;
    }

    int countBlocked() const {
        int c = 0;
        for (int r = 0; r < roadCount; r++) if (roads[r].blocked) c++;
        return c;
    }

    // ------------------------------------------------------------
    //  Adjacency matrix
    //  matrix[u][v] = travel time of road u->v, 0 if no usable road.
    //  useLiveTraffic = true -> includes congestion; blocked roads = 0.
    // ------------------------------------------------------------
    void buildMatrix(bool useLiveTraffic) {
        freeMatrix();
        matrixSize = nodeCount;
        matrix = new int*[matrixSize];
        for (int i = 0; i < matrixSize; i++) {
            matrix[i] = new int[matrixSize];
            for (int j = 0; j < matrixSize; j++) matrix[i][j] = 0;
        }
        for (int r = 0; r < roadCount; r++) {
            if (roads[r].blocked) continue;
            matrix[roads[r].from][roads[r].to] =
                useLiveTraffic ? roads[r].currentTime() : roads[r].baseTime();
        }
    }

    int matrixAt(int u, int v) const {
        if (matrix == NULL || u < 0 || v < 0 || u >= matrixSize || v >= matrixSize) return 0;
        return matrix[u][v];
    }

    // ------------------------------------------------------------
    //  Loading a map
    //  Format (one item per line, '#' starts a comment):
    //    INTERSECTION <name> <x> <y>
    //    ROAD <from> <to> <lengthMeters> <lanes> <speedKmh> <TWOWAY|ONEWAY>
    //  Names cannot contain spaces (use underscores).
    // ------------------------------------------------------------
    bool loadFromStream(std::istream& in, std::string& error) {
        clear();
        std::string line;
        int lineNo = 0;
        while (std::getline(in, line)) {
            lineNo++;
            if (!line.empty() && line[line.length() - 1] == '\r')   // Windows line endings
                line.erase(line.length() - 1);
            if (lineNo == 1 && line.length() >= 3 && (unsigned char)line[0] == 0xEF &&
                (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
                line.erase(0, 3);                                    // UTF-8 marker added by Notepad
            std::stringstream ss(line);
            std::string type;
            if (!(ss >> type) || type[0] == '#') continue;          // blank or comment

            std::stringstream where;
            where << "line " << lineNo << ": ";

            if (type == "INTERSECTION") {
                std::string name; float x, y;
                if (!(ss >> name >> x >> y)) { error = where.str() + "expected INTERSECTION name x y"; return false; }
                if (addIntersection(name, x, y) == -1) { error = where.str() + "duplicate intersection '" + name + "'"; return false; }
            }
            else if (type == "ROAD") {
                std::string a, b, dir; int len, lanes, speed;
                if (!(ss >> a >> b >> len >> lanes >> speed >> dir)) {
                    error = where.str() + "expected ROAD from to length lanes speed TWOWAY|ONEWAY"; return false;
                }
                if (dir != "TWOWAY" && dir != "ONEWAY") { error = where.str() + "direction must be TWOWAY or ONEWAY"; return false; }
                if (findIntersection(a) == -1) { error = where.str() + "unknown intersection '" + a + "'"; return false; }
                if (findIntersection(b) == -1) { error = where.str() + "unknown intersection '" + b + "'"; return false; }
                if (!addRoadByName(a, b, len, lanes, speed, dir == "TWOWAY")) {
                    error = where.str() + "invalid or duplicate road " + a + " -> " + b; return false;
                }
            }
            else { error = where.str() + "unknown keyword '" + type + "'"; return false; }
        }
        if (nodeCount == 0) { error = "map contains no intersections"; return false; }
        return true;
    }

    bool loadFromFile(const std::string& path, std::string& error) {
        std::ifstream file(path.c_str());
        if (!file) { error = "cannot open file '" + path + "'"; return false; }
        return loadFromStream(file, error);
    }

    // ------------------------------------------------------------
    //  Console printing (for testing; the GUI will draw instead)
    // ------------------------------------------------------------
    void printAdjList(std::ostream& out) const {
        out << "Adjacency List (road: destination, length m, travel s)\n";
        for (int v = 0; v < nodeCount; v++) {
            out << "  [" << v << "] " << nodes[v].name << " ->";
            for (ListNode<int>* n = adj[v]->getHead(); n != NULL; n = n->next) {
                const Road& r = roads[n->data];
                out << "  " << nodes[r.to].name << "(" << r.length << "m," << r.baseTime() << "s"
                    << (r.blocked ? ",BLOCKED" : "") << ")";
            }
            out << "\n";
        }
    }

    void printMatrix(std::ostream& out) const {
        if (matrix == NULL) { out << "(matrix not built)\n"; return; }
        out << "Adjacency Matrix (travel time in seconds, . = no road)\n     ";
        for (int j = 0; j < matrixSize; j++) { out.width(5); out << j; }
        out << "\n";
        for (int i = 0; i < matrixSize; i++) {
            out.width(4); out << i << " ";
            for (int j = 0; j < matrixSize; j++) {
                out.width(5);
                if (matrix[i][j] == 0) out << "."; else out << matrix[i][j];
            }
            out << "   " << nodes[i].name << "\n";
        }
    }
};

#endif
