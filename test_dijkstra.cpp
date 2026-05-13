#include <iostream>
#include <vector>
#include <map>
#include <queue>
#include <tuple>
#include <algorithm>

struct Cell { int x, y; bool operator==(const Cell& o) const { return x==o.x && y==o.y; } };
struct PedalBounds { int x, y, w, h; };

std::vector<Cell> findPath (
    Cell start, Cell end,
    const std::vector<PedalBounds>& pedals,
    int cols, int rows)
{
    struct PQNode {
        int x, y, d;
        int cost;
        int parentIdx;
        bool operator> (const PQNode& o) const { return cost > o.cost; }
    };
    struct VisitedNode { Cell cell; int parent; };
    struct StateKey {
        int x, y, d;
        bool operator< (const StateKey& o) const {
            if (x != o.x) return x < o.x;
            if (y != o.y) return y < o.y;
            return d < o.d;
        }
    };
    auto isValidNode = [&] (int x, int y) { return x >= -1 && x <= cols + 1 && y >= -1 && y <= rows + 1; };
    auto isEdgeBlocked = [&] (int x1, int y1, int x2, int y2) {
        for (auto& p : pedals) {
            if (y1 == y2) {
                if (y1 > p.y && y1 < p.y + p.h) {
                    int minX = std::min(x1, x2);
                    if (minX >= p.x && minX < p.x + p.w) return true;
                }
            } else if (x1 == x2) {
                if (x1 > p.x && x1 < p.x + p.w) {
                    int minY = std::min(y1, y2);
                    if (minY >= p.y && minY < p.y + p.h) return true;
                }
            }
        }
        return false;
    };
    std::vector<VisitedNode> visited;
    std::map<StateKey, int> minCost;
    std::priority_queue<PQNode, std::vector<PQNode>, std::greater<PQNode>> pq;
    pq.push ({ start.x, start.y, 4, 0, -1 });
    minCost[{ start.x, start.y, 4 }] = 0;
    static constexpr int dx[] = { 1, -1, 0, 0 };
    static constexpr int dy[] = { 0, 0, 1, -1 };
    int foundIdx = -1;
    while (! pq.empty()) {
        auto cur = pq.top(); pq.pop();
        StateKey curKey { cur.x, cur.y, cur.d };
        if (cur.cost > minCost[curKey]) continue;
        int idx = (int) visited.size();
        visited.push_back ({ { cur.x, cur.y }, cur.parentIdx });
        if (cur.x == end.x && cur.y == end.y) { foundIdx = idx; break; }
        for (int nd = 0; nd < 4; ++nd) {
            int nx = cur.x + dx[nd]; int ny = cur.y + dy[nd];
            if (! isValidNode (nx, ny)) continue;
            if (isEdgeBlocked (cur.x, cur.y, nx, ny)) continue;
            int moveCost = 10;
            if (cur.d != 4 && cur.d != nd) {
                if ((cur.d == 0 && nd == 1) || (cur.d == 1 && nd == 0) ||
                    (cur.d == 2 && nd == 3) || (cur.d == 3 && nd == 2)) continue;
                moveCost += 100;
            }
            int nextCost = cur.cost + moveCost;
            StateKey nextKey { nx, ny, nd };
            if (minCost.find (nextKey) == minCost.end() || nextCost < minCost[nextKey]) {
                minCost[nextKey] = nextCost; pq.push ({ nx, ny, nd, nextCost, idx });
            }
        }
    }
    std::vector<Cell> path;
    if (foundIdx != -1) {
        int curr = foundIdx;
        while (curr != -1) { path.push_back (visited[curr].cell); curr = visited[curr].parent; }
        std::reverse (path.begin(), path.end());
    }
    return path;
}
int main() {
    auto path = findPath({0, 4}, {2, 4}, {{2, 3, 2, 2}}, 16, 8);
    std::cout << "Path to Pedal IN: " << path.size() << "\n";
    for(auto c : path) std::cout << "(" << c.x << "," << c.y << ") ";
    std::cout << "\n";

    auto path2 = findPath({4, 4}, {16, 4}, {{2, 3, 2, 2}}, 16, 8);
    std::cout << "Path to Main OUT: " << path2.size() << "\n";
    for(auto c : path2) std::cout << "(" << c.x << "," << c.y << ") ";
    std::cout << "\n";
}
