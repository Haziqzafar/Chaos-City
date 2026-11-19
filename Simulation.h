// =====================================================================
//  Chaos City - Simulation.h
//  ONE object that owns and drives the whole system. The console demo and
//  the SFML GUI only ever talk to this class.
//
//   Simulation
//    |- Graph             city map (Step 2)
//    |- VehicleManager    vehicles, queues, routing (Step 4)
//    |- SignalController  traffic lights (Step 5)
//    |- ChaosManager      closures, accidents, surges, undo/redo (Step 6)
//    |- Analytics         statistics & history (Step 7)
//    |- DSAPanel          benchmarks & algorithm demos (Step 8)
//
//  Time: one tick = SIM_TICK (1 s) of simulated time.
//   tick()            - exactly one step (console, tests)
//   advance(realSec)  - for the GUI: runs realSec x speed ticks, e.g.
//                       speed 10 at 60 FPS -> 10 simulated s per real s.
//   pause / resume, speed 0.25x - 60x, automatic spawn rate (veh/min).
// =====================================================================
#ifndef SIMULATION_H
#define SIMULATION_H

#include <string>
#include <fstream>
#include <sstream>
#include "Graph.h"
#include "GraphAlgorithms.h"
#include "VehicleManager.h"
#include "SignalController.h"
#include "ChaosManager.h"
#include "Analytics.h"
#include "DSAPanel.h"
#include "MST.h"
#include "TopoSort.h"

const double MIN_SPEED = 0.25;
const double MAX_SPEED = 60.0;
const int    MAX_TICKS_PER_FRAME = 120;   // stops a slow PC from falling further behind

enum Scenario { SCENARIO_NORMAL = 0, SCENARIO_RUSH_HOUR, SCENARIO_NIGHT, SCENARIO_STADIUM, SCENARIO_EVACUATION };

inline const char* scenarioName(Scenario s) {
    switch (s) {
        case SCENARIO_RUSH_HOUR:  return "Rush Hour";
        case SCENARIO_NIGHT:      return "Quiet Night";
        case SCENARIO_STADIUM:    return "Stadium Event";
        case SCENARIO_EVACUATION: return "Evacuation";
        default:                  return "Normal Day";
    }
}

// Everything the HUD needs in one struct
struct SimSummary {
    double time;
    bool paused;
    double speed;
    double spawnRate;
    SignalStrategy strategy;
    bool chaosMode;
    int active, arrived, queued, entering, diverted, cancelled;
    double avgTrip, avgWait, avgSpeedKmh, avgCongestion;
    int accidents, closedRoads, rerouted;
    SimSummary() : time(0), paused(false), speed(1), spawnRate(0), strategy(ADAPTIVE),
        chaosMode(false), active(0), arrived(0), queued(0), entering(0), diverted(0),
        cancelled(0), avgTrip(0), avgWait(0), avgSpeedKmh(0), avgCongestion(0),
        accidents(0), closedRoads(0), rerouted(0) {}
};

class Simulation {
private:
    Graph city;
    VehicleManager* vm;
    SignalController* sc;
    ChaosManager* cm;
    Analytics* an;
    DSAPanel* panel;

    bool loaded;
    bool paused;
    double speed;
    double spawnRate;          // vehicles per simulated minute
    double spawnCredit;
    double tickCredit;         // fractional ticks carried between frames
    unsigned int seed;
    SignalStrategy strategy;
    Scenario scenario;

    Simulation(const Simulation&);
    Simulation& operator=(const Simulation&);

    void destroyManagers() {
        delete panel; panel = NULL;      // reverse order of creation
        delete an;    an = NULL;
        delete cm;    cm = NULL;
        delete sc;    sc = NULL;
        delete vm;    vm = NULL;
    }

    void createManagers() {
        vm = new VehicleManager(city, seed);
        sc = new SignalController(city, strategy);
        cm = new ChaosManager(city, *vm, seed + 1);
        an = new Analytics(city, *vm);
        panel = new DSAPanel(city, *vm, *cm, seed + 2);
    }

    void clearRoadStates() {
        for (int r = 0; r < city.getRoadCount(); r++) {
            Road* road = city.getRoad(r);
            road->blocked = false;
            road->incident = false;
            road->vehiclesOnRoad = 0;
        }
    }

public:
    Simulation(unsigned int randomSeed = 2026)
        : vm(NULL), sc(NULL), cm(NULL), an(NULL), panel(NULL), loaded(false),
          paused(false), speed(1.0), spawnRate(12.0), spawnCredit(0), tickCredit(0),
          seed(randomSeed), strategy(ADAPTIVE), scenario(SCENARIO_NORMAL) {}

    ~Simulation() { destroyManagers(); }

    // Load the map and build everything. Returns false with a message on error.
    bool init(const std::string& mapFile, std::string& error) {
        destroyManagers();
        loaded = false;
        if (!city.loadFromFile(mapFile, error)) return false;
        createManagers();
        loaded = true;
        return true;
    }

