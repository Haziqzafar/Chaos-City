// =====================================================================
//  Chaos City - VehicleManager.h
//  Creates, moves, routes and removes all vehicles.
//
//  Data structures:
//   HashTable<int, Vehicle*>  registry   - find any vehicle by ID in O(1)
//   LinkedList<Vehicle*>      active     - iterate all vehicles each tick
//   Queue<int>* roadQueue[r]             - vehicles waiting at the END of
//                                          road r to cross the intersection
//   Queue<int>* entryQueue[r]            - newly spawned vehicles waiting
//                                          at the START of road r because
//                                          it is full
//   LinkedList<TripRecord>    trips      - finished trips for analytics
//
//  One simulation tick:
//   1) update(dt)          - every MOVING vehicle drives forward; those
//                            reaching the end of a road join its queue
//   2) release vehicles    - Step 4: processUncontrolled() lets every
//                            queue go (no signals yet).
//                            Step 5: traffic lights call releaseFromRoad()
//                            only for roads that have a green light.
//
//  Spillback: a vehicle cannot enter the next road if that road is full,
//  so it stays at the head of its queue and blocks the cars behind it.
// =====================================================================
#ifndef VEHICLEMANAGER_H
#define VEHICLEMANAGER_H

#include "Vehicle.h"
#include "Graph.h"
#include "GraphAlgorithms.h"
#include "HashTable.h"
#include "LinkedList.h"
#include "Queue.h"
#include "Stack.h"

const double STUCK_TIMEOUT = 300.0;   // seconds before a stuck vehicle is diverted away

// Saturation flow: how fast a queue discharges on green.
// 0.25 simulated cars per second per lane (one car every 4 s per lane).
// One simulated car = 250 m of real traffic, so this keeps the scale consistent.
const double SATURATION_FLOW = 0.25;

// Small deterministic random generator (same results on every PC,
// unlike rand() which differs between compilers)
class SimpleRandom {
private:
    unsigned int state;
public:
    SimpleRandom(unsigned int seed = 12345) : state(seed) {}
    void seed(unsigned int s) { state = s; }
    unsigned int next() {                        // linear congruential generator
        state = state * 1103515245u + 12345u;
        return (state >> 16) & 0x7FFF;
    }
    int nextInt(int n) { return n <= 0 ? 0 : (int)(next() % (unsigned int)n); }
};

class VehicleManager {
private:
    Graph& graph;
    HashTable<int, Vehicle*> registry;
    LinkedList<Vehicle*> active;
    Queue<int>** roadQueue;            // one queue per road (at its end)
    Queue<int>** entryQueue;           // one queue per road (at its start)
    double* flowCredit;                // fractional "cars allowed to cross" per road
    int queueCount;
    LinkedList<TripRecord> trips;
    SimpleRandom rng;

    int nextId;
    double simTime;
    int spawned, arrived, rejected, divertedStuck, cancelled;
    double totalTravelTime;

    VehicleManager(const VehicleManager&);
    VehicleManager& operator=(const VehicleManager&);

    // Speed in m/s on a road: limited by vehicle AND road, slowed by congestion
    // A CLOSED road still lets vehicles already on it drive out;
    // an ACCIDENT (incident) stops them where they are.
    double speedOn(const Vehicle* v, const Road* r) const {
        if (r->incident) return 0;
        int kmh = v->maxSpeed() < r->speedLimit ? v->maxSpeed() : r->speedLimit;
        return kmh / 3.6 / r->slowdown();
    }

    // Remove one id from the middle of a queue: dequeue everything and
    // enqueue back all other ids (O(n), order preserved)
    static bool removeFromQueue(Queue<int>* q, int id) {
        int n = q->size(), x;
        bool found = false;
        for (int i = 0; i < n; i++) {
            q->dequeue(x);
            if (x == id && !found) found = true;
            else q->enqueue(x);
        }
        return found;
    }

    void reachRoadEnd(Vehicle* v) {
        const Road* r = graph.getRoad(v->currentRoad);
        if (r->to == v->destination) {
            v->state = ARRIVED;
        } else {
            v->state = WAITING;
            roadQueue[v->currentRoad]->enqueue(v->id);
        }
    }

