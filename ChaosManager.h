// =====================================================================
//  Chaos City - ChaosManager.h
//  Chaos Mode: things that go wrong in the city, and recovery.
//
//   closeRoad     - road works: no new vehicles may enter (both ways by
//                   default). Vehicles already on it drive out.
//   causeAccident - one direction blocked for a duration; vehicles ON the
//                   road stop until it clears (automatic timer).
//   surge         - many vehicles appear at once (random, to a hotspot,
//                   or from a hotspot).
//   Chaos Mode    - random events every CHAOS_INTERVAL seconds.
//
//  After every change the city reacts automatically:
//   - rerouteAffected(): every vehicle whose remaining route uses a
//     blocked road gets a new Dijkstra route straight away
//   - improveRoutes():   after a road reopens, vehicles switch to it if
//     that saves at least 10% of their remaining time
//
//  Data structures:
//   Stack<ChaosAction> undoStack / redoStack - Undo & Redo buttons
//   LinkedList<Accident>   accidents          - active accident timers
//   LinkedList<ChaosEvent> eventLog           - timeline for the dashboard
//   bool* closed / int* accidentsOn (arrays)  - why each road is blocked,
//                   so an accident clearing never reopens a closed road
// =====================================================================
#ifndef CHAOSMANAGER_H
#define CHAOSMANAGER_H

#include <string>
#include <sstream>
#include "Graph.h"
#include "GraphAlgorithms.h"
#include "VehicleManager.h"
#include "LinkedList.h"
#include "Stack.h"

const double DEFAULT_ACCIDENT_TIME = 180.0;  // seconds
const double CHAOS_INTERVAL        = 120.0;  // seconds between random events
const double IMPROVE_THRESHOLD     = 0.9;    // new route must be <= 90% of old time

enum ChaosType { CHAOS_CLOSE = 0, CHAOS_REOPEN = 1, CHAOS_ACCIDENT = 2, CHAOS_SURGE = 3 };
enum SurgeKind { SURGE_RANDOM = 0, SURGE_TO = 1, SURGE_FROM = 2 };

// One undoable user action
struct ChaosAction {
    ChaosType type;
    int roadA, roadB;          // road ids (roadB = -1 if one direction)
    double duration;           // accidents
    SurgeKind surgeKind;       // surges
    int hotspot;               // surge target/source intersection
    int count;                 // surge size
    int firstId, lastId;       // vehicle ids created by the surge
    ChaosAction() : type(CHAOS_CLOSE), roadA(-1), roadB(-1), duration(0),
        surgeKind(SURGE_RANDOM), hotspot(-1), count(0), firstId(0), lastId(-1) {}
};

struct Accident {
    int roadId;
    double clearTime;          // simulation time when it clears
    Accident() : roadId(-1), clearTime(0) {}
    Accident(int r, double t) : roadId(r), clearTime(t) {}
    bool operator==(const Accident& o) const { return roadId == o.roadId && clearTime == o.clearTime; }
};

struct ChaosEvent {
    double time;
    std::string text;
    ChaosEvent() : time(0) {}
    ChaosEvent(double t, const std::string& s) : time(t), text(s) {}
    bool operator==(const ChaosEvent& o) const { return time == o.time && text == o.text; }
};

class ChaosManager {
private:
    Graph& graph;
    VehicleManager& vm;
    bool* closed;              // closed[road] = road works
    int* accidentsOn;          // number of active accidents on each road
    int roadCount;

    LinkedList<Accident> accidents;
    Stack<ChaosAction> undoStack;
    Stack<ChaosAction> redoStack;
    LinkedList<ChaosEvent> eventLog;

    bool chaosMode;
    double chaosTimer;
    SimpleRandom rng;

    int totalRerouted, totalClosures, totalAccidents, totalSurges;

    ChaosManager(const ChaosManager&);
    ChaosManager& operator=(const ChaosManager&);

    // ---------- helpers ----------
    std::string roadName(int r) const {
        const Road* road = graph.getRoad(r);
        return graph.getIntersection(road->from).name + " -> " + graph.getIntersection(road->to).name;
    }

    void log(const std::string& text) {
        eventLog.pushBack(ChaosEvent(vm.getSimTime(), text));
    }

    // Road state is derived from WHY it is blocked
    void applyState(int r) {
        Road* road = graph.getRoad(r);
        road->incident = accidentsOn[r] > 0;
        road->blocked  = closed[r] || accidentsOn[r] > 0;
    }

