// =====================================================================
//  Chaos City - Step 5 test driver: traffic lights
//  Keep city_map.txt in the same folder.
// =====================================================================
#include <iostream>
#include <string>
#include "Graph.h"
#include "VehicleManager.h"
#include "SignalController.h"

using namespace std;

int passed = 0, failed = 0;

void check(bool condition, const string& testName) {
    if (condition) { passed++; cout << "  [PASS] " << testName << "\n"; }
    else           { failed++; cout << "  [FAIL] " << testName << "\n"; }
}

void tick(VehicleManager& vm, SignalController& sc) {
    vm.update(SIM_TICK);
    sc.update(SIM_TICK, vm);
}

int greensAt(const Graph& g, const SignalController& sc, int node) {
    int greens = 0;
    for (int r = 0; r < g.getRoadCount(); r++)
        if (g.getRoad(r)->to == node && sc.colorOf(r) != LIGHT_RED) greens++;
    return greens;
}

void testFixedTimer(Graph& city) {
    cout << "\n== Fixed timer ==\n";
    VehicleManager vm(city);
    SignalController sc(city, FIXED_TIMER);
    int saddar = city.findIntersection("Saddar");
    const TrafficLight* L = sc.getLight(saddar);
    check(L->count == 4, "Saddar light controls 4 approaches");

    int first = L->currentGreen;
    bool alwaysOne = true;
    int seenGreens[4] = { -1, -1, -1, -1 }, seen = 0;
    for (int t = 0; t < 4 * 23 + 1; t++) {        // one full cycle
        tick(vm, sc);
        for (int v = 0; v < city.getNodeCount(); v++)
            if (sc.getLight(v)->count > 1 && greensAt(city, sc, v) != 1) alwaysOne = false;
        if (L->phase == PHASE_GREEN && (seen == 0 || seenGreens[seen - 1] != L->currentGreen) && seen < 4)
            seenGreens[seen++] = L->currentGreen;
    }
    check(alwaysOne, "exactly one approach not-red at every multi-road intersection");
    check(seen == 4, "all 4 Saddar approaches got a green in one cycle");
    check(L->currentGreen == first, "after 4 x (20+3)s the cycle returns to the first road");
}

// Small T-junction used by several tests: N, S, E all feed C, C feeds X
struct Junction {
    Graph g;
    int n, s, e, exitRoad;
    Junction(int nLen, int nLanes, int sLen, int sLanes, int eLen, int eLanes) {
        g.addIntersection("N", 0, 0); g.addIntersection("S", 0, 2);
        g.addIntersection("E", 1, 1); g.addIntersection("C", 0, 1);
        g.addIntersection("X", -1, 1);
        n = g.addRoad(0, 3, nLen, nLanes, 36);      // 36 km/h = 10 m/s
        s = g.addRoad(1, 3, sLen, sLanes, 36);
        e = g.addRoad(2, 3, eLen, eLanes, 36);
        exitRoad = g.addRoad(3, 4, 5000, 4, 36);
    }
};

void testRedHoldsQueue() {
    cout << "\n== Red light holds vehicles ==\n";
    Junction j(500, 2, 100, 1, 500, 2);
    VehicleManager vm(j.g);
    SignalController sc(j.g, FIXED_TIMER);        // N is green first (0-20 s)
    int id = vm.spawnVehicle(1, 4, CAR);          // S->C is 100 m: reaches light at ~10 s
    for (int t = 0; t < 12; t++) tick(vm, sc);
    check(sc.colorOf(j.s) == LIGHT_RED, "S approach is red while N is green");
    check(vm.getVehicle(id)->state == WAITING && vm.queueLength(j.s) == 1, "car waits in S queue at red");
    int t = 12;
    while (sc.colorOf(j.s) != LIGHT_GREEN) { tick(vm, sc); t++; }
    cout << "  S turned green at t = " << t << "s (20 green + 3 yellow)\n";
    check(t == 23, "fixed timer: S green after 20 s green + 3 s yellow");
    for (int k = 0; k < 6 && vm.getVehicle(id)->currentRoad != j.exitRoad; k++) tick(vm, sc);
    check(vm.getVehicle(id)->currentRoad == j.exitRoad, "car crosses once green");
}

double runJunction(SignalStrategy strat, double& maxQuietWait) {
    Junction j(1000, 3, 1000, 3, 1000, 3);
    VehicleManager vm(j.g);
    SignalController sc(j.g, strat);
    for (int t = 0; t < 2000; t++) {
        if (t < 1200 && t % 8 == 0)  vm.spawnVehicle(0, 4, CAR);   // N busy: car every 8 s
        if (t < 1200 && t % 60 == 0) vm.spawnVehicle(1, 4, CAR);   // S quiet: car every 60 s
        if (t < 1200 && t % 30 == 0) vm.spawnVehicle(2, 4, CAR);   // E medium
        tick(vm, sc);
    }
    double total = 0;
    maxQuietWait = 0;
    for (ListNode<TripRecord>* n = vm.getTrips().getHead(); n; n = n->next) {
        total += n->data.waitTime;
        if (n->data.origin == 1 && n->data.waitTime > maxQuietWait) maxQuietWait = n->data.waitTime;
    }
    return vm.arrivedCount() ? total / vm.arrivedCount() : 0;
}

