// =====================================================================
//  Chaos City - Step 6 test driver: Chaos Mode
//  Keep city_map.txt in the same folder.
// =====================================================================
#include <iostream>
#include <string>
#include "Graph.h"
#include "VehicleManager.h"
#include "SignalController.h"
#include "ChaosManager.h"

using namespace std;

int passed = 0, failed = 0;

void check(bool condition, const string& testName) {
    if (condition) { passed++; cout << "  [PASS] " << testName << "\n"; }
    else           { failed++; cout << "  [FAIL] " << testName << "\n"; }
}

void tick(VehicleManager& vm, SignalController& sc, ChaosManager& cm) {
    vm.update(SIM_TICK);
    sc.update(SIM_TICK, vm);
    cm.update(SIM_TICK);
}

void resetCity(Graph& g) {
    for (int r = 0; r < g.getRoadCount(); r++) {
        g.getRoad(r)->blocked = false;
        g.getRoad(r)->incident = false;
    }
}

// How many vehicles have road r in their REMAINING route
int usingRoad(VehicleManager& vm, int r) {
    int c = 0;
    for (ListNode<Vehicle*>* n = vm.activeHead(); n; n = n->next)
        if (n->data->state != ENTERING && n->data->route.contains(r)) c++;
    return c;
}

// Queue sizes must always match vehicle states
bool queuesConsistent(VehicleManager& vm) {
    int waiting = vm.countInState(WAITING) + vm.countInState(STUCK);
    return vm.totalQueued() == waiting && vm.totalEntering() == vm.countInState(ENTERING);
}

void printLog(ChaosManager& cm, int last) {
    int skip = cm.eventCount() - last, i = 0;
    for (ListNode<ChaosEvent>* n = cm.eventLogHead(); n; n = n->next, i++)
        if (i >= skip) cout << "    t=" << (int)n->data.time << "s  " << n->data.text << "\n";
}

void testClosure(Graph& city) {
    cout << "\n== Road closure + automatic rerouting ==\n";
    VehicleManager vm(city, 11);
    SignalController sc(city, ADAPTIVE);
    ChaosManager cm(city, vm);
    vm.spawnMany(60);
    for (int t = 0; t < 30; t++) tick(vm, sc, cm);

    int saddar = city.findIntersection("Saddar"), karsaz = city.findIntersection("Karsaz");
    int sk = city.findRoad(saddar, karsaz), ks = city.findRoad(karsaz, saddar);
    int before = usingRoad(vm, sk) + usingRoad(vm, ks);
    check(cm.closeRoad(saddar, karsaz), "close Saddar <-> Karsaz");
    printLog(cm, 1);
    check(city.getRoad(sk)->blocked && city.getRoad(ks)->blocked, "both directions blocked");
    check(usingRoad(vm, sk) + usingRoad(vm, ks) == 0, "no vehicle still plans to use it");
    cout << "  vehicles that planned to use it: " << before << ", rerouted: " << cm.getTotalRerouted() << "\n";
    check(cm.getTotalRerouted() >= before, "every affected vehicle rerouted");
    check(!cm.closeRoad(saddar, karsaz), "closing again does nothing");

    // Vehicles already ON a closed road drive out (closure != accident)
    bool movedOut = true;
    for (ListNode<Vehicle*>* n = vm.activeHead(); n; n = n->next)
        if (n->data->currentRoad == sk && n->data->state == MOVING) {
            double p = n->data->position;
            tick(vm, sc, cm);
            Vehicle* v = vm.getVehicle(n->data->id);
            if (v && v->currentRoad == sk && v->position <= p) movedOut = false;
            break;
        }
    check(movedOut, "vehicle already on closed road keeps driving");
    check(queuesConsistent(vm), "queues consistent");
    resetCity(city);
}