    // Remove from every structure and free memory
    // (completedTrip = arrived; otherwise counted as diverted)
    void removeVehicle(Vehicle* v, bool completedTrip) {
        Road* r = graph.getRoad(v->currentRoad);
        if (v->state != ENTERING && r != NULL && r->vehiclesOnRoad > 0) r->vehiclesOnRoad--;

        if (completedTrip) {
            TripRecord t;
            t.vehicleId = v->id;   t.type = v->type;
            t.origin = v->origin;  t.destination = v->destination;
            t.travelTime = simTime - v->spawnTime;
            t.waitTime = v->waitTime;
            t.distance = v->distance;
            t.reroutes = v->reroutes;
            trips.pushBack(t);
            totalTravelTime += t.travelTime;
            arrived++;
        } else {
            divertedStuck++;
        }
        registry.remove(v->id);
        active.remove(v);
        delete v;
    }

    // Spawned vehicles waiting at their origin join their first road when
    // it has space. If that road was closed meanwhile, they reroute.
    void admitEntries() {
        for (int r = 0; r < queueCount; r++) {
            Queue<int>* eq = entryQueue[r];
            Road* road = graph.getRoad(r);
            int id;
            while (eq->peek(id)) {
                Vehicle* v;
                if (!registry.get(id, v)) { eq->dequeue(id); continue; }
                if (road->blocked) {
                    eq->dequeue(id);
                    SearchStats s;
                    int t = GraphAlgorithms::dijkstraRoute(graph, v->origin, v->destination, true, v->route, s);
                    int first;
                    if (t == INF || !v->route.popFront(first)) { removeVehicle(v, false); continue; }
                    v->currentRoad = first;
                    v->reroutes++;
                    entryQueue[first]->enqueue(id);
                    continue;
                }
                if (road->vehiclesOnRoad >= road->capacity()) break;   // still full
                eq->dequeue(id);
                road->vehiclesOnRoad++;
                v->state = MOVING;
                v->position = 0;
            }
        }
    }

    // New route from the END of the current road. false = no route exists.
    bool rerouteFromRoadEnd(Vehicle* v) {
        int from = graph.getRoad(v->currentRoad)->to;
        SearchStats s;
        int t = GraphAlgorithms::dijkstraRoute(graph, from, v->destination, true, v->route, s);
        if (t == INF || v->route.isEmpty()) return false;
        v->reroutes++;
        return true;
    }

public:
    VehicleManager(Graph& g, unsigned int seed = 12345)
        : graph(g), registry(64), rng(seed), nextId(1001), simTime(0),
          spawned(0), arrived(0), rejected(0), divertedStuck(0), cancelled(0), totalTravelTime(0) {
        queueCount = graph.getRoadCount();
        roadQueue = new Queue<int>*[queueCount];
        entryQueue = new Queue<int>*[queueCount];
        flowCredit = new double[queueCount];
        for (int i = 0; i < queueCount; i++) {
            roadQueue[i] = new Queue<int>();
            entryQueue[i] = new Queue<int>();
            flowCredit[i] = 0;
        }
    }

    ~VehicleManager() {
        clearVehicles();
        for (int i = 0; i < queueCount; i++) { delete roadQueue[i]; delete entryQueue[i]; }
        delete[] roadQueue;
        delete[] entryQueue;
        delete[] flowCredit;
    }

    // Delete every vehicle and empty all queues (used by "Reset" button)
    void clearVehicles() {
        Vehicle* v;
        while (active.popFront(v)) {
            Road* r = graph.getRoad(v->currentRoad);
            if (v->state != ENTERING && r != NULL && r->vehiclesOnRoad > 0) r->vehiclesOnRoad--;
            delete v;
        }
        registry.clear();
        for (int i = 0; i < queueCount; i++) {
            roadQueue[i]->clear(); entryQueue[i]->clear(); flowCredit[i] = 0;
        }
    }

