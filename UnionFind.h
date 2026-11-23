// =====================================================================
//  Chaos City - UnionFind.h
//  Disjoint Set Union (Union-Find) on intersections 0..n-1.
//   find  - which group is x in? (path compression: recursion points every
//           visited node straight at the root)
//   unite - merge two groups (union by rank: shorter tree goes under taller)
//  Both are almost O(1) amortised. Used by Kruskal's MST to reject roads
//  that would form a cycle, and to count connected components.
// =====================================================================
#ifndef UNIONFIND_H
#define UNIONFIND_H

class UnionFind {
private:
    int* parent;
    int* rank;
    int n;
    int components;

    UnionFind(const UnionFind&);
    UnionFind& operator=(const UnionFind&);

public:
    UnionFind(int size) : n(size), components(size) {
        parent = new int[n];
        rank = new int[n];
        for (int i = 0; i < n; i++) { parent[i] = i; rank[i] = 0; }
    }
    ~UnionFind() { delete[] parent; delete[] rank; }

    int find(int x) {
        if (parent[x] != x) parent[x] = find(parent[x]);   // path compression (recursive)
        return parent[x];
    }

    // Returns false if already in the same group (edge would make a cycle)
    bool unite(int a, int b) {
        int ra = find(a), rb = find(b);
        if (ra == rb) return false;
        if (rank[ra] < rank[rb]) parent[ra] = rb;
        else if (rank[ra] > rank[rb]) parent[rb] = ra;
        else { parent[rb] = ra; rank[ra]++; }
        components--;
        return true;
    }

    bool connected(int a, int b) { return find(a) == find(b); }
    int componentCount() const { return components; }
    int size() const { return n; }
};

#endif
