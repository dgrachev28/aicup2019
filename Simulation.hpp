#ifndef _SIMULATION_HPP_
#define _SIMULATION_HPP_


#include "model/Game.hpp"
#include "model/UnitAction.hpp"
#include "Debug.hpp"
#include "Util.hpp"

class Simulation {
public:
    explicit Simulation(
        Game game,
        int myPlayerId,
        Debug& debug,
        ColorFloat color = ColorFloat(1.0, 0.0, 0.0, 0.5),
        bool simMove = true,
        bool simBullets = true,
        bool simShoot = true,
        int microTicks = 100
    );

    void simulate(std::unordered_map<int, UnitAction> actions, std::optional<int> microTicks = std::nullopt);

private:
    void move(const UnitAction& action, int unitId);
    void moveX(const UnitAction& action, int unitId);
    void moveY(const UnitAction& action, int unitId);
    void fallDown(const UnitAction& action, int unitId);

    void simulateBullets();

    void explode(const Bullet& bullet, std::optional<int> unitId);

    double calculateHitProbability(const Bullet& bullet, const Unit& targetUnit);

    void simulateShoot(const UnitAction& action, int unitId);

    void createBullets(const UnitAction& action, int unitId, const Rect& targetUnit);

public:
    Game game;
    Debug& debug;
    std::vector<DamageEvent> events;
    ColorFloat color;
    std::vector<Bullet> bullets;
    std::unordered_map<int, Unit> units;
private:
    int startTick;
    int microTicks;
    double ticksMultiplier;

    bool simMove;
    bool simBullets;
    bool simShoot;
    int myPlayerId;
};

#endif
