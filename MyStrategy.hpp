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

    std::optional<UnitAction> avoidBullets(const Unit& unit, const Game& game, Debug& debug);

    bool shouldShoot(Unit unit, Vec2Double aim, const Game& game, Debug& debug);

private:
    std::shared_ptr<Simulation> simulation;
    std::optional<Unit> nextUnit;
    std::optional<UnitAction> prevAction;
};

struct Damage {
    int me;
    int enemy;
};

#endif