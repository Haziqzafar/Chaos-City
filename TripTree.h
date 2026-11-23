// =====================================================================
//  Chaos City - TripTree.h
//  Binary Search Tree of completed trips, ordered by travel time
//  (ties broken by vehicle id so every key is unique).
//
//  balanced = true  -> AVL tree: after every insert/remove, rotations keep
//                      |height(left) - height(right)| <= 1, so height is
//                      O(log n) and every operation is O(log n).
//  balanced = false -> plain BST (for comparison in the report: trips
//                      inserted in increasing order make it a linked list).
//
//  Every node also stores the SIZE of its subtree. That gives O(log n):
//    kth(k)            -> k-th fastest trip (median, 90th percentile)
//    countLess(t)      -> how many trips took less than t seconds
//    countBetween(a,b) -> histogram buckets
//
//  Traversals are recursive (in-order gives trips sorted by time).
// =====================================================================
#ifndef TRIPTREE_H
#define TRIPTREE_H

#include "Vehicle.h"
#include "LinkedList.h"

class TripTree {
private:
    struct Node {
        TripRecord trip;
        int height;
        int size;
        Node* left;
        Node* right;
        Node(const TripRecord& t) : trip(t), height(1), size(1), left(NULL), right(NULL) {}
    };

    Node* root;
    bool balanced;
    int rotations;            // counted for the report

    TripTree(const TripTree&);
    TripTree& operator=(const TripTree&);

    // ---------- key comparison: (travelTime, vehicleId) ----------
    static bool keyLess(double t1, int id1, double t2, int id2) {
        if (t1 != t2) return t1 < t2;
        return id1 < id2;
    }
    static bool nodeLess(const TripRecord& a, const TripRecord& b) {
        return keyLess(a.travelTime, a.vehicleId, b.travelTime, b.vehicleId);
    }

    static int h(Node* n) { return n ? n->height : 0; }
    static int sz(Node* n) { return n ? n->size : 0; }
    static int maxInt(int a, int b) { return a > b ? a : b; }

    static void fix(Node* n) {
        n->height = 1 + maxInt(h(n->left), h(n->right));
        n->size = 1 + sz(n->left) + sz(n->right);
    }

    // ---------- AVL rotations ----------
    //        y                x       |
    //       / \   right      / \      |
    //      x   C  ----->    A   y     |
    //     / \     <-----       / \    |
    //    A   B     left       B   C   |
    Node* rotateRight(Node* y) {
        Node* x = y->left;
        y->left = x->right;
        x->right = y;
        fix(y); fix(x);
        rotations++;
        return x;
    }

    Node* rotateLeft(Node* x) {
        Node* y = x->right;
        x->right = y->left;
        y->left = x;
        fix(x); fix(y);
        rotations++;
        return y;
    }

    Node* rebalance(Node* n) {
        fix(n);
        if (!balanced) return n;
        int bf = h(n->left) - h(n->right);           // balance factor
        if (bf > 1) {                                // left heavy
            if (h(n->left->left) < h(n->left->right))
                n->left = rotateLeft(n->left);       // Left-Right case
            return rotateRight(n);                   // Left-Left case
        }
        if (bf < -1) {                               // right heavy
            if (h(n->right->right) < h(n->right->left))
                n->right = rotateRight(n->right);    // Right-Left case
            return rotateLeft(n);                    // Right-Right case
        }
        return n;
    }

    // ---------- recursive operations ----------
    Node* insertRec(Node* n, const TripRecord& t) {
        if (n == NULL) return new Node(t);
        if (nodeLess(t, n->trip)) n->left = insertRec(n->left, t);
        else                      n->right = insertRec(n->right, t);
        return rebalance(n);
    }

    Node* removeMin(Node* n, Node*& minNode) {
        if (n->left == NULL) { minNode = n; return n->right; }
        n->left = removeMin(n->left, minNode);
        return rebalance(n);
    }

    Node* removeRec(Node* n, double time, int id, bool& removed) {
        if (n == NULL) return NULL;
        if (keyLess(time, id, n->trip.travelTime, n->trip.vehicleId))
            n->left = removeRec(n->left, time, id, removed);
        else if (keyLess(n->trip.travelTime, n->trip.vehicleId, time, id))
            n->right = removeRec(n->right, time, id, removed);
        else {
            removed = true;
            Node* l = n->left;
            Node* r = n->right;
            delete n;
            if (r == NULL) return l;
            Node* successor;                         // smallest in right subtree
            r = removeMin(r, successor);
            successor->left = l;
            successor->right = r;
            return rebalance(successor);
        }
        return rebalance(n);
    }

