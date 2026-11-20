// =====================================================================
//  Chaos City - Step 4 test driver: vehicles
//  Keep city_map.txt in the same folder.
// =====================================================================
#include <iostream>
#include <string>
#include <cmath>
#include "Graph.h"
#include "GraphAlgorithms.h"
#include "VehicleManager.h"

using namespace std;

int passed = 0, failed = 0;

void check(bool condition, const string& testName) {
    if (condition) { passed++; cout << "  [PASS] " << testName << "\n"; }
    else           { failed++; cout << "  [FAIL] " << testName << "\n"; }
}

// Run ticks until the vehicle disappears (arrives) or maxTicks pass
int runUntilGone(VehicleManager& vm, int id, int maxTicks) {
    for (int t = 1; t <= maxTicks; t++) {
        vm.stepUncontrolled();
        if (vm.getVehicle(id) == NULL) return t;
    }
    return -1;
}

// Expected free-flow time of a route for a given vehicle top speed
int expectedTime(const Graph& g, const LinkedList<int>& route, int firstRoad, int vmax) {
    double t = 0;
    const Road* r = g.getRoad(firstRoad);
    int kmh = vmax < r->speedLimit ? vmax : r->speedLimit;
    t += r->length * 3.6 / kmh;
    for (ListNode<int>* n = route.getHead(); n != NULL; n = n->next) {
        r = g.getRoad(n->data);
        kmh = vmax < r->speedLimit ? vmax : r->speedLimit;
        t += r->length * 3.6 / kmh;
    }
    return (int)t;
}

void testSpawnAndRegistry(Graph& city) {
    cout << "\n== Spawning & hash registry ==\n";
    VehicleManager vm(city);
    int tower = city.findIntersection("Tower"), malir = city.findIntersection("Malir");

    int id = vm.spawnVehicle(tower, malir, CAR);
    check(id == 1001, "first vehicle gets ID 1001");
    Vehicle* v = vm.getVehicle(id);
    check(v != NULL && v->origin == tower && v->destination == malir, "hash lookup returns correct vehicle");
    check(city.getRoad(v->currentRoad)->vehiclesOnRoad == 1, "first road counts the vehicle");
    check(v->route.size() == 3, "3 roads remaining after first (4-road route)");

    check(vm.spawnVehicle(tower, tower, CAR) == -1, "origin == destination rejected");
    check(vm.spawnVehicle(tower, 99, CAR) == -1, "invalid intersection rejected");
    check(vm.rejectedCount() == 2 && vm.activeCount() == 1, "rejects counted, nothing leaked into list");
}

void testTravel(Graph& city) {
    cout << "\n== Travel Tower -> Malir ==\n";
    int tower = city.findIntersection("Tower"), malir = city.findIntersection("Malir");
    VehicleType types[3] = { CAR, BUS, AMBULANCE };
    int times[3];
    for (int i = 0; i < 3; i++) {
        VehicleManager vm(city);
        int id = vm.spawnVehicle(tower, malir, types[i]);
        Vehicle* v = vm.getVehicle(id);
        int expected = expectedTime(city, v->route, v->currentRoad, v->maxSpeed());
        int ticks = runUntilGone(vm, id, 5000);
        times[i] = ticks;
        const TripRecord& t = vm.getTrips().getHead()->data;
        cout << "  " << Vehicle::typeName(types[i]) << ": expected ~" << expected
             << "s, actual " << ticks << "s, distance " << t.distance << "m\n";
        // + up to 4 s per intersection crossed (queue discharge at saturation flow)
        check(ticks >= expected && ticks <= expected + 3 * 4 + 3, Vehicle::typeName(types[i]) + " time matches physics");
        check(fabs(t.distance - 22500) < 1, Vehicle::typeName(types[i]) + " drove 22.5 km");
        check(vm.activeCount() == 0 && vm.arrivedCount() == 1, "removed after arrival, trip recorded");
    }
    check(times[2] < times[0] && times[0] < times[1], "Ambulance < Car < Bus");
}

void testSpillback() {
    cout << "\n== Spillback (full next road blocks the queue) ==\n";
    Graph g;
    g.addIntersection("A", 0, 0); g.addIntersection("B", 1, 0); g.addIntersection("C", 2, 0);
    g.addRoad(0, 1, 500, 1, 36);     // A->B : 10 m/s, 50 s
    g.addRoad(1, 2, 250, 1, 9);      // B->C : capacity 1 car, 2.5 m/s, 100 s
    VehicleManager vm(g);
    int blocker = vm.spawnVehicle(1, 2, CAR);   // sits on B->C
    int waiter  = vm.spawnVehicle(0, 2, CAR);   // must enter B->C later
    for (int t = 0; t < 55; t++) vm.stepUncontrolled();
    Vehicle* w = vm.getVehicle(waiter);
    check(w != NULL && w->state == WAITING, "waiter queued at B because B->C is full");
    check(vm.queueLength(0) == 1, "queue on A->B has 1 vehicle");
    runUntilGone(vm, blocker, 500);
    vm.stepUncontrolled();
    check(w->state == MOVING && w->currentRoad == 1, "waiter enters once blocker leaves");
    runUntilGone(vm, waiter, 500);
    check(vm.arrivedCount() == 2, "both arrive");
    const TripRecord& t2 = vm.getTrips().getHead()->next->data;
    check(t2.waitTime > 0, "wait time recorded for queued vehicle");
}

