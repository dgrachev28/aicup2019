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

private:
    std::shared_ptr<Simulation> simulation;
    std::optional<Unit> nextUnit;
    std::optional<UnitAction> prevAction;
};

struct Damage {
    double me;
    double enemy;
};

#endif