void testUndoRedo(Graph& city) {
    cout << "\n== Undo / Redo stacks ==\n";
    VehicleManager vm(city, 5);
    ChaosManager cm(city, vm);
    int saddar = city.findIntersection("Saddar"), karsaz = city.findIntersection("Karsaz");
    int sk = city.findRoad(saddar, karsaz);

    cm.closeRoad(saddar, karsaz);
    check(cm.undo() && !city.getRoad(sk)->blocked, "undo closure -> road open");
    check(cm.redo() && city.getRoad(sk)->blocked, "redo -> closed again");
    check(cm.undo() && !city.getRoad(sk)->blocked, "undo again -> open");
    cm.causeAccident(city.findRoad(karsaz, saddar), 100);
    check(!cm.canRedo(), "new action clears redo stack");
    check(cm.undoCount() == 1, "only the accident remains undoable");
    check(cm.undo() && !city.getRoad(city.findRoad(karsaz, saddar))->blocked, "undo accident clears it");
    check(!cm.undo(), "undo on empty stack returns false");
    resetCity(city);
}

void testAccident(Graph& city) {
    cout << "\n== Accident: vehicles trapped, timer clears it ==\n";
    VehicleManager vm(city, 3);
    SignalController sc(city, ADAPTIVE);
    ChaosManager cm(city, vm);
    int tower = city.findIntersection("Tower"), malir = city.findIntersection("Malir");
    int id = vm.spawnVehicle(tower, malir, CAR);              // on Tower -> Saddar
    for (int t = 0; t < 60; t++) tick(vm, sc, cm);
    Vehicle* v = vm.getVehicle(id);
    int road = v->currentRoad;
    double pos = v->position;
    cm.causeAccident(road, 50);
    printLog(cm, 1);
    for (int t = 0; t < 30; t++) tick(vm, sc, cm);
    check(v->position == pos, "vehicle on accident road does not move");
    check(cm.activeAccidents() == 1 && city.getRoad(road)->incident, "accident active");
    for (int t = 0; t < 25; t++) tick(vm, sc, cm);
    check(cm.activeAccidents() == 0 && !city.getRoad(road)->blocked, "timer cleared accident after 50 s");
    check(v->position > pos, "vehicle drives again");
    printLog(cm, 1);
    while (vm.getVehicle(id) != NULL) tick(vm, sc, cm);
    check(vm.arrivedCount() == 1, "vehicle still reaches Malir");

    // Accident + closure on the same road: clearing the accident must NOT reopen it
    int r = city.findRoad(tower, city.findIntersection("Lyari"));
    cm.closeRoad(tower, city.findIntersection("Lyari"), false);
    cm.causeAccident(r, 5);
    for (int t = 0; t < 10; t++) tick(vm, sc, cm);
    check(city.getRoad(r)->blocked && !city.getRoad(r)->incident, "accident cleared but road stays closed");
    resetCity(city);
}

void testSurgeUndo(Graph& city) {
    cout << "\n== Surge + undo removes surge vehicles ==\n";
    VehicleManager vm(city, 21);
    SignalController sc(city, ADAPTIVE);
    ChaosManager cm(city, vm);
    vm.spawnMany(20);
    int tower = city.findIntersection("Tower");
    int made = cm.surgeFrom(tower, 80);     // everyone leaves Tower at once
    printLog(cm, 1);
    check(made == 80 && vm.activeCount() == 100, "80 surge vehicles added");
    int peakQ = 0, peakE = 0, t = 0;
    for (; t < 400; t++) {                 // undo while cars are still queued
        tick(vm, sc, cm);
        if (vm.totalQueued() > peakQ) peakQ = vm.totalQueued();
        if (vm.totalEntering() > peakE) peakE = vm.totalEntering();
        if (t > 60 && vm.totalQueued() >= 3 && vm.totalEntering() >= 3) break;
    }
    int enteringBefore = vm.totalEntering();
    cout << "  peak queued " << peakQ << ", peak waiting at origin " << peakE << "\n";
    check(enteringBefore >= 3 && vm.totalQueued() >= 3, "undo happens while queues are non-empty");
    cout << "  after " << t << " s: " << vm.activeCount() << " active, " << vm.totalQueued()
         << " queued, " << enteringBefore << " waiting at origin\n";
    check(queuesConsistent(vm), "queues consistent during surge");
    int arrivedBefore = vm.arrivedCount();
    cm.undo();
    printLog(cm, 1);
    int remainingSurge = 0;
    for (ListNode<Vehicle*>* n = vm.activeHead(); n; n = n->next)
        if (n->data->origin == tower && n->data->id > 1020) remainingSurge++;
    check(remainingSurge == 0, "no surge vehicles left after undo");
    check(vm.arrivedCount() == arrivedBefore && vm.cancelledCount() > 0, "cancelled, not counted as arrivals");
    check(queuesConsistent(vm), "queues consistent after removing from the middle");
    while (vm.activeCount() > 0) tick(vm, sc, cm);
    bool clean = true;
    for (int r = 0; r < city.getRoadCount(); r++) if (city.getRoad(r)->vehiclesOnRoad != 0) clean = false;
    check(clean, "all road counters back to 0");
}