void testRerouteAndStuck(Graph& city) {
    cout << "\n== Reroute & stuck vehicles ==\n";
    int tower = city.findIntersection("Tower"), malir = city.findIntersection("Malir");
    int karsaz = city.findIntersection("Karsaz"), airport = city.findIntersection("Airport");

    {
        VehicleManager vm(city);
        int id = vm.spawnVehicle(tower, malir, CAR);
        city.setBlockedBetween(karsaz, airport, true);   // accident after departure
        int ticks = runUntilGone(vm, id, 5000);
        const TripRecord& t = vm.getTrips().getHead()->data;
        cout << "  rerouted trip: " << ticks << "s, " << t.distance << "m, reroutes " << t.reroutes << "\n";
        check(ticks > 0 && t.reroutes == 1, "vehicle reroutes at Karsaz and still arrives");
        check(t.distance > 22500, "detour is longer");
        city.setBlockedBetween(karsaz, airport, false);
    }
    {
        VehicleManager vm(city);
        int id = vm.spawnVehicle(tower, malir, CAR);
        for (int r = 0; r < city.getRoadCount(); r++)        // cut Malir off
            if (city.getRoad(r)->to == malir) city.setBlocked(r, true);
        bool sawStuck = false;
        for (int t = 0; t < 3000 && vm.getVehicle(id) != NULL; t++) {
            vm.stepUncontrolled();
            Vehicle* v = vm.getVehicle(id);
            if (v != NULL && v->state == STUCK) sawStuck = true;
        }
        check(sawStuck, "vehicle becomes STUCK when destination unreachable");
        check(vm.getVehicle(id) == NULL && vm.divertedCount() == 1 && vm.arrivedCount() == 0,
              "stuck vehicle diverted after timeout");
        for (int r = 0; r < city.getRoadCount(); r++) city.setBlocked(r, false);
    }
}

void testRandomTraffic(Graph& city) {
    cout << "\n== 200 random vehicles ==\n";
    VehicleManager vm(city, 2026);
    int ok = vm.spawnMany(200);
    int peakQueue = 0, ticks = 0;
    while (vm.activeCount() > 0 && ticks < 20000) {
        vm.stepUncontrolled();
        if (vm.totalQueued() > peakQueue) peakQueue = vm.totalQueued();
        if (ticks % 100 == 0 && ticks < 400) vm.spawnMany(20);   // keep traffic coming
        ticks++;
    }
    cout << "  spawned " << vm.spawnedCount() << ", arrived " << vm.arrivedCount()
         << ", ticks " << ticks << ", avg trip " << (int)vm.averageTravelTime()
         << "s, peak queue " << peakQueue << "\n";
    check(ok == 200, "all 200 spawned");
    check(vm.activeCount() == 0 && vm.arrivedCount() == vm.spawnedCount(), "every vehicle arrived (no deadlock)");
    bool clean = true;
    for (int r = 0; r < city.getRoadCount(); r++) if (city.getRoad(r)->vehiclesOnRoad != 0) clean = false;
    check(clean, "all road counters back to 0");

    VehicleManager vm2(city, 7);
    vm2.spawnMany(30);
    vm2.stepUncontrolled();
    vm2.clearVehicles();
    clean = vm2.activeCount() == 0;
    for (int r = 0; r < city.getRoadCount(); r++) if (city.getRoad(r)->vehiclesOnRoad != 0) clean = false;
    check(clean, "clearVehicles resets everything");
}

int main() {
    cout << "===== CHAOS CITY : STEP 4 VEHICLE TESTS =====\n";
    Graph city;
    string err;
    if (!city.loadFromFile("city_map.txt", err)) {
        cout << "Cannot load map: " << err << "\n";
        cin.get();
        return 1;
    }
    testSpawnAndRegistry(city);
    testTravel(city);
    testSpillback();
    testRerouteAndStuck(city);
    testRandomTraffic(city);
    cout << "\nResult: " << passed << " passed, " << failed << " failed\n";
    cout << "\nPress Enter to exit...";
    cin.get();
    return 0;
}