    int reverseOf(int r) const {
        const Road* road = graph.getRoad(r);
        return graph.findRoad(road->to, road->from);
    }

    void recordAction(const ChaosAction& a) {
        undoStack.push(a);
        redoStack.clear();          // a new action invalidates the redo history
    }

    // ---------- the actual operations (record = push on undo stack) ----------
    bool doClose(int ra, int rb, bool record) {
        bool changed = false;
        if (ra != -1 && !closed[ra]) { closed[ra] = true; applyState(ra); changed = true; }
        if (rb != -1 && !closed[rb]) { closed[rb] = true; applyState(rb); changed = true; }
        if (!changed) return false;
        totalClosures++;
        int n = rerouteAffected();
        std::stringstream ss;
        ss << "Road CLOSED: " << roadName(ra) << (rb != -1 ? " (both ways)" : "")
           << ", " << n << " vehicles rerouted";
        if (!GraphAlgorithms::isStronglyConnected(graph)) ss << " - WARNING: city split, some trips impossible";
        log(ss.str());
        if (record) {
            ChaosAction a; a.type = CHAOS_CLOSE; a.roadA = ra; a.roadB = rb;
            recordAction(a);
        }
        return true;
    }

    bool doReopen(int ra, int rb, bool record) {
        bool changed = false;
        if (ra != -1 && closed[ra]) { closed[ra] = false; applyState(ra); changed = true; }
        if (rb != -1 && closed[rb]) { closed[rb] = false; applyState(rb); changed = true; }
        if (!changed) return false;
        int n = improveRoutes();
        std::stringstream ss;
        ss << "Road REOPENED: " << roadName(ra) << (rb != -1 ? " (both ways)" : "")
           << ", " << n << " vehicles switched back";
        log(ss.str());
        if (record) {
            ChaosAction a; a.type = CHAOS_REOPEN; a.roadA = ra; a.roadB = rb;
            recordAction(a);
        }
        return true;
    }

    bool doAccident(int r, double duration, bool record) {
        if (!graph.validRoad(r) || duration <= 0) return false;
        accidents.pushBack(Accident(r, vm.getSimTime() + duration));
        accidentsOn[r]++;
        applyState(r);
        totalAccidents++;
        int n = rerouteAffected();
        std::stringstream ss;
        ss << "ACCIDENT on " << roadName(r) << " for " << (int)duration << "s, "
           << graph.getRoad(r)->vehiclesOnRoad << " vehicles trapped, " << n << " rerouted";
        log(ss.str());
        if (record) {
            ChaosAction a; a.type = CHAOS_ACCIDENT; a.roadA = r; a.duration = duration;
            recordAction(a);
        }
        return true;
    }

    // Clear the most recent accident on road r. Returns false if none active.
    bool clearAccident(int r, const std::string& reason) {
        Accident found;
        bool ok = false;
        for (ListNode<Accident>* n = accidents.getHead(); n != NULL; n = n->next)
            if (n->data.roadId == r) { found = n->data; ok = true; }   // keep last match
        if (!ok) return false;
        accidents.remove(found);
        accidentsOn[r]--;
        applyState(r);
        int n = improveRoutes();
        std::stringstream ss;
        ss << "Accident CLEARED (" << reason << "): " << roadName(r) << ", "
           << n << " vehicles switched back";
        log(ss.str());
        return true;
    }

    // Returns how many vehicles were created; fills first/last id
    int doSurge(SurgeKind kind, int hotspot, int count, bool record) {
        int firstId = vm.getNextId();
        int made = 0;
        int V = graph.getNodeCount();
        for (int i = 0; i < count; i++) {
            int id;
            if (kind == SURGE_RANDOM) id = vm.spawnRandom();
            else {
                int other = rng.nextInt(V - 1);
                if (other >= hotspot) other++;
                VehicleType t = rng.nextInt(100) < 80 ? CAR : BUS;
                id = (kind == SURGE_TO) ? vm.spawnVehicle(other, hotspot, t)
                                        : vm.spawnVehicle(hotspot, other, t);
            }
            if (id != -1) made++;
        }
        int lastId = vm.getNextId() - 1;
        totalSurges++;
        std::stringstream ss;
        ss << "TRAFFIC SURGE: " << made << " vehicles";
        if (kind == SURGE_TO)   ss << " heading to " << graph.getIntersection(hotspot).name;
        if (kind == SURGE_FROM) ss << " leaving " << graph.getIntersection(hotspot).name;
        log(ss.str());
        if (record) {
            ChaosAction a; a.type = CHAOS_SURGE; a.surgeKind = kind; a.hotspot = hotspot;
            a.count = count; a.firstId = firstId; a.lastId = lastId;
            recordAction(a);
        }
        return made;
    }