    // ------------------------------------------------------------
    //  Spawning
    // ------------------------------------------------------------
    // Returns new vehicle id, or -1 if invalid / no route exists.
    int spawnVehicle(int origin, int destination, VehicleType type) {
        if (!graph.validNode(origin) || !graph.validNode(destination) || origin == destination) {
            rejected++;
            return -1;
        }
        Vehicle* v = new Vehicle(nextId, type, origin, destination, simTime);
        SearchStats s;
        int t = GraphAlgorithms::dijkstraRoute(graph, origin, destination, true, v->route, s);
        if (t == INF || v->route.isEmpty()) {
            delete v;                        // no route: free memory, reject
            rejected++;
            return -1;
        }
        int firstRoad;
        v->route.popFront(firstRoad);
        v->currentRoad = firstRoad;
        Road* fr = graph.getRoad(firstRoad);
        if (fr->vehiclesOnRoad < fr->capacity()) {
            fr->vehiclesOnRoad++;                // room: start driving
        } else {
            v->state = ENTERING;                 // full: wait at origin
            entryQueue[firstRoad]->enqueue(v->id);
        }

        registry.put(v->id, v);
        active.pushBack(v);
        spawned++;
        return nextId++;
    }

    // Random origin/destination; 70% car, 15% bus, 10% truck, 5% ambulance
    int spawnRandom() {
        int V = graph.getNodeCount();
        if (V < 2) return -1;
        int o = rng.nextInt(V);
        int d = rng.nextInt(V - 1);
        if (d >= o) d++;                     // guarantees d != o
        int p = rng.nextInt(100);
        VehicleType t = p < 70 ? CAR : p < 85 ? BUS : p < 95 ? TRUCK : AMBULANCE;
        return spawnVehicle(o, d, t);
    }

    // Traffic surge: many vehicles at once
    int spawnMany(int count) {
        int ok = 0;
        for (int i = 0; i < count; i++) if (spawnRandom() != -1) ok++;
        return ok;
    }

    // ------------------------------------------------------------
    //  Phase 1 of a tick: movement
    // ------------------------------------------------------------
    void update(double dt) {
        simTime += dt;
        admitEntries();
        Stack<Vehicle*> finished;            // can't delete while iterating the list

        for (ListNode<Vehicle*>* n = active.getHead(); n != NULL; n = n->next) {
            Vehicle* v = n->data;
            if (v->state == MOVING) {
                const Road* r = graph.getRoad(v->currentRoad);
                double step = speedOn(v, r) * dt;
                if (v->position + step >= r->length) step = r->length - v->position;
                v->position += step;
                v->distance += step;
                if (v->position >= r->length) reachRoadEnd(v);
                if (v->state == ARRIVED) finished.push(v);
            } else {
                v->waitTime += dt;
                if (v->state == STUCK) v->stuckTime += dt;
            }
        }
        Vehicle* v;
        while (finished.pop(v)) removeVehicle(v, true);
    }

    // ------------------------------------------------------------
    //  Phase 2 of a tick: let up to maxCount vehicles leave road's queue
    //  (called by traffic lights in Step 5). Returns how many crossed.
    // ------------------------------------------------------------
    int releaseFromRoad(int roadId, int maxCount) {
        if (roadId < 0 || roadId >= queueCount) return 0;
        Queue<int>* q = roadQueue[roadId];
        int released = 0, id;

        while (released < maxCount && q->peek(id)) {
            Vehicle* v;
            if (!registry.get(id, v)) { q->dequeue(id); continue; }   // safety

            // Next road missing or closed -> try a new route
            ListNode<int>* nextNode = v->route.getHead();
            if (nextNode == NULL || graph.getRoad(nextNode->data)->blocked) {
                if (!rerouteFromRoadEnd(v)) {
                    v->state = STUCK;
                    if (v->stuckTime >= STUCK_TIMEOUT) {   // give up: divert out of city
                        q->dequeue(id);
                        removeVehicle(v, false);
                        continue;
                    }
                    break;                   // blocks everyone behind it
                }
                nextNode = v->route.getHead();
            }

            Road* next = graph.getRoad(nextNode->data);
            if (next->vehiclesOnRoad >= next->capacity()) break;   // SPILLBACK

            // Cross the intersection onto the next road
            q->dequeue(id);
            graph.getRoad(v->currentRoad)->vehiclesOnRoad--;
            int nr;
            v->route.popFront(nr);
            v->currentRoad = nr;
            v->position = 0;
            v->state = MOVING;
            v->stuckTime = 0;
            next->vehiclesOnRoad++;
            released++;
        }
        return released;
    }

