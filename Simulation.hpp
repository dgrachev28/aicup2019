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

    void simulate(const UnitAction& action, int unitId);

private:
    void move(const UnitAction& action, int unitId);
    void moveX(const UnitAction& action, int unitId);
    void moveY(const UnitAction& action, int unitId);
    void fallDown(const UnitAction& action, int unitId);

    void simulateBullets();

    void explode(const Bullet& bullet,
                 std::optional<int> unitId);

    Unit simulateShoot(const UnitAction& action, Unit unit);

public:
    Game game;
    std::vector<Bullet> bullets;
    std::unordered_map<int, Unit> units;
private:
    int microTicks;

    bool simMove;
    bool simBullets;
    bool simShoot;
};


#endif
