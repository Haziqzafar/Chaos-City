// =====================================================================
//  Chaos City - SignalController.h
//  Traffic lights at every intersection.
//
//  Each intersection's light controls its INCOMING roads ("approaches").
//  Exactly one approach is green at a time; between greens there is a
//  YELLOW clearance period where nobody crosses.
//
//  Two strategies (switchable at run time, compared on the dashboard):
//
//   FIXED    - classic timer. Approaches take turns in a fixed order,
//              each green for FIXED_GREEN seconds. The order is kept in a
//              CircularQueue (array based): dequeue the front, enqueue it
//              at the back -> next phase.
//
//   ADAPTIVE - queue-length based. Green lasts at least MIN_GREEN and is
//              ended early when its queue empties, when no vehicle has
//              crossed for GAP_OUT seconds (the road ahead is full, so the
//              green is wasted), or at MAX_GREEN.
//              The next green goes to the approach with highest priority,
//              chosen with a MAX-HEAP:
//                 priority = queue length + red time / STARVATION_SECONDS
//              (red time stops a quiet road from waiting forever).
//              An ambulance waiting on a red approach pre-empts the light.
//
//  Users can also force a road green manually (forceGreen).
// =====================================================================
#ifndef SIGNALCONTROLLER_H
#define SIGNALCONTROLLER_H

#include "Graph.h"
#include "VehicleManager.h"
#include "Queue.h"
#include "Heap.h"

const double FIXED_GREEN        = 20.0;  // seconds
const double YELLOW_TIME        = 3.0;
const double MIN_GREEN          = 6.0;
const double MAX_GREEN          = 40.0;
const double GAP_OUT            = 5.0;   // no vehicle crossed for 5 s -> end green
                                         // (1 lane discharges a car every 4 s)
const double STARVATION_SECONDS = 10.0;  // each 10 s of red = +1 car of priority
const double EMERGENCY_BONUS    = 1000.0;

enum SignalStrategy { FIXED_TIMER = 0, ADAPTIVE = 1 };
enum LightPhase     { PHASE_GREEN = 0, PHASE_YELLOW = 1 };
enum LightColor     { LIGHT_RED = 0, LIGHT_YELLOW = 1, LIGHT_GREEN = 2, LIGHT_NONE = 3 };

// Max-heap entry for choosing the next green approach
struct ApproachPriority {
    double priority;
    int roadId;
    ApproachPriority() : priority(0), roadId(-1) {}
    ApproachPriority(double p, int r) : priority(p), roadId(r) {}
    bool operator<(const ApproachPriority& o) const { return priority < o.priority; }
};

class TrafficLight {
public:
    int intersection;
    int* approaches;           // incoming road ids
    double* redTime;           // seconds each approach has been red
    int count;
    CircularQueue<int>* ring;  // fixed-timer phase order

    LightPhase phase;
    int currentGreen;          // road id that is green (or was, during yellow)
    int pendingGreen;          // road id that becomes green after yellow
    double timer;              // seconds in the current phase
    double sinceRelease;       // seconds since a vehicle last crossed on green
    int switches;

    TrafficLight(int node, const Graph& g) : intersection(node), approaches(NULL),
        redTime(NULL), count(0), ring(NULL), phase(PHASE_GREEN),
        currentGreen(-1), pendingGreen(-1), timer(0), sinceRelease(0), switches(0) {
        for (int r = 0; r < g.getRoadCount(); r++)
            if (g.getRoad(r)->to == node) count++;
        if (count == 0) return;
        approaches = new int[count];
        redTime = new double[count];
        ring = new CircularQueue<int>(count);
        int k = 0;
        for (int r = 0; r < g.getRoadCount(); r++)
            if (g.getRoad(r)->to == node) {
                approaches[k] = r;
                redTime[k] = 0;
                ring->enqueue(r);
                k++;
            }
        ring->peek(currentGreen);
    }

    ~TrafficLight() {
        delete[] approaches;
        delete[] redTime;
        delete ring;
    }

    // Linear search: position of a road in the approaches array
    int indexOf(int roadId) const {
        for (int i = 0; i < count; i++) if (approaches[i] == roadId) return i;
        return -1;
    }

    // Rotate the circular queue until 'roadId' is at the front
    void alignRing(int roadId) {
        int front;
        for (int i = 0; i < count; i++) {
            ring->peek(front);
            if (front == roadId) return;
            ring->dequeue(front);
            ring->enqueue(front);
        }
    }

    // Next road in fixed order
    int nextInRing() {
        int front;
        ring->dequeue(front);
        ring->enqueue(front);
        ring->peek(front);
        return front;
    }

    void startYellow(int nextRoad) {
        pendingGreen = nextRoad;
        phase = PHASE_YELLOW;
        timer = 0;
    }

private:
    TrafficLight(const TrafficLight&);
    TrafficLight& operator=(const TrafficLight&);
};

class SignalController {
private:
    Graph& graph;
    TrafficLight** lights;      // one per intersection
    int lightCount;
    SignalStrategy strategy;
    int totalSwitches;
    int emergencyPreemptions;

    SignalController(const SignalController&);
    SignalController& operator=(const SignalController&);

