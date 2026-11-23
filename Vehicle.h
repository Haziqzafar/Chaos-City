// =====================================================================
//  Chaos City - Vehicle.h
//  One vehicle in the simulation. Always created with new and handled
//  through a Vehicle* (stored in the hash table and the active list).
//
//  Movement model (one simulation tick = SIM_TICK seconds):
//   MOVING  : drives along currentRoad; position = meters from road start
//   WAITING : reached end of road, standing in that road's queue
//             (in Step 5 the traffic light decides when it may cross)
//   STUCK   : waiting, but no route to destination exists (roads closed)
//   ENTERING: spawned at origin, waiting for space on its first road
//   ARRIVED : reached destination, about to be removed
// =====================================================================
#ifndef VEHICLE_H
#define VEHICLE_H

#include <string>
#include "LinkedList.h"
#include "Graph.h"

const double SIM_TICK = 1.0;          // seconds of simulated time per tick

enum VehicleType  { CAR = 0, BUS = 1, TRUCK = 2, AMBULANCE = 3 };
enum VehicleState { MOVING = 0, WAITING = 1, STUCK = 2, ARRIVED = 3, ENTERING = 4 };
// ENTERING: just spawned, but its first road is full -> waits at the
//           origin intersection until there is space

struct Vehicle {
    int id;
    VehicleType type;
    int origin;               // intersection id
    int destination;          // intersection id
    int currentRoad;          // road id the vehicle is on (or queued at the end of)
    double position;          // meters travelled along currentRoad
    LinkedList<int> route;    // REMAINING road ids after currentRoad
    VehicleState state;

    // statistics
    double spawnTime;
    double waitTime;          // total seconds spent waiting in queues
    double stuckTime;         // seconds stuck at the current queue head
    double distance;          // meters driven
    int reroutes;

    Vehicle(int i, VehicleType t, int o, int d, double now)
        : id(i), type(t), origin(o), destination(d), currentRoad(-1),
          position(0), state(MOVING), spawnTime(now), waitTime(0),
          stuckTime(0), distance(0), reroutes(0) {}

    int maxSpeed() const {               // km/h
        switch (type) {
            case BUS:       return 40;
            case TRUCK:     return 35;
            case AMBULANCE: return 80;
            default:        return 60;
        }
    }

    bool isEmergency() const { return type == AMBULANCE; }

    static std::string typeName(VehicleType t) {
        switch (t) {
            case BUS:       return "Bus";
            case TRUCK:     return "Truck";
            case AMBULANCE: return "Ambulance";
            default:        return "Car";
        }
    }

    static std::string stateName(VehicleState s) {
        switch (s) {
            case WAITING: return "Waiting";
            case STUCK:   return "Stuck";
            case ARRIVED: return "Arrived";
            case ENTERING: return "Entering";
            default:      return "Moving";
        }
    }

    // Screen position for the GUI: interpolate between the two
    // intersections of the current road.
    void getScreenPos(const Graph& g, float& x, float& y) const {
        const Road* r = g.getRoad(currentRoad);
        if (r == NULL) { x = y = 0; return; }
        const Intersection& a = g.getIntersection(r->from);
        const Intersection& b = g.getIntersection(r->to);
        float t = (float)(position / r->length);
        if (t > 1) t = 1;
        x = a.x + (b.x - a.x) * t;
        y = a.y + (b.y - a.y) * t;
    }

private:
    Vehicle(const Vehicle&);
    Vehicle& operator=(const Vehicle&);
};

// Summary kept after a vehicle finishes (fed into the analytics tree in Step 7)
struct TripRecord {
    int vehicleId;
    VehicleType type;
    int origin, destination;
    double travelTime;     // seconds from spawn to arrival
    double waitTime;
    double distance;       // meters
    int reroutes;

    TripRecord() : vehicleId(-1), type(CAR), origin(-1), destination(-1),
                   travelTime(0), waitTime(0), distance(0), reroutes(0) {}
};

#endif