    int undoSurge(const ChaosAction& a) {
        int removed = 0;
        for (int id = a.firstId; id <= a.lastId; id++)
            if (vm.cancelVehicle(id)) removed++;
        std::stringstream ss;
        ss << "UNDO surge: " << removed << " surge vehicles removed";
        log(ss.str());
        return removed;
    }

    // Opposite of an action (for undo) or the action again (for redo)
    void perform(const ChaosAction& a, bool reverse, ChaosAction& resulting) {
        resulting = a;
        switch (a.type) {
            case CHAOS_CLOSE:
                if (reverse) doReopen(a.roadA, a.roadB, false); else doClose(a.roadA, a.roadB, false);
                break;
            case CHAOS_REOPEN:
                if (reverse) doClose(a.roadA, a.roadB, false); else doReopen(a.roadA, a.roadB, false);
                break;
            case CHAOS_ACCIDENT:
                if (reverse) { if (!clearAccident(a.roadA, "undo")) log("UNDO accident: already cleared"); }
                else doAccident(a.roadA, a.duration, false);
                break;
            case CHAOS_SURGE:
                if (reverse) undoSurge(a);
                else {
                    resulting.firstId = vm.getNextId();
                    doSurge(a.surgeKind, a.hotspot, a.count, false);
                    resulting.lastId = vm.getNextId() - 1;
                }
                break;
        }
    }

    // Pick a random two-way road whose closure keeps the city connected
    bool randomSafeClosure() {
        for (int attempt = 0; attempt < 20; attempt++) {
            int r = rng.nextInt(roadCount);
            int rev = reverseOf(r);
            if (rev == -1 || closed[r] || closed[rev]) continue;
            if (wouldDisconnect(r, rev)) continue;
            return doClose(r, rev, true);
        }
        return false;
    }

public:
    ChaosManager(Graph& g, VehicleManager& v, unsigned int seed = 777)
        : graph(g), vm(v), chaosMode(false), chaosTimer(0), rng(seed),
          totalRerouted(0), totalClosures(0), totalAccidents(0), totalSurges(0) {
        roadCount = graph.getRoadCount();
        closed = new bool[roadCount];
        accidentsOn = new int[roadCount];
        for (int i = 0; i < roadCount; i++) { closed[i] = false; accidentsOn[i] = 0; }
    }

    ~ChaosManager() {
        delete[] closed;
        delete[] accidentsOn;
    }

    // ------------------------------------------------------------
    //  User actions (all undoable)
    // ------------------------------------------------------------
    // Close the road u -> v (and v -> u if bothWays). Intersection ids.
    bool closeRoad(int u, int v, bool bothWays = true) {
        int ra = graph.findRoad(u, v);
        if (ra == -1) return false;
        int rb = bothWays ? graph.findRoad(v, u) : -1;
        return doClose(ra, rb, true);
    }

    bool closeRoadByName(const std::string& a, const std::string& b, bool bothWays = true) {
        return closeRoad(graph.findIntersection(a), graph.findIntersection(b), bothWays);
    }

    bool reopenRoad(int u, int v, bool bothWays = true) {
        int ra = graph.findRoad(u, v);
        if (ra == -1) return false;
        int rb = bothWays ? graph.findRoad(v, u) : -1;
        return doReopen(ra, rb, true);
    }

    bool causeAccident(int roadId, double duration = DEFAULT_ACCIDENT_TIME) {
        return doAccident(roadId, duration, true);
    }

    int surge(int count)                   { return doSurge(SURGE_RANDOM, -1, count, true); }
    int surgeTo(int hotspot, int count)    { return graph.validNode(hotspot) ? doSurge(SURGE_TO, hotspot, count, true) : 0; }
    int surgeFrom(int hotspot, int count)  { return graph.validNode(hotspot) ? doSurge(SURGE_FROM, hotspot, count, true) : 0; }

    // ------------------------------------------------------------
    //  Undo / Redo (two stacks)
    // ------------------------------------------------------------
    bool undo() {
        ChaosAction a, result;
        if (!undoStack.pop(a)) return false;
        perform(a, true, result);
        redoStack.push(result);
        return true;
    }

    bool redo() {
        ChaosAction a, result;
        if (!redoStack.pop(a)) return false;
        perform(a, false, result);
        undoStack.push(result);
        return true;
    }

