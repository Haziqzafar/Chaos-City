// =====================================================================
//  Chaos City - MapView.h
//  Draws the live city map and handles zoom, pan and mouse hovering.
//
//   - Each direction of a two-way road is its own LANE, offset to the
//     LEFT of the direction of travel (Pakistan drives on the left).
//   - Lane colour = congestion (green -> yellow -> red); grey when empty;
//     dashed red when closed; flashing warning sign for an accident.
//   - Traffic light dot at the end of each lane (red / yellow / green).
//   - Vehicles drawn as small rotated rectangles, coloured by type;
//     queued vehicles line up behind the stop line.
//   - "+N" badge at an intersection = vehicles waiting to enter the city.
//   - Zoom with the mouse wheel (around the cursor), drag to pan.
//   - Overlays (Step 10b): highlighted route with A/B markers, selected
//     road / intersection, ring around a followed vehicle, tool-coloured
//     hover outline.
//   - DSA overlays (Step 10d): minimum spanning tree edges, evacuation
//     order badges (topological sort), highlighted intersections
//     (string search results).
// =====================================================================
#ifndef MAPVIEW_H
#define MAPVIEW_H

#include "GuiCommon.h"
#include "Simulation.h"
#include "MST.h"

class MapView {
private:
    Simulation& sim;
    sf::FloatRect area;          // screen rectangle the map is drawn in
    float zoom;                  // screen pixels per map unit
    float fitZoom;               // zoom that fits the whole city
    sf::Vector2f offset;         // screen position of map point (0,0)
    float minX, minY, maxX, maxY;

    int hoverRoad, hoverNode;
    int selectedNode, selectedRoad;
    LinkedList<int> route;       // road ids of the highlighted route
    int routeA, routeB;          // route start / end intersections
    int followId;                // vehicle with a ring around it (-1 = none)
    bool followVisible;
    sf::Vector2f followPos;
    sf::Color hoverTint;

    // DSA overlays
    MSTEdge* mstEdges;           // edges to draw (size nodeCount)
    int mstCount;
    int* evacRank;               // rank in evacuation order, -1 = not shown
    int evacSource;
    bool* highlighted;           // intersections found by string search
    int* queueSlot;              // per-road counter while drawing queues
    int* enteringAt;             // per-intersection waiting-to-enter count
    int roadCount, nodeCount;

    MapView(const MapView&);
    MapView& operator=(const MapView&);

    float scale() const { return zoom / fitZoom; }      // 1.0 = default view

    void computeBounds() {
        Graph& g = sim.graph();
        minX = minY = 1e9f; maxX = maxY = -1e9f;
        for (int v = 0; v < g.getNodeCount(); v++) {
            const Intersection& n = g.getIntersection(v);
            if (n.x < minX) minX = n.x;
            if (n.y < minY) minY = n.y;
            if (n.x > maxX) maxX = n.x;
            if (n.y > maxY) maxY = n.y;
        }
        if (minX > maxX) { minX = minY = 0; maxX = maxY = 100; }
    }

    // Lane geometry: start/end points on screen, shifted left and shortened
    // so lanes start at the edge of the intersection circles.
    void laneEnds(int roadId, sf::Vector2f& a, sf::Vector2f& b) const {
        Graph& g = sim.graph();
        const Road* r = g.getRoad(roadId);
        a = toScreen(g.getIntersection(r->from).x, g.getIntersection(r->from).y);
        b = toScreen(g.getIntersection(r->to).x, g.getIntersection(r->to).y);
        sf::Vector2f d = b - a;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len < 1) return;
        sf::Vector2f u = d / len;
        if (g.findRoad(r->to, r->from) != -1) {
            sf::Vector2f left(u.y, -u.x);                // left of travel on screen
            float off = laneOffset();
            a += left * off;
            b += left * off;
        }
        float cut = nodeRadius() + 2;
        if (len > 2 * cut + 4) { a += u * cut; b -= u * cut; }
    }

