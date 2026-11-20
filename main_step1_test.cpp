// =====================================================================
//  Chaos City - Step 1 test driver
//  Verifies every core data structure before the simulation uses them.
//  Compile in Dev-C++ with: Tools > Compiler Options > add  -std=c++11
// =====================================================================
#include <iostream>
#include <string>
#include <sstream>
#include "LinkedList.h"
#include "Stack.h"
#include "Queue.h"
#include "Heap.h"
#include "HashTable.h"

using namespace std;

int passed = 0, failed = 0;

void check(bool condition, const string& testName) {
    if (condition) { passed++; cout << "  [PASS] " << testName << "\n"; }
    else           { failed++; cout << "  [FAIL] " << testName << "\n"; }
}

// Dijkstra-style heap entry: (distance, vertex)
struct DistNode {
    int dist;
    int vertex;
    DistNode() : dist(0), vertex(-1) {}
    DistNode(int d, int v) : dist(d), vertex(v) {}
    bool operator<(const DistNode& other) const { return dist < other.dist; }
};

void testLinkedList() {
    cout << "\n== LinkedList ==\n";
    LinkedList<int> list;
    list.pushBack(10); list.pushBack(20); list.pushBack(30); list.pushFront(5);
    check(list.size() == 4, "size after 4 inserts");
    check(list.getHead()->data == 5, "pushFront puts 5 at head");
    check(list.contains(20), "contains 20");
    check(list.remove(20) && !list.contains(20), "remove 20");
    list.reverse();
    check(list.getHead()->data == 30, "recursive reverse -> head is 30");
    list.pushBack(99);
    int x; list.popFront(x);
    check(x == 30 && list.size() == 3, "popFront + tail still valid after reverse");
}

void testStack() {
    cout << "\n== Stack ==\n";
    Stack<string> undo;
    undo.push("close road A-B");
    undo.push("accident at C");
    undo.push("surge at D");
    string last;
    undo.peek(last);
    check(last == "surge at D", "peek returns last action");
    undo.pop(last); undo.pop(last);
    check(last == "accident at C" && undo.size() == 1, "LIFO order");
}

void testQueue() {
    cout << "\n== Queue (linked) ==\n";
    Queue<int> signalLine;
    for (int id = 101; id <= 105; id++) signalLine.enqueue(id);
    int car;
    signalLine.dequeue(car);
    check(car == 101, "first car in is first out");
    check(signalLine.size() == 4, "size 4 after one dequeue");

    cout << "\n== CircularQueue (array) ==\n";
    CircularQueue<int> lane(3);
    check(lane.enqueue(1) && lane.enqueue(2) && lane.enqueue(3), "fill lane of 3");
    check(!lane.enqueue(4), "4th car rejected when lane full");
    lane.dequeue(car);
    check(lane.enqueue(4), "wrap-around after dequeue");
    int order[3]; for (int i = 0; i < 3; i++) lane.dequeue(order[i]);
    check(order[0] == 2 && order[1] == 3 && order[2] == 4, "wrap-around keeps FIFO order");
}

void testHeap() {
    cout << "\n== Heap (min, Dijkstra style) ==\n";
    Heap<DistNode> pq(true, 2);   // small capacity forces grow()
    pq.push(DistNode(7, 1)); pq.push(DistNode(2, 2)); pq.push(DistNode(9, 3));
    pq.push(DistNode(1, 4)); pq.push(DistNode(5, 5));
    DistNode d;
    pq.pop(d);
    check(d.dist == 1 && d.vertex == 4, "smallest distance comes out first");
    int prev = d.dist; bool sorted = true;
    while (pq.pop(d)) { if (d.dist < prev) sorted = false; prev = d.dist; }
    check(sorted, "pops come out in ascending order (grow worked)");

    cout << "\n== Heap (max, congestion) ==\n";
    Heap<int> congested(false);
    congested.push(3); congested.push(12); congested.push(7);
    int top; congested.top(top);
    check(top == 12, "max-heap top is most congested");
}

void testHashTable() {
    cout << "\n== HashTable ==\n";
    HashTable<int, string> vehicles(4);   // small to force rehash
    for (int id = 1; id <= 20; id++) {
        stringstream ss; ss << "Car#" << id;   // avoids to_string (broken on old MinGW)
        vehicles.put(id, ss.str());
    }
    check(vehicles.size() == 20, "20 vehicles stored");
    check(vehicles.getBucketCount() > 4, "rehash grew the table");
    string v;
    check(vehicles.get(13, v) && v == "Car#13", "lookup id 13");
    vehicles.put(13, "Ambulance#13");
    vehicles.get(13, v);
    check(v == "Ambulance#13" && vehicles.size() == 20, "update keeps size");
    check(vehicles.remove(13) && !vehicles.contains(13), "remove id 13");

    HashTable<string, int> junctions;
    junctions.put("Saddar", 0); junctions.put("Clifton", 1); junctions.put("Gulshan", 2);
    int idx;
    check(junctions.get("Clifton", idx) && idx == 1, "string key lookup");
    cout << "  load factor = " << vehicles.loadFactor()
         << ", collisions = " << vehicles.getCollisions() << "\n";
}

int main() {
    cout << "===== CHAOS CITY : STEP 1 DATA STRUCTURE TESTS =====\n";
    testLinkedList();
    testStack();
    testQueue();
    testHeap();
    testHashTable();
    cout << "\nResult: " << passed << " passed, " << failed << " failed\n";
    cout << "\nPress Enter to exit...";
    cin.get();
    return 0;
}