    bool canUndo() const { return !undoStack.isEmpty(); }
    bool canRedo() const { return !redoStack.isEmpty(); }
    int undoCount() const { return undoStack.size(); }

    // ------------------------------------------------------------
    //  Automatic reactions
    // ------------------------------------------------------------
    // Reroute every vehicle whose REMAINING route uses a blocked road
    int rerouteAffected() {
        int count = 0;
        for (ListNode<Vehicle*>* n = vm.activeHead(); n != NULL; n = n->next) {
            Vehicle* v = n->data;
            if (v->state == ENTERING) continue;          // handled when it enters
            bool affected = false;
            for (ListNode<int>* r = v->route.getHead(); r != NULL; r = r->next)
                if (graph.getRoad(r->data)->blocked) { affected = true; break; }
            if (affected && vm.rerouteVehicle(v->id)) count++;
        }
        totalRerouted += count;
        return count;
    }

    // Switch vehicles to a better route if it saves >= 10% of remaining time
    int improveRoutes() {
        int count = 0;
        for (ListNode<Vehicle*>* n = vm.activeHead(); n != NULL; n = n->next) {
            Vehicle* v = n->data;
            if (v->state == ENTERING || v->route.isEmpty()) continue;
            int from = graph.getRoad(v->currentRoad)->to;
            int oldTime = GraphAlgorithms::routeTime(graph, v->route, true);
            LinkedList<int> better;
            SearchStats s;
            int newTime = GraphAlgorithms::dijkstraRoute(graph, from, v->destination, true, better, s);
            if (newTime == INF || better.isEmpty()) continue;
            if (oldTime == INF || newTime <= oldTime * IMPROVE_THRESHOLD) {
                v->route.clear();
                for (ListNode<int>* r = better.getHead(); r != NULL; r = r->next)
                    v->route.pushBack(r->data);
                v->reroutes++;
                count++;
            }
        }
        totalRerouted += count;
        return count;
    }

    // Would closing these roads split the city? (state restored afterwards)
    bool wouldDisconnect(int ra, int rb) {
        Road* a = graph.getRoad(ra);
        Road* b = rb != -1 ? graph.getRoad(rb) : NULL;
        bool oldA = a->blocked, oldB = b ? b->blocked : false;
        a->blocked = true;
        if (b) b->blocked = true;
        bool split = !GraphAlgorithms::isStronglyConnected(graph);
        a->blocked = oldA;
        if (b) b->blocked = oldB;
        return split;
    }

    // ------------------------------------------------------------
    //  Called once per tick: accident timers + random Chaos Mode
    // ------------------------------------------------------------
    void update(double dt) {
        double now = vm.getSimTime();
        Stack<int> expired;
        for (ListNode<Accident>* n = accidents.getHead(); n != NULL; n = n->next)
            if (n->data.clearTime <= now) expired.push(n->data.roadId);
        int r;
        while (expired.pop(r)) clearAccident(r, "timer");

        if (!chaosMode) return;
        chaosTimer += dt;
        if (chaosTimer < CHAOS_INTERVAL) return;
        chaosTimer = 0;
        int pick = rng.nextInt(100);
        if (pick < 50) {
            causeAccident(rng.nextInt(roadCount), 120 + rng.nextInt(181));   // 2-5 minutes
        } else if (pick < 80) {
            surge(15 + rng.nextInt(16));                                     // 15-30 vehicles
        } else {
            randomSafeClosure();
        }
    }

    void setChaosMode(bool on) {
        chaosMode = on;
        chaosTimer = 0;
        log(on ? "CHAOS MODE ON" : "CHAOS MODE OFF");
    }
    bool isChaosMode() const { return chaosMode; }

    // ------------------------------------------------------------
    //  Queries
    // ------------------------------------------------------------
    bool isClosed(int r) const { return r >= 0 && r < roadCount && closed[r]; }
    bool hasAccident(int r) const { return r >= 0 && r < roadCount && accidentsOn[r] > 0; }
    int activeAccidents() const { return accidents.size(); }
    int closedRoadCount() const {
        int c = 0;
        for (int i = 0; i < roadCount; i++) if (closed[i]) c++;
        return c;
    }
    ListNode<ChaosEvent>* eventLogHead() const { return eventLog.getHead(); }
    int eventCount() const { return eventLog.size(); }
    int getTotalRerouted() const { return totalRerouted; }
    int getTotalAccidents() const { return totalAccidents; }
    int getTotalClosures() const { return totalClosures; }
    int getTotalSurges() const { return totalSurges; }
};

#endif