void testAdaptive() {
    cout << "\n== Adaptive vs fixed at one junction (busy N, medium E, quiet S) ==\n";
    double fixedMaxQuiet, adaptMaxQuiet;
    double fixedWait = runJunction(FIXED_TIMER, fixedMaxQuiet);
    double adaptWait = runJunction(ADAPTIVE, adaptMaxQuiet);
    cout << "  avg wait: fixed " << fixedWait << "s, adaptive " << adaptWait << "s\n";
    cout << "  longest wait of a quiet-road car: fixed " << fixedMaxQuiet
         << "s, adaptive " << adaptMaxQuiet << "s\n";
    check(adaptWait < fixedWait, "adaptive lowers average wait");
    check(adaptMaxQuiet <= MAX_GREEN + YELLOW_TIME + 5, "quiet road never starved (served within one max green)");
}

// E has 4 cars, S has 1 vehicle. Who gets green after N?
int whoGetsGreenFirst(VehicleType sType, int& preemptions, double& sWait) {
    Junction j(1000, 3, 1000, 4, 1000, 4);     // S and E identical: both arrive at t = 101 s
    VehicleManager vm(j.g);
    SignalController sc(j.g, ADAPTIVE);           // N green first
    for (int i = 0; i < 4; i++) vm.spawnVehicle(2, 4, CAR);   // E queue of 4 at ~101 s
    int sid = vm.spawnVehicle(1, 4, sType);                   // S single vehicle at ~101 s
    int first = -1;
    for (int t = 0; t < 300; t++) {
        tick(vm, sc);
        if (first == -1 && sc.colorOf(j.s) == LIGHT_GREEN) first = j.s;
        if (first == -1 && sc.colorOf(j.e) == LIGHT_GREEN) first = j.e;
        Vehicle* v = vm.getVehicle(sid);
        if (v != NULL) sWait = v->waitTime;
    }
    preemptions = sc.getEmergencyPreemptions();
    return first == j.s ? 1 : first == j.e ? 2 : 0;
}

void testEmergency() {
    cout << "\n== Ambulance pre-emption ==\n";
    int pre; double wait;
    int carCase = whoGetsGreenFirst(CAR, pre, wait);
    cout << "  S has a car       -> first green goes to " << (carCase == 1 ? "S" : "E")
         << " (E has the longer queue)\n";
    check(carCase == 2 && pre == 0, "normal case: max-heap picks longer queue E");
    int ambCase = whoGetsGreenFirst(AMBULANCE, pre, wait);
    cout << "  S has an ambulance -> first green goes to " << (ambCase == 1 ? "S" : "E")
         << ", ambulance waited " << wait << "s, pre-emptions " << pre << "\n";
    check(ambCase == 1 && pre == 1, "ambulance pre-empts: S served first");
    check(wait <= MIN_GREEN + YELLOW_TIME + 4, "ambulance waits only a few seconds");
}

struct RunResult { double avgTrip; double avgWait; int arrived; int ticks; };

RunResult runScenario(Graph& city, SignalStrategy strat, bool useSignals) {
    VehicleManager vm(city, 99);
    SignalController sc(city, strat);
    int t = 0;
    while (t < 20000) {
        if (t < 1200 && t % 10 == 0) vm.spawnMany(3);   // rush hour: 3 cars / 10 s for 20 min
        vm.update(SIM_TICK);
        if (useSignals) sc.update(SIM_TICK, vm); else vm.processUncontrolled();
        t++;
        if (t > 1200 && vm.activeCount() == 0) break;
    }
    double wait = 0;
    for (ListNode<TripRecord>* n = vm.getTrips().getHead(); n; n = n->next) wait += n->data.waitTime;
    RunResult r;
    r.arrived = vm.arrivedCount();
    r.avgTrip = vm.averageTravelTime();
    r.avgWait = r.arrived ? wait / r.arrived : 0;
    r.ticks = t;
    return r;
}

void testComparison(Graph& city) {
    cout << "\n== Strategy comparison (same 360 vehicles) ==\n";
    RunResult none  = runScenario(city, FIXED_TIMER, false);
    RunResult fixed = runScenario(city, FIXED_TIMER, true);
    RunResult adapt = runScenario(city, ADAPTIVE, true);
    cout << "  No signals : avg trip " << (int)none.avgTrip  << "s, avg wait " << (int)none.avgWait  << "s, arrived " << none.arrived  << "\n";
    cout << "  Fixed timer: avg trip " << (int)fixed.avgTrip << "s, avg wait " << (int)fixed.avgWait << "s, arrived " << fixed.arrived << "\n";
    cout << "  Adaptive   : avg trip " << (int)adapt.avgTrip << "s, avg wait " << (int)adapt.avgWait << "s, arrived " << adapt.arrived << "\n";
    check(fixed.arrived == 360 && adapt.arrived == 360, "no deadlock: all 360 arrive under both strategies");
    check(adapt.avgWait < fixed.avgWait, "adaptive signals reduce average waiting time");
    check(adapt.avgTrip < fixed.avgTrip, "adaptive signals reduce average trip time");
}

int main() {
    cout << "===== CHAOS CITY : STEP 5 TRAFFIC LIGHT TESTS =====\n";
    Graph city;
    string err;
    if (!city.loadFromFile("city_map.txt", err)) {
        cout << "Cannot load map: " << err << "\n";
        cin.get();
        return 1;
    }
    testFixedTimer(city);
    testRedHoldsQueue();
    testAdaptive();
    testEmergency();
    testComparison(city);
    cout << "\nResult: " << passed << " passed, " << failed << " failed\n";
    cout << "\nPress Enter to exit...";
    cin.get();
    return 0;
}