    // Green for dt seconds: earn lanes * SATURATION_FLOW * dt credit and
    // release one vehicle per whole credit. Called by traffic lights.
    int releaseFlow(int roadId, double dt) {
        if (roadId < 0 || roadId >= queueCount) return 0;
        if (roadQueue[roadId]->isEmpty()) { flowCredit[roadId] = 0; return 0; }
        flowCredit[roadId] += graph.getRoad(roadId)->lanes * SATURATION_FLOW * dt;
        int allowed = (int)flowCredit[roadId];
        int released = releaseFromRoad(roadId, allowed);
        flowCredit[roadId] -= released;
        if (released < allowed && flowCredit[roadId] > 1.0) flowCredit[roadId] = 1.0;  // blocked: don't bank
        return released;
    }

    // No traffic lights: every road discharges at saturation flow
    void processUncontrolled(double dt = SIM_TICK) {
        for (int r = 0; r < queueCount; r++) releaseFlow(r, dt);
    }

    // Convenience for Step 4: one full tick without signals
    void stepUncontrolled(double dt = SIM_TICK) {
        update(dt);
        processUncontrolled(dt);
    }

    // Recalculate a vehicle's route (Chaos Mode uses this in Step 6)
    bool rerouteVehicle(int id) {
        Vehicle* v;
        if (!registry.get(id, v)) return false;
        if (v->state == ENTERING) return false;      // admitEntries handles these
        return rerouteFromRoadEnd(v);
    }

    // Delete a vehicle wherever it is (used by "undo surge")
    bool cancelVehicle(int id) {
        Vehicle* v;
        if (!registry.get(id, v)) return false;
        if (v->state == ENTERING) removeFromQueue(entryQueue[v->currentRoad], id);
        else if (v->state == WAITING || v->state == STUCK) removeFromQueue(roadQueue[v->currentRoad], id);
        removeVehicle(v, false);
        divertedStuck--;                             // not a diversion: a cancellation
        cancelled++;
        return true;
    }

    int getNextId() const { return nextId; }
    int cancelledCount() const { return cancelled; }

    // ------------------------------------------------------------
    //  Queries (dashboard / GUI)
    // ------------------------------------------------------------
    Vehicle* getVehicle(int id) const {
        Vehicle* v;
        return registry.get(id, v) ? v : NULL;
    }
    ListNode<Vehicle*>* activeHead() const { return active.getHead(); }
    const LinkedList<TripRecord>& getTrips() const { return trips; }

    int activeCount() const { return active.size(); }
    int spawnedCount() const { return spawned; }
    int arrivedCount() const { return arrived; }
    int rejectedCount() const { return rejected; }
    int divertedCount() const { return divertedStuck; }
    double getSimTime() const { return simTime; }
    double averageTravelTime() const { return arrived == 0 ? 0 : totalTravelTime / arrived; }

    int queueLength(int roadId) const {
        return (roadId >= 0 && roadId < queueCount) ? roadQueue[roadId]->size() : 0;
    }
    int entryQueueLength(int roadId) const {
        return (roadId >= 0 && roadId < queueCount) ? entryQueue[roadId]->size() : 0;
    }
    int totalEntering() const {
        int t = 0;
        for (int i = 0; i < queueCount; i++) t += entryQueue[i]->size();
        return t;
    }
    int totalQueued() const {
        int t = 0;
        for (int i = 0; i < queueCount; i++) t += roadQueue[i]->size();
        return t;
    }
    // Is an ambulance waiting in this road's queue? (used for signal pre-emption)
    bool emergencyWaitingAt(int roadId) const {
        for (ListNode<Vehicle*>* n = active.getHead(); n != NULL; n = n->next) {
            const Vehicle* v = n->data;
            if (v->currentRoad == roadId && v->isEmergency() &&
                (v->state == WAITING || v->state == STUCK)) return true;
        }
        return false;
    }

    int countInState(VehicleState s) const {
        int c = 0;
        for (ListNode<Vehicle*>* n = active.getHead(); n != NULL; n = n->next)
            if (n->data->state == s) c++;
        return c;
    }
    const HashTable<int, Vehicle*>& getRegistry() const { return registry; }
};

#endif