    // Fresh start on the same map (keeps speed, spawn rate and strategy)
    void reset() {
        if (!loaded) return;
        destroyManagers();
        clearRoadStates();
        spawnCredit = 0;
        tickCredit = 0;
        createManagers();
    }

    void setSeed(unsigned int s) { seed = s; }
    bool isLoaded() const { return loaded; }

    // ------------------------------------------------------------
    //  Time
    // ------------------------------------------------------------
    void tick() {
        if (!loaded) return;
        spawnCredit += spawnRate / 60.0 * SIM_TICK;
        while (spawnCredit >= 1.0) { vm->spawnRandom(); spawnCredit -= 1.0; }
        vm->update(SIM_TICK);
        sc->update(SIM_TICK, *vm);
        cm->update(SIM_TICK);
        an->update(SIM_TICK);
    }

    // GUI: call every frame with the real seconds since last frame.
    // Returns the number of ticks that were run.
    int advance(double realSeconds) {
        if (!loaded || paused || realSeconds <= 0) return 0;
        tickCredit += realSeconds * speed / SIM_TICK;
        int ticks = (int)(tickCredit + 1e-9);     // 9.9999999 must count as 10
        if (ticks > MAX_TICKS_PER_FRAME) { ticks = MAX_TICKS_PER_FRAME; tickCredit = 0; }
        else tickCredit -= ticks;
        for (int i = 0; i < ticks; i++) tick();
        return ticks;
    }

    void runFor(double simSeconds) {                // console / tests (ignores pause)
        int n = (int)(simSeconds / SIM_TICK);
        for (int i = 0; i < n; i++) tick();
    }

    void pause() { paused = true; }
    void resume() { paused = false; }
    void togglePause() { paused = !paused; }
    bool isPaused() const { return paused; }

    void setSpeed(double s) { speed = s < MIN_SPEED ? MIN_SPEED : (s > MAX_SPEED ? MAX_SPEED : s); }
    double getSpeed() const { return speed; }
    void faster() { setSpeed(speed * 2); }
    void slower() { setSpeed(speed / 2); }

    void setSpawnRate(double perMinute) { spawnRate = perMinute < 0 ? 0 : (perMinute > 300 ? 300 : perMinute); }
    double getSpawnRate() const { return spawnRate; }

    // ------------------------------------------------------------
    //  Controls (thin wrappers so the GUI needs only this class)
    // ------------------------------------------------------------
    void setStrategy(SignalStrategy s) { strategy = s; if (sc) sc->setStrategy(s); }
    SignalStrategy getStrategy() const { return strategy; }
    void toggleStrategy() { setStrategy(strategy == ADAPTIVE ? FIXED_TIMER : ADAPTIVE); }

    int spawnVehicle(int from, int to, VehicleType type) { return loaded ? vm->spawnVehicle(from, to, type) : -1; }
    bool closeRoad(int u, int v, bool bothWays = true) { return loaded && cm->closeRoad(u, v, bothWays); }
    bool reopenRoad(int u, int v, bool bothWays = true) { return loaded && cm->reopenRoad(u, v, bothWays); }
    bool accident(int roadId, double seconds = DEFAULT_ACCIDENT_TIME) { return loaded && cm->causeAccident(roadId, seconds); }
    int surge(int count) { return loaded ? cm->surge(count) : 0; }
    int surgeTo(int place, int count) { return loaded ? cm->surgeTo(place, count) : 0; }
    int surgeFrom(int place, int count) { return loaded ? cm->surgeFrom(place, count) : 0; }
    bool undo() { return loaded && cm->undo(); }
    bool redo() { return loaded && cm->redo(); }
    void setChaosMode(bool on) { if (loaded) cm->setChaosMode(on); }
    void toggleChaosMode() { if (loaded) cm->setChaosMode(!cm->isChaosMode()); }
    bool forceGreen(int roadId) { return loaded && sc->forceGreen(roadId); }

    // Preset situations for the demo
    void loadScenario(Scenario s) {
        reset();
        scenario = s;
        int stadium = city.findIntersection("Karsaz");
        int evacuate = city.findIntersection("Saddar");
        switch (s) {
            case SCENARIO_RUSH_HOUR: setSpawnRate(30); break;
            case SCENARIO_NIGHT:     setSpawnRate(4);  break;
            case SCENARIO_STADIUM:
                setSpawnRate(10);
                if (stadium != -1) cm->surgeTo(stadium, 120);
                break;
            case SCENARIO_EVACUATION:
                setSpawnRate(6);
                if (evacuate != -1) cm->surgeFrom(evacuate, 120);
                break;
            default: setSpawnRate(12); break;
        }
    }
    Scenario getScenario() const { return scenario; }

    // ------------------------------------------------------------
    //  Access for drawing / dashboards
    // ------------------------------------------------------------
    Graph& graph() { return city; }
    VehicleManager& vehicles() { return *vm; }
    SignalController& signals() { return *sc; }
    ChaosManager& chaos() { return *cm; }
    Analytics& analytics() { return *an; }
    DSAPanel& dsa() { return *panel; }

