// =====================================================================
//  Chaos City - City.h
//  Basic building blocks of the road network:
//    Intersection = graph VERTEX
//    Road         = directed, weighted graph EDGE
//  A two-way street is stored as two Roads (one per direction), because
//  each direction has its own traffic, its own queue and can be blocked
//  separately (e.g. an accident on one side only).
// =====================================================================
#ifndef CITY_H
#define CITY_H

#include <string>

const int INF = 1000000000;      // "no path" / "road unusable"
// Simulation scale: one simulated vehicle stands for a group of real cars,
// so each "slot" covers 250 m of one lane. This lets a few dozen simulated
// vehicles actually congest a road (real spacing ~7 m would need thousands).
const int CAR_SPACE_METERS = 250;

struct Intersection {
    int id;
    std::string name;
    float x, y;                  // screen position, used later by the SFML GUI

    Intersection() : id(-1), name(""), x(0), y(0) {}
    Intersection(int i, const std::string& n, float px, float py)
        : id(i), name(n), x(px), y(py) {}
};

struct Road {
    int id;
    int from;                    // intersection id where the road starts
    int to;                      // intersection id where the road ends
    int length;                  // meters
    int lanes;
    int speedLimit;              // km/h
    bool blocked;                // no NEW vehicles may enter (closure or accident)
    bool incident;               // accident ON the road: vehicles on it cannot move
    int vehiclesOnRoad;          // updated by the simulation

    Road() : id(-1), from(-1), to(-1), length(0), lanes(1),
             speedLimit(40), blocked(false), incident(false), vehiclesOnRoad(0) {}

    Road(int i, int f, int t, int len, int ln, int spd)
        : id(i), from(f), to(t), length(len), lanes(ln),
          speedLimit(spd), blocked(false), incident(false), vehiclesOnRoad(0) {}

    // How many cars fit on this road at once
    int capacity() const {
        int c = (length / CAR_SPACE_METERS) * lanes;
        return c < 1 ? 1 : c;
    }

    // Free-flow travel time in seconds: time = distance / speed
    int baseTime() const {
        int t = (int)(length * 3.6 / speedLimit);
        return t < 1 ? 1 : t;
    }

    // Travel time with congestion, using the BPR formula used by real
    // traffic engineers:  t = t0 * (1 + 0.15 * (volume/capacity)^4)
    // Blocked roads return INF so routing algorithms avoid them.
    int currentTime() const {
        if (blocked) return INF;
        return (int)(baseTime() * slowdown());
    }

    // BPR congestion multiplier (1.0 = free flow)
    double slowdown() const {
        double ratio = (double)vehiclesOnRoad / capacity();
        return 1.0 + 0.15 * ratio * ratio * ratio * ratio;
    }

    double congestionRatio() const {
        return (double)vehiclesOnRoad / capacity();
    }
};

#endif