    void clearRec(Node* n) {                         // post-order delete
        if (n == NULL) return;
        clearRec(n->left);
        clearRec(n->right);
        delete n;
    }

    void inorderRec(Node* n, TripRecord* out, int& k) const {
        if (n == NULL) return;
        inorderRec(n->left, out, k);
        out[k++] = n->trip;
        inorderRec(n->right, out, k);
    }

    // Collect trips with lo <= time <= hi, skipping subtrees outside the range
    void rangeRec(Node* n, double lo, double hi, LinkedList<TripRecord>& out) const {
        if (n == NULL) return;
        if (n->trip.travelTime > lo) rangeRec(n->left, lo, hi, out);
        if (n->trip.travelTime >= lo && n->trip.travelTime <= hi) out.pushBack(n->trip);
        if (n->trip.travelTime < hi) rangeRec(n->right, lo, hi, out);
    }

    // Checks BST order, stored heights/sizes and (if AVL) balance
    bool validRec(Node* n, const TripRecord* lo, const TripRecord* hi, int& height, int& size) const {
        if (n == NULL) { height = 0; size = 0; return true; }
        if (lo && !nodeLess(*lo, n->trip)) return false;
        if (hi && !nodeLess(n->trip, *hi)) return false;
        int hl, hr, sl, sr;
        if (!validRec(n->left, lo, &n->trip, hl, sl)) return false;
        if (!validRec(n->right, &n->trip, hi, hr, sr)) return false;
        height = 1 + maxInt(hl, hr);
        size = 1 + sl + sr;
        if (height != n->height || size != n->size) return false;
        if (balanced && (hl - hr > 1 || hr - hl > 1)) return false;
        return true;
    }

public:
    TripTree(bool useAVL = true) : root(NULL), balanced(useAVL), rotations(0) {}
    ~TripTree() { clearRec(root); }

    void clear() { clearRec(root); root = NULL; rotations = 0; }

    void insert(const TripRecord& t) { root = insertRec(root, t); }

    bool remove(double travelTime, int vehicleId) {
        bool removed = false;
        root = removeRec(root, travelTime, vehicleId, removed);
        return removed;
    }

    int size() const { return sz(root); }
    int height() const { return h(root); }
    bool isEmpty() const { return root == NULL; }
    int getRotations() const { return rotations; }
    bool isAVL() const { return balanced; }

    bool isValid() const { int hh, ss; return validRec(root, NULL, NULL, hh, ss); }

    bool minTrip(TripRecord& out) const {
        if (!root) return false;
        Node* n = root;
        while (n->left) n = n->left;
        out = n->trip;
        return true;
    }

    bool maxTrip(TripRecord& out) const {
        if (!root) return false;
        Node* n = root;
        while (n->right) n = n->right;
        out = n->trip;
        return true;
    }

    // k-th smallest (0-based) using subtree sizes: O(height)
    bool kth(int k, TripRecord& out) const {
        if (k < 0 || k >= size()) return false;
        Node* n = root;
        while (n) {
            int leftSize = sz(n->left);
            if (k < leftSize) n = n->left;
            else if (k == leftSize) { out = n->trip; return true; }
            else { k -= leftSize + 1; n = n->right; }
        }
        return false;
    }

    // Number of trips with travelTime < t : O(height)
    int countLess(double t) const {
        int count = 0;
        Node* n = root;
        while (n) {
            if (n->trip.travelTime < t) { count += sz(n->left) + 1; n = n->right; }
            else n = n->left;
        }
        return count;
    }

    // Number of trips with lo <= travelTime < hi
    int countBetween(double lo, double hi) const { return countLess(hi) - countLess(lo); }

    // Trips sorted by travel time into caller's array (size >= size())
    int toSortedArray(TripRecord* out) const {
        int k = 0;
        inorderRec(root, out, k);
        return k;
    }

    void range(double lo, double hi, LinkedList<TripRecord>& out) const {
        out.clear();
        rangeRec(root, lo, hi, out);
    }
};

#endif