void testConnectivityGuard(Graph& city) {
    cout << "\n== Connectivity guard ==\n";
    VehicleManager vm(city);
    ChaosManager cm(city, vm);
    int tower = city.findIntersection("Tower");
    int lyari = city.findIntersection("Lyari"), saddar = city.findIntersection("Saddar");
    check(!cm.wouldDisconnect(city.findRoad(tower, lyari), city.findRoad(lyari, tower)),
          "closing Tower-Lyari keeps city connected");
    cm.closeRoad(tower, lyari);
    check(cm.wouldDisconnect(city.findRoad(tower, saddar), city.findRoad(saddar, tower)),
          "then closing Tower-Saddar would cut Tower off");
    cm.closeRoad(tower, saddar);
    printLog(cm, 1);
    check(vm.spawnVehicle(lyari, tower, CAR) == -1, "trips into Tower now impossible");
    resetCity(city);
}

void testChaosMode(Graph& city) {
    cout << "\n== Chaos Mode: 1 hour of random events ==\n";
    VehicleManager vm(city, 2026);
    SignalController sc(city, ADAPTIVE);
    ChaosManager cm(city, vm, 99);
    cm.setChaosMode(true);
    bool consistent = true;
    for (int t = 0; t < 3600; t++) {
        if (t % 15 == 0) vm.spawnMany(1);
        tick(vm, sc, cm);
        if (t % 100 == 0 && !queuesConsistent(vm)) consistent = false;
    }
    cm.setChaosMode(false);
    while (cm.undo()) {}                                   // undo everything
    int drain = 0;
    while (vm.activeCount() > 0 && drain < 20000) { tick(vm, sc, cm); drain++; }
    cout << "  accidents " << cm.getTotalAccidents() << ", closures " << cm.getTotalClosures()
         << ", surges " << cm.getTotalSurges() << ", reroutes " << cm.getTotalRerouted()
         << ", log entries " << cm.eventCount() << "\n";
    cout << "  spawned " << vm.spawnedCount() << ", arrived " << vm.arrivedCount()
         << ", cancelled by undo " << vm.cancelledCount() << ", diverted " << vm.divertedCount()
         << ", avg trip " << (int)vm.averageTravelTime() << "s\n";
    printLog(cm, 6);
    check(cm.getTotalAccidents() + cm.getTotalClosures() + cm.getTotalSurges() >= 20, "~30 random events happened");
    check(consistent, "queues consistent throughout");
    check(vm.activeCount() == 0, "city fully drains (no permanent gridlock)");
    check(vm.arrivedCount() + vm.cancelledCount() + vm.divertedCount() == vm.spawnedCount(),
          "every vehicle accounted for");
    check(city.countBlocked() == 0 && cm.closedRoadCount() == 0, "undo-all reopened every road");
}

int main() {
    cout << "===== CHAOS CITY : STEP 6 CHAOS MODE TESTS =====\n";
    Graph city;
    string err;
    if (!city.loadFromFile("city_map.txt", err)) {
        cout << "Cannot load map: " << err << "\n";
        cin.get();
        return 1;
    }
    testClosure(city);
    testUndoRedo(city);
    testAccident(city);
    testSurgeUndo(city);
    testConnectivityGuard(city);
    testChaosMode(city);
    cout << "\nResult: " << passed << " passed, " << failed << " failed\n";
    cout << "\nPress Enter to exit...";
    cin.get();
    return 0;
}