    // ADAPTIVE: pick the best red approach with the max-heap. -1 = none waiting.
    int chooseAdaptive(TrafficLight* L, VehicleManager& vm, bool emergencyOnly) {
        Heap<ApproachPriority> heap(false, L->count);    // MAX-heap
        for (int i = 0; i < L->count; i++) {
            int r = L->approaches[i];
            if (r == L->currentGreen) continue;
            int q = vm.queueLength(r);
            if (q == 0) continue;
            bool emergency = vm.emergencyWaitingAt(r);
            if (emergencyOnly && !emergency) continue;
            double p = q + L->redTime[i] / STARVATION_SECONDS;
            if (emergency) p += EMERGENCY_BONUS;
            heap.push(ApproachPriority(p, r));
        }
        ApproachPriority best;
        return heap.pop(best) ? best.roadId : -1;
    }

    void updateLight(TrafficLight* L, double dt, VehicleManager& vm) {
        if (L->count == 0) return;
        if (L->count == 1) {                       // nothing to conflict with
            vm.releaseFlow(L->approaches[0], dt);
            return;
        }

        L->timer += dt;
        for (int i = 0; i < L->count; i++) {
            bool green = (L->phase == PHASE_GREEN && L->approaches[i] == L->currentGreen);
            L->redTime[i] = green ? 0 : L->redTime[i] + dt;
        }

        if (L->phase == PHASE_YELLOW) {
            if (L->timer >= YELLOW_TIME) {
                L->currentGreen = L->pendingGreen;
                L->phase = PHASE_GREEN;
                L->timer = 0;
                L->sinceRelease = 0;
                L->redTime[L->indexOf(L->currentGreen)] = 0;
                L->switches++;
                totalSwitches++;
            }
            return;                                // nobody crosses on yellow
        }

        // GREEN: vehicles on the green approach cross at saturation flow
        if (vm.releaseFlow(L->currentGreen, dt) > 0) L->sinceRelease = 0;
        else L->sinceRelease += dt;

        if (strategy == FIXED_TIMER) {
            if (L->timer >= FIXED_GREEN) {
                L->alignRing(L->currentGreen);
                L->startYellow(L->nextInRing());
            }
            return;
        }

        // ADAPTIVE
        if (L->timer < MIN_GREEN) return;
        if (!vm.emergencyWaitingAt(L->currentGreen)) {
            int e = chooseAdaptive(L, vm, true);   // ambulance on a red road?
            if (e != -1) { L->startYellow(e); emergencyPreemptions++; return; }
        }
        bool wasted = vm.queueLength(L->currentGreen) > 0 && L->sinceRelease >= GAP_OUT;
        if (vm.queueLength(L->currentGreen) == 0 || wasted || L->timer >= MAX_GREEN) {
            int next = chooseAdaptive(L, vm, false);
            if (next != -1) L->startYellow(next);  // else: nobody waiting, stay green
        }
    }

public:
    SignalController(Graph& g, SignalStrategy s = ADAPTIVE)
        : graph(g), strategy(s), totalSwitches(0), emergencyPreemptions(0) {
        lightCount = graph.getNodeCount();
        lights = new TrafficLight*[lightCount];
        for (int v = 0; v < lightCount; v++) lights[v] = new TrafficLight(v, graph);
    }

    ~SignalController() {
        for (int v = 0; v < lightCount; v++) delete lights[v];
        delete[] lights;
    }

    // Phase 2 of a tick (after VehicleManager::update)
    void update(double dt, VehicleManager& vm) {
        for (int v = 0; v < lightCount; v++) updateLight(lights[v], dt, vm);
    }

    void setStrategy(SignalStrategy s) { strategy = s; }
    SignalStrategy getStrategy() const { return strategy; }
    static const char* strategyName(SignalStrategy s) { return s == FIXED_TIMER ? "Fixed Timer" : "Adaptive"; }

    // Manual control from the GUI: make roadId green (after a yellow)
    bool forceGreen(int roadId) {
        const Road* r = graph.getRoad(roadId);
        if (r == NULL) return false;
        TrafficLight* L = lights[r->to];
        if (L->indexOf(roadId) == -1) return false;
        if (L->phase == PHASE_GREEN && L->currentGreen == roadId) return true;
        L->startYellow(roadId);
        return true;
    }

    // Colour of the light at the END of a road (for drawing)
    LightColor colorOf(int roadId) const {
        const Road* r = graph.getRoad(roadId);
        if (r == NULL) return LIGHT_NONE;
        const TrafficLight* L = lights[r->to];
        if (L->count <= 1) return LIGHT_GREEN;
        if (L->currentGreen != roadId) return LIGHT_RED;
        return L->phase == PHASE_GREEN ? LIGHT_GREEN : LIGHT_YELLOW;
    }

    // Seconds left in the current phase (for the GUI countdown)
    double timeLeft(int intersection) const {
        const TrafficLight* L = lights[intersection];
        if (L->phase == PHASE_YELLOW) return YELLOW_TIME - L->timer;
        if (strategy == FIXED_TIMER) return FIXED_GREEN - L->timer;
        return MAX_GREEN - L->timer;          // upper bound in adaptive mode
    }

    const TrafficLight* getLight(int intersection) const { return lights[intersection]; }
    int getTotalSwitches() const { return totalSwitches; }
    int getEmergencyPreemptions() const { return emergencyPreemptions; }
};

#endif