public:
    float nodeRadius() const { return clampf(8 * scale(), 5, 18); }
    float laneOffset() const { return clampf(4.5f * scale(), 2.5f, 12); }
    float laneWidth(int lanes) const { return clampf((1.6f + 1.0f * lanes) * scale(), 2, 12); }
    float vehicleScale() const { return clampf(scale(), 0.75f, 1.5f); }

    MapView(Simulation& s) : sim(s), zoom(1), fitZoom(1), hoverRoad(-1), hoverNode(-1),
                             selectedNode(-1), selectedRoad(-1), routeA(-1), routeB(-1),
                             followId(-1), followVisible(false), hoverTint(255, 255, 255, 170) {
        roadCount = sim.graph().getRoadCount();
        nodeCount = sim.graph().getNodeCount();
        queueSlot = new int[roadCount > 0 ? roadCount : 1];
        enteringAt = new int[nodeCount > 0 ? nodeCount : 1];
        mstEdges = new MSTEdge[nodeCount > 0 ? nodeCount : 1];
        mstCount = 0;
        evacRank = new int[nodeCount > 0 ? nodeCount : 1];
        highlighted = new bool[nodeCount > 0 ? nodeCount : 1];
        evacSource = -1;
        for (int v = 0; v < nodeCount; v++) { evacRank[v] = -1; highlighted[v] = false; }
        computeBounds();
    }

    ~MapView() {
        delete[] queueSlot; delete[] enteringAt;
        delete[] mstEdges; delete[] evacRank; delete[] highlighted;
    }

    // ---------- DSA overlays ----------
    void setMST(const MSTEdge* e, int n) {
        mstCount = n < nodeCount ? n : nodeCount;
        for (int i = 0; i < mstCount; i++) mstEdges[i] = e[i];
    }
    void clearMST() { mstCount = 0; }

    // order = topological order; only reachable intersections get a badge
    void setEvacOrder(const int* order, int n, const int* dist, int source) {
        for (int v = 0; v < nodeCount; v++) evacRank[v] = -1;
        int rank = 1;
        for (int i = 0; i < n; i++) if (dist[order[i]] != INF) evacRank[order[i]] = rank++;
        evacSource = source;
    }
    void clearEvac() { for (int v = 0; v < nodeCount; v++) evacRank[v] = -1; evacSource = -1; }

    void setHighlights(const LinkedList<int>& nodes) {
        for (int v = 0; v < nodeCount; v++) highlighted[v] = false;
        for (ListNode<int>* n = nodes.getHead(); n != NULL; n = n->next)
            if (n->data >= 0 && n->data < nodeCount) highlighted[n->data] = true;
    }
    void clearHighlights() { for (int v = 0; v < nodeCount; v++) highlighted[v] = false; }

    // ------------------------------------------------------------
    //  View transform
    // ------------------------------------------------------------
    void setArea(const sf::FloatRect& r) { area = r; }
    const sf::FloatRect& getArea() const { return area; }

    void fitToArea() {
        float margin = 60;
        float w = maxX - minX, h = maxY - minY;
        float zx = (area.width - 2 * margin) / (w > 1 ? w : 1);
        float zy = (area.height - 2 * margin) / (h > 1 ? h : 1);
        fitZoom = zoom = zx < zy ? zx : zy;
        offset.x = area.left + (area.width - w * zoom) / 2 - minX * zoom;
        offset.y = area.top + (area.height - h * zoom) / 2 - minY * zoom;
    }

    sf::Vector2f toScreen(float wx, float wy) const { return sf::Vector2f(offset.x + wx * zoom, offset.y + wy * zoom); }
    sf::Vector2f toWorld(sf::Vector2f p) const { return sf::Vector2f((p.x - offset.x) / zoom, (p.y - offset.y) / zoom); }

    // Zoom keeping the map point under the cursor fixed
    void zoomAt(float factor, sf::Vector2f screenPoint) {
        float nz = clampf(zoom * factor, fitZoom * 0.5f, fitZoom * 6);
        sf::Vector2f w = toWorld(screenPoint);
        zoom = nz;
        offset.x = screenPoint.x - w.x * zoom;
        offset.y = screenPoint.y - w.y * zoom;
    }

    void panBy(float dx, float dy) { offset.x += dx; offset.y += dy; }
    bool contains(sf::Vector2f p) const { return area.contains(p); }

    // ------------------------------------------------------------
    //  Mouse hover
    // ------------------------------------------------------------
    void updateHover(sf::Vector2f mouse) {
        hoverRoad = hoverNode = -1;
        if (!area.contains(mouse)) return;
        Graph& g = sim.graph();
        float bestNode = nodeRadius() + 4;
        for (int v = 0; v < g.getNodeCount(); v++) {
            sf::Vector2f c = toScreen(g.getIntersection(v).x, g.getIntersection(v).y);
            float dx = c.x - mouse.x, dy = c.y - mouse.y;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < bestNode) { bestNode = d; hoverNode = v; }
        }
        if (hoverNode != -1) return;
        float bestRoad = laneOffset() + 3;
        for (int r = 0; r < g.getRoadCount(); r++) {
            sf::Vector2f a, b;
            laneEnds(r, a, b);
            float d = distToSegment(mouse, a, b);
            if (d < bestRoad) { bestRoad = d; hoverRoad = r; }
        }
    }

    int getHoverRoad() const { return hoverRoad; }
    int getHoverNode() const { return hoverNode; }
    void setSelectedNode(int v) { selectedNode = v; }
    void setSelectedRoad(int r) { selectedRoad = r; }
    int getSelectedNode() const { return selectedNode; }
    int getSelectedRoad() const { return selectedRoad; }
    void setHoverTint(const sf::Color& c) { hoverTint = c; }

    void setRoute(const LinkedList<int>& roads, int a, int b) {
        route.clear();
        for (ListNode<int>* n = roads.getHead(); n != NULL; n = n->next) route.pushBack(n->data);
        routeA = a; routeB = b;
    }
    void setRouteEnds(int a, int b) { routeA = a; routeB = b; }
    void clearRoute() { route.clear(); routeA = routeB = -1; }
    void setFollow(int vehicleId) { followId = vehicleId; }
    bool isFollowVisible() const { return followVisible; }
    sf::Vector2f getFollowPos() const { return followPos; }

    // ------------------------------------------------------------
    //  Drawing
    // ------------------------------------------------------------
    void draw(sf::RenderWindow& win, const sf::Font* font, float seconds) {
        Graph& g = sim.graph();
        VehicleManager& vm = sim.vehicles();
        SignalController& sc = sim.signals();
        ChaosManager& cm = sim.chaos();
        bool flash = ((int)(seconds * 3)) % 2 == 0;

        // background + grid (clipped to the map area by drawing the panel on top later)
        drawRect(win, area.left, area.top, area.width, area.height, Theme::MAP_BG);
        float step = 50 * zoom;
        if (step > 12) {
            sf::Vector2f o = toScreen(0, 0);
            float sx = o.x - std::floor((o.x - area.left) / step) * step;
            float sy = o.y - std::floor((o.y - area.top) / step) * step;
            for (float x = sx; x < area.left + area.width; x += step)
                drawThickLine(win, sf::Vector2f(x, area.top), sf::Vector2f(x, area.top + area.height), 1, Theme::GRID);
            for (float y = sy; y < area.top + area.height; y += step)
                drawThickLine(win, sf::Vector2f(area.left, y), sf::Vector2f(area.left + area.width, y), 1, Theme::GRID);
        }

        // 0) highlighted route glow (under the lanes)
        for (ListNode<int>* n = route.getHead(); n != NULL; n = n->next) {
            sf::Vector2f a, b;
            laneEnds(n->data, a, b);
            float w = laneWidth(g.getRoad(n->data)->lanes);
            drawThickLine(win, a, b, w + 9 * clampf(scale(), 0.8f, 1.6f), sf::Color(84, 160, 255, flash ? 120 : 90));
        }

        // 1) roads: hover / selected outline, then each lane
        for (int r = 0; r < g.getRoadCount(); r++) {
            const Road* road = g.getRoad(r);
            sf::Vector2f a, b;
            laneEnds(r, a, b);
            float w = laneWidth(road->lanes);
            if (r == selectedRoad) drawThickLine(win, a, b, w + 6, Theme::ACCENT);
            if (r == hoverRoad) drawThickLine(win, a, b, w + 5, hoverTint);

            if (cm.isClosed(r)) {
                drawThickLine(win, a, b, w, Theme::ROAD_CLOSED);
                drawDashedLine(win, a, b, w * 0.6f, 6, withAlpha(Theme::BAD, 200));
            } else if (cm.hasAccident(r)) {
                drawThickLine(win, a, b, w, flash ? Theme::BAD : sf::Color(140, 40, 35));
            } else if (road->vehiclesOnRoad == 0) {
                drawThickLine(win, a, b, w, Theme::ROAD_EMPTY);
            } else {
                drawThickLine(win, a, b, w, congestionColor(road->congestionRatio()));
            }
            // direction arrow at 60% of the lane (helps on one-way roads)
            sf::Vector2f d = b - a;
            float len = std::sqrt(d.x * d.x + d.y * d.y);
            if (len > 40 && scale() > 0.8f) {
                sf::Vector2f u = d / len, n(-u.y, u.x);
                sf::Vector2f tip = a + d * 0.6f;
                float s = 3.5f * vehicleScale();
                sf::ConvexShape arrow(3);
                arrow.setPoint(0, tip + u * s);
                arrow.setPoint(1, tip - u * s + n * s);
                arrow.setPoint(2, tip - u * s - n * s);
                arrow.setFillColor(sf::Color(255, 255, 255, 60));
                win.draw(arrow);
            }
        }

        // 2) vehicles
        for (int r = 0; r < roadCount; r++) queueSlot[r] = 0;
        for (int v = 0; v < nodeCount; v++) enteringAt[v] = 0;
        float vs = vehicleScale();
        followVisible = false;
        for (ListNode<Vehicle*>* n = vm.activeHead(); n != NULL; n = n->next) {
            const Vehicle* v = n->data;
            if (v->state == ENTERING) {
                enteringAt[v->origin]++;
                if (v->id == followId) {
                    const Intersection& o = g.getIntersection(v->origin);
                    followPos = toScreen(o.x, o.y);
                    followVisible = true;
                }
                continue;
            }
            const Road* road = g.getRoad(v->currentRoad);
            sf::Vector2f a, b;
            laneEnds(v->currentRoad, a, b);
            sf::Vector2f d = b - a;
            float len = std::sqrt(d.x * d.x + d.y * d.y);
            if (len < 1) continue;
            float t;
            float carLen = (v->type == BUS ? 14 : v->type == TRUCK ? 12 : 9) * vs;
            if (v->state == MOVING) {
                t = (float)(v->position / road->length);
            } else {                                   // queued behind the stop line
                int slot = queueSlot[v->currentRoad]++;
                t = 1 - (slot * 11 * vs + 6 * vs) / len;     // 11 px per car in the queue
                if (t < 0.02f) t = 0.02f;
            }
            sf::Vector2f p = a + d * clampf(t, 0, 1);
            if (v->id == followId) { followPos = p; followVisible = true; }
            float ang = std::atan2(d.y, d.x) * 180 / GUI_PI;
            float carW = (v->type == BUS || v->type == TRUCK ? 6 : 5) * vs;

            sf::RectangleShape body(sf::Vector2f(carLen, carW));
            body.setOrigin(carLen / 2, carW / 2);
            body.setPosition(p);
            body.setRotation(ang);
            sf::Color c = v->type == BUS ? Theme::BUS : v->type == TRUCK ? Theme::TRUCK
                        : v->type == AMBULANCE ? (flash ? Theme::BAD : Theme::AMBULANCE) : Theme::CAR;
            body.setFillColor(c);
            if (v->state == STUCK) { body.setOutlineColor(Theme::BAD); body.setOutlineThickness(1.5f); }
            else { body.setOutlineColor(sf::Color(0, 0, 0, 140)); body.setOutlineThickness(1); }
            win.draw(body);
        }

        // 3) accident warning signs (on top of vehicles)
        for (int r = 0; r < g.getRoadCount(); r++) {
            if (!cm.hasAccident(r)) continue;
            sf::Vector2f a, b;
            laneEnds(r, a, b);
            sf::Vector2f m = (a + b) * 0.5f;
            float s = 9 * vs;
            sf::ConvexShape tri(3);
            tri.setPoint(0, sf::Vector2f(m.x, m.y - s));
            tri.setPoint(1, sf::Vector2f(m.x + s, m.y + s * 0.8f));
            tri.setPoint(2, sf::Vector2f(m.x - s, m.y + s * 0.8f));
            tri.setFillColor(flash ? Theme::WARN : Theme::BAD);
            tri.setOutlineColor(sf::Color::Black);
            tri.setOutlineThickness(1.5f);
            win.draw(tri);
            drawText(win, font, "!", m.x - 2.5f * vs, m.y - s * 0.75f, (unsigned)(11 * vs), sf::Color::Black, true);
        }

        // 3b) minimum spanning tree edges (centre to centre, teal)
        for (int i = 0; i < mstCount; i++) {
            const Intersection& p = g.getIntersection(mstEdges[i].u);
            const Intersection& q = g.getIntersection(mstEdges[i].v);
            sf::Vector2f a = toScreen(p.x, p.y), b = toScreen(q.x, q.y);
            drawThickLine(win, a, b, 7 * clampf(scale(), 0.8f, 1.5f), sf::Color(0, 220, 190, 200));
            sf::Vector2f m = (a + b) * 0.5f;
            std::string km = toStr1(mstEdges[i].weight / 1000.0) + " km";
            float tw = textWidth(font, km, 11, true);
            drawRect(win, m.x - tw / 2 - 4, m.y - 9, tw + 8, 17, sf::Color(0, 70, 62, 230));
            drawText(win, font, km, m.x - tw / 2, m.y - 8, 11, sf::Color(170, 255, 240), true);
        }

        // 4) traffic lights at the end of each lane
        float lr = clampf(3.2f * scale(), 2.5f, 7);
        for (int r = 0; r < g.getRoadCount(); r++) {
            const TrafficLight* L = sc.getLight(g.getRoad(r)->to);
            if (L->count <= 1) continue;               // no conflict -> no signal
            sf::Vector2f a, b;
            laneEnds(r, a, b);
            LightColor col = sc.colorOf(r);
            sf::Color c = col == LIGHT_GREEN ? Theme::GOOD : col == LIGHT_YELLOW ? Theme::WARN : Theme::BAD;
            drawCircle(win, b, lr, c, sf::Color(10, 10, 12), 1.2f);
        }

        // 5) intersections + labels + entering badges
        float nr = nodeRadius();
        unsigned int labelSize = (unsigned int)clampf(13 * scale(), 11, 20);
        for (int v = 0; v < g.getNodeCount(); v++) {
            const Intersection& in = g.getIntersection(v);
            sf::Vector2f c = toScreen(in.x, in.y);
            bool hot = (v == hoverNode || v == selectedNode);
            drawCircle(win, c, nr, Theme::NODE, hot ? sf::Color::White : Theme::NODE_RING, hot ? 3.0f : 1.5f);
            drawText(win, font, in.name, c.x + nr + 4, c.y - nr - 2, labelSize, Theme::TEXT, false, true);
            if (enteringAt[v] > 0) {
                std::string s = "+" + toStr(enteringAt[v]);
                float tw = textWidth(font, s, 11, true);
                float bx = c.x - nr - tw - 10, by = c.y - 8;
                drawRect(win, bx, by, tw + 8, 16, withAlpha(Theme::WARN, 230));
                drawText(win, font, s, bx + 4, by + 1, 11, sf::Color::Black, true);
            }
        }

        // 5b) string-search highlights and evacuation order badges
        for (int v = 0; v < nodeCount; v++) {
            sf::Vector2f c = toScreen(g.getIntersection(v).x, g.getIntersection(v).y);
            if (highlighted[v]) {
                float pr = nr + 6 + 2 * (float)std::sin(seconds * 5);
                drawCircle(win, c, pr, sf::Color(242, 190, 50, 50), Theme::WARN, 3);
            }
            if (evacRank[v] > 0) {
                float t = nodeCount > 1 ? (evacRank[v] - 1) / (float)(nodeCount - 1) : 0;
                sf::Vector2f b(c.x - nr - 10, c.y + nr + 6);
                drawCircle(win, b, 10, lerpColor(Theme::BAD, Theme::GOOD, t), sf::Color::Black, 1.5f);
                std::string s = toStr(evacRank[v]);
                float tw = textWidth(font, s, 11, true);
                drawText(win, font, s, b.x - tw / 2, b.y - 8, 11, sf::Color::Black, true);
            }
        }
        if (evacSource >= 0 && evacSource < nodeCount) {
            sf::Vector2f c = toScreen(g.getIntersection(evacSource).x, g.getIntersection(evacSource).y);
            drawCircle(win, c, nr + 8, sf::Color::Transparent, Theme::BAD, 3);
        }

        // 6) route markers A / B and the followed vehicle
        int ends[2] = { routeA, routeB };
        for (int i = 0; i < 2; i++) {
            if (ends[i] < 0 || ends[i] >= nodeCount) continue;
            sf::Vector2f c = toScreen(g.getIntersection(ends[i]).x, g.getIntersection(ends[i]).y);
            sf::Color col = i == 0 ? Theme::GOOD : Theme::BAD;
            drawCircle(win, c, nr + 5, sf::Color::Transparent, col, 3);
            drawCircle(win, sf::Vector2f(c.x, c.y - nr - 16), 9, col, sf::Color::Black, 1.5f);
            drawText(win, font, i == 0 ? "A" : "B", c.x - 4.5f, c.y - nr - 25, 13, sf::Color::Black, true);
        }
        if (followVisible) {
            float pr = 9 + 3 * (float)std::sin(seconds * 6);
            drawCircle(win, followPos, pr, sf::Color::Transparent, sf::Color::White, 2);
        }
    }
};

#endif