    SimSummary summary() const {
        SimSummary s;
        if (!loaded) return s;
        s.time = vm->getSimTime();
        s.paused = paused;
        s.speed = speed;
        s.spawnRate = spawnRate;
        s.strategy = strategy;
        s.chaosMode = cm->isChaosMode();
        s.active = vm->activeCount();
        s.arrived = vm->arrivedCount();
        s.queued = vm->totalQueued();
        s.entering = vm->totalEntering();
        s.diverted = vm->divertedCount();
        s.cancelled = vm->cancelledCount();
        s.avgTrip = an->avgTravelTime();
        s.avgWait = an->avgWaitTime();
        s.avgSpeedKmh = an->averageSpeedKmh();
        s.avgCongestion = an->averageCongestion();
        s.accidents = cm->activeAccidents();
        s.closedRoads = cm->closedRoadCount();
        s.rerouted = cm->getTotalRerouted();
        return s;
    }

    static std::string formatTime(double seconds) {
        int t = (int)seconds;
        std::stringstream ss;
        int h = t / 3600, m = (t / 60) % 60, s = t % 60;
        ss << h << ":" << (m < 10 ? "0" : "") << m << ":" << (s < 10 ? "0" : "") << s;
        return ss.str();
    }

    std::string roadLabel(int roadId) const {
        const Road* r = city.getRoad(roadId);
        if (r == NULL) return "?";
        return city.getIntersection(r->from).name + " -> " + city.getIntersection(r->to).name;
    }

    // ------------------------------------------------------------
    //  Text report (for the project report / "Export" button)
    // ------------------------------------------------------------
    bool writeReport(const std::string& path) {
        if (!loaded) return false;
        std::ofstream out(path.c_str());
        if (!out) return false;
        SimSummary s = summary();
        out << "CHAOS CITY - SIMULATION REPORT\n==============================\n";
        out << "Scenario: " << scenarioName(scenario) << "\n";
        out << "Simulated time: " << formatTime(s.time) << "\n";
        out << "Signal strategy: " << SignalController::strategyName(s.strategy) << "\n";
        out << "Spawn rate: " << s.spawnRate << " vehicles/min\n\n";

        out << "VEHICLES\n";
        out << "  Spawned " << vm->spawnedCount() << ", arrived " << s.arrived << ", active " << s.active
            << ", diverted " << s.diverted << ", cancelled " << s.cancelled << "\n";
        out << "  Currently queued " << s.queued << ", waiting at origin " << s.entering << "\n\n";

        out << "TRIPS (AVL tree, height " << an->getTree().height() << ")\n";
        out << "  Average " << (int)an->avgTravelTime() << " s, median " << (int)an->medianTrip()
            << " s, 90th percentile " << (int)an->percentile(90) << " s\n";
        out << "  Fastest " << (int)an->fastestTrip() << " s, slowest " << (int)an->slowestTrip() << " s\n";
        out << "  Average wait " << (int)an->avgWaitTime() << " s, average distance " << an->avgTripKm() << " km\n";
        out << "  By type: Car " << (int)an->avgTimeByType(CAR) << " s, Bus " << (int)an->avgTimeByType(BUS)
            << " s, Truck " << (int)an->avgTimeByType(TRUCK) << " s, Ambulance " << (int)an->avgTimeByType(AMBULANCE) << " s\n";
        int buckets[6];
        an->histogram(buckets, 6, 600);
        out << "  Histogram (10-minute buckets): ";
        for (int b = 0; b < 6; b++) out << buckets[b] << (b < 5 ? " | " : " (60+ min)\n\n");

        out << "MOST CONGESTED ROADS (merge sort)\n";
        int top[5];
        SortStats st;
        int k = an->topCongestedRoads(top, 5, MERGE_SORT, st);
        for (int i = 0; i < k; i++)
            out << "  " << i + 1 << ". " << roadLabel(top[i]) << "  "
                << (int)(city.getRoad(top[i])->congestionRatio() * 100) << "% full\n";

        out << "\nCHAOS\n";
        out << "  Accidents " << cm->getTotalAccidents() << ", closures " << cm->getTotalClosures()
            << ", surges " << cm->getTotalSurges() << ", vehicles rerouted " << cm->getTotalRerouted() << "\n";

        int V = city.getNodeCount();
        MSTEdge* edges = new MSTEdge[V];
        MSTResult mst = MST::kruskal(city, edges);
        out << "\nCORE NETWORK (minimum spanning tree, Kruskal)\n";
        out << "  " << mst.totalWeight / 1000.0 << " km of " << MST::networkLength(city) / 1000.0
            << " km, " << mst.components << " component(s)\n";
        for (int i = 0; i < mst.edgeCount; i++)
            out << "  " << city.getIntersection(edges[i].u).name << " - "
                << city.getIntersection(edges[i].v).name << " (" << edges[i].weight << " m)\n";
        delete[] edges;

        out << "\nEVENT LOG\n";
        for (ListNode<ChaosEvent>* n = cm->eventLogHead(); n != NULL; n = n->next)
            out << "  [" << formatTime(n->data.time) << "] " << n->data.text << "\n";
        return true;
    }
};

#endif
