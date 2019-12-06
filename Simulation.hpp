#ifndef _SIMULATION_HPP_
#define _SIMULATION_HPP_


#include "model/Game.hpp"
#include "model/UnitAction.hpp"

class Simulation {
public:
    explicit Simulation(
        Game game,
        bool simMove = true,
        bool simBullets = true,
        bool simShoot = true,
        int microTicks = 100
    );

    std::unordered_map<int, Unit> simulate(const UnitAction& action, std::unordered_map<int, Unit> units, int unitId);

private:
    Unit move(const UnitAction& action, Unit unit);
    Unit moveX(const UnitAction& action, Unit unit);
    Unit moveY(const UnitAction& action, Unit unit);
    Unit fallDown(const UnitAction& action, Unit unit);

    std::unordered_map<int, Unit> simulateBullets(std::unordered_map<int, Unit> units);

    std::unordered_map<int, Unit> explode(const Bullet& bullet,
                                          std::unordered_map<int, Unit> units,
                                          std::optional<int> unitId);

    Unit simulateShoot(const UnitAction& action, Unit unit);

public:
    Game game;
    std::vector<Bullet> bullets;
private:
    int microTicks;

    bool simMove;
    bool simBullets;
    bool simShoot;
};


#endif
