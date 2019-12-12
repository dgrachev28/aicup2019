#ifndef _MY_STRATEGY_HPP_
#define _MY_STRATEGY_HPP_

#include "Debug.hpp"
#include "model/CustomData.hpp"
#include "model/Game.hpp"
#include "model/Unit.hpp"
#include "model/UnitAction.hpp"
#include "Simulation.hpp"

class MyStrategy {
public:
    MyStrategy();

    UnitAction getAction(const Unit& unit, const Game& game, Debug& debug);

    Vec2Double predictShootAngle(const Unit& unit, const Unit& enemyUnit, const Game& game, Debug& debug);

    std::optional<UnitAction> avoidBullets(
        const Unit& unit,
        const Game& game,
        const Vec2Double& targetPos,
        double targetImportance,
        const UnitAction& targetAction,
        Debug& debug
    );

    bool shouldShoot(Unit unit, int enemyUnitId, Vec2Double aim, const Game& game, Debug& debug);

    int compareSimulations(
        const Simulation& sim1,
        const Simulation& sim2,
        const UnitAction& action1,
        const UnitAction& action2,
        const Game& game,
        const Unit& unit,
        int actionTicks,
        const Vec2Double& targetPos,
        double targetImportance,
        const UnitAction& targetAction
    );

    void buildPathGraph(const Unit& unit, const Game& game, Debug& debug);

    void pathDfs(int x, int y, const std::vector<UnitAction>& actions, const Unit& unit, const Game& game, Debug& debug);

    void floydWarshall();

    int getPathsIndex(const Vec2Double& vec);

    Vec2Double fromPathsIndex(int idx);

    Vec2Double findTargetPosition(const Unit& unit, const Unit* nearestEnemy, const Game& game, Debug& debug, double& targetImportance);

    double calculatePathDistance(const Vec2Double& src, const Vec2Double& dst, const Unit& unit, const Game& game, Debug& debug);

private:
    std::shared_ptr<Simulation> simulation;
    std::optional<Unit> nextUnit;
    std::optional<UnitAction> prevAction;
    std::unordered_map<int16_t, std::unordered_map<int16_t, int16_t>> paths = std::unordered_map<int16_t, std::unordered_map<int16_t, int16_t>>();
};

struct Damage {
    double me;
    double enemy;
};

#endif