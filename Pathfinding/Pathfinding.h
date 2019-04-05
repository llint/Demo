//
//  Astar.h
//  Demo
//
//  Created by Lin Luo on 10/31/15.
//
//

#ifndef Pathfinding_h
#define Pathfinding_h

#include <array>
#include <tuple>
#include <queue>
#include <deque>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct Location
{
    size_t x, y;

    Location(size_t x_ = 0, size_t y_ = 0) : x(x_), y(y_) {}
    Location(const Location& rhs) : x(rhs.x), y(rhs.y) {}

    Location& operator=(const Location& rhs)
    {
        x = rhs.x;
        y = rhs.y;
        return *this;
    }

    bool operator==(const Location& rhs) const
    {
        return x == rhs.x && y == rhs.y;
    }

    bool operator!=(const Location& rhs) const
    {
        return !(*this == rhs);
    }

    bool operator<(const Location& rhs) const
    {
        return (x < rhs.x) || (x == rhs.x ? y < rhs.y : false);
    }
};

namespace std
{
    template <>
    struct hash<Location>
    {
        std::size_t operator()(const Location& k) const
        {
            return k.x ^ k.y;
        }
    };
}

static const Location DIRS[4] = { {1, 0}, {0, (size_t)-1}, {(size_t)-1, 0}, {0, 1} };

template <size_t N>
class SquareGrid
{
public:
    using Weight = uint8_t;

    SquareGrid()
    {
        Reset();
    }

    SquareGrid(const std::unordered_map<Location, Weight>& weights)
    {
        Reset();

        SetWeights(weights);
    }

    void Reset()
    {
        for (size_t y = 0; y < N; ++y)
        {
            for (size_t x = 0; x < N; ++x)
            {
                m_grid[y][x] = 1; // NB: default cost
            }
        }
    }

    void SetWeights(const std::unordered_map<Location, Weight>& weights)
    {
        for (const auto& each : weights)
        {
            if ( valid(each.first) && each.second > 0 )
            {
                m_grid[each.first.y][each.first.x] = each.second;
            }
        }
    }

    Weight GetWeight(Location p) const
    {
        return valid(p) ? m_grid[p.y][p.x] : 0;
    }

    void SetWeight(Location p, Weight weight)
    {
        if ( valid(p) && weight > 0 )
        {
            m_grid[p.y][p.x] = weight;
        }
    }

    std::deque<Location> ComputePath(Location start, Location goal) const
    {
        if ( !valid(start) || !valid(goal) )
        {
            return std::deque<Location>();
        }

        std::unordered_map<Location, Location> came_from;
        std::unordered_map<Location, size_t> cost_so_far;

        using Element = std::pair<size_t, Location>;
        std::priority_queue< Element, std::vector<Element>, std::greater<Element> > frontier;
        frontier.emplace(0, start);

        came_from[start] = start;
        cost_so_far[start] = 0;

        while ( !frontier.empty() )
        {
            auto current = frontier.top().second;
            frontier.pop();

            if (current == goal) {
                break;
            }

            for (const auto& next : neighbors(current))
            {
                size_t new_cost = cost_so_far[current] + GetWeight(next);
                if (!cost_so_far.count(next) || new_cost < cost_so_far[next])
                {
                    cost_so_far[next] = new_cost;
                    size_t priority = new_cost + heuristic(next, goal);
                    frontier.emplace(priority, next);
                    came_from[next] = current;
                }
            }
        }

        std::deque<Location> path;
        Location current = goal;
        path.push_front(current);
        while (current != start)
        {
            current = came_from[current];
            path.push_front(current);
        }
        return path;
    }

private:
    static bool valid(Location p)
    {
        return p.x < N && p.y < N;
    }

    static std::vector<Location> neighbors(Location p)
    {
        std::vector<Location> results;

        for (auto d : DIRS) {
            Location next(p.x + d.x, p.y + d.y);
            if ( valid(next) )
            {
                results.push_back(next);
            }
        }

        if ((p.x + p.y) % 2 == 0) {
            // aesthetic improvement on square grids
            std::reverse(results.begin(), results.end());
        }

        return results;
    }
    
    static size_t heuristic(Location a, Location b)
    {
        return (a.x > b.x ? a.x - b.x : b.x - a.x) + (a.y > b.y ? a.y - b.y : b.y - a.y);
    }

    std::array< std::array<uint8_t, N>, N > m_grid;
};


#endif /* Astar_h */
