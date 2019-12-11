#include <iostream>
#include <cmath>
#include "MyStrategy.hpp"
#include "Util.hpp"
#include "StrategyGenerator.hpp"

MyStrategy::MyStrategy() {}


UnitAction MyStrategy::getAction(const Unit& unit, const Game& game,
                                 Debug& debug) {
    if (simulation) {
        simulation->game.currentTick = game.currentTick;
        simulation->bullets = game.bullets;
    } else {
        simulation = std::make_shared<Simulation>(Simulation(game, unit.playerId, debug, ColorFloat(1.0, 0.0, 0.0, 0.5), true, true, false));
    }
    if (nextUnit) {
//        if (!areSame(unit.position.x, nextUnit->position.x, 1e-2) || !areSame(unit.position.y, nextUnit->position.y, 1e-2)) {
//            std::cerr << "Unit position x: " << unit.position.x << ", simulation pos x: " << nextUnit->position.x << '\n';
//            std::cerr << "Unit position y: " << unit.position.y << ", simulation pos y: " << nextUnit->position.y << '\n';
//            std::cerr << "Unit jumpState: " << unit.jumpState.toString() << ", simulation jumpState: " << nextUnit->jumpState.toString() << '\n';
//        }
//        if (unit.health != nextUnit->health) {
//            std::cerr << "Unit health: " << unit.health << ", simulation health: " << nextUnit->health << '\n';
//        }
//        if (unit.weapon && nextUnit->weapon) {
//            if (!areSame(unit.weapon->spread, nextUnit->weapon->spread, 1e-3) || !areSame(*(unit.weapon->fireTimer), *(nextUnit->weapon->fireTimer), 1e-9)) {
//                std::cerr << "Unit spread: " << unit.weapon->spread << ", simulation spread: " << nextUnit->weapon->spread << '\n';
//                std::cerr << "Unit fireTimer: " << *(unit.weapon->fireTimer) << ", simulation fireTimer: " << *(nextUnit->weapon->fireTimer) << '\n';
//            }
//        }
    }

    const Unit* nearestEnemy = nullptr;
    for (const Unit& other : game.units) {
        if (other.playerId != unit.playerId) {
            if (nearestEnemy == nullptr ||
                distanceSqr(unit.position, other.position) <
                distanceSqr(unit.position, nearestEnemy->position)) {
                nearestEnemy = &other;
            }
        }
    }
    const LootBox* nearestWeapon = nullptr;
    for (const LootBox& lootBox : game.lootBoxes) {
        if (std::dynamic_pointer_cast<Item::Weapon>(lootBox.item)) {
            if (nearestWeapon == nullptr ||
                distanceSqr(unit.position, lootBox.position) <
                distanceSqr(unit.position, nearestWeapon->position)) {
                if (std::dynamic_pointer_cast<Item::Weapon>(lootBox.item)->weaponType != ROCKET_LAUNCHER) {
                    nearestWeapon = &lootBox;
                }
            }
        }
    }

    const LootBox* nearestHealthPack = nullptr;
    for (const LootBox& lootBox : game.lootBoxes) {
        if (std::dynamic_pointer_cast<Item::HealthPack>(lootBox.item)) {
            if (nearestHealthPack == nullptr ||
                distanceSqr(unit.position, lootBox.position) <
                distanceSqr(unit.position, nearestHealthPack->position)) {
                nearestHealthPack = &lootBox;
            }
        }
    }

    Vec2Double targetPos = unit.position;
    double targetImportance = 1.0;
    if ((!unit.weapon || unit.weapon->typ == ROCKET_LAUNCHER) && nearestWeapon != nullptr) {
        targetPos = nearestWeapon->position;
    } else if (nearestHealthPack != nullptr && unit.health <= 90.0) {
        targetPos = nearestHealthPack->position;
        if (unit.health <= 90.0) {
            targetImportance = 10.0;
        }
        if (unit.health <= 80.0) {
            targetImportance = 50.0;
        }
        if (unit.health <= 60.0) {
            targetImportance = 100.0;
        }
    } else if (nearestEnemy != nullptr) {
        double desiredDistance = (nearestEnemy->weapon && nearestEnemy->weapon->typ == ROCKET_LAUNCHER) ? 49.0 : 9.0;
        if (distanceSqr(unit.position, nearestEnemy->position) > desiredDistance) {
            targetPos = Vec2Double(nearestEnemy->position.x, nearestEnemy->position.y + nearestEnemy->size.y);
        } else {
            targetPos = unit.position.x > nearestEnemy->position.x ? Vec2Double(50.0, 50.0) : Vec2Double(0.0, 50.0);
        }
    }
    debug.draw(CustomData::Log(
        std::string("Target pos: ") + targetPos.toString()));
    Vec2Double aim = Vec2Double(0, 0);
    if (nearestEnemy != nullptr && unit.weapon) {
        aim = predictShootAngle(unit, *nearestEnemy, game, debug);
    }
    bool jump = targetPos.y > unit.position.y;
    if (targetPos.x > unit.position.x &&
        game.level.tiles[size_t(unit.position.x + 1)][size_t(unit.position.y)] ==
        Tile::WALL) {
        jump = true;
    }
    if (targetPos.x < unit.position.x &&
        game.level.tiles[size_t(unit.position.x - 1)][size_t(unit.position.y)] ==
        Tile::WALL) {
        jump = true;
    }
    bool jumpDown = !jump;
    if (jump && unit.jumpState.maxTime < 0.3 &&
        game.level.tiles[size_t(unit.position.x)][size_t(unit.position.y - 1)] == Tile::PLATFORM) {
        jump = false;
    }
    double vel = targetPos.x - unit.position.x;

    if (vel > 0) {
        vel = 100.0;
    } else {
        vel = -100.0;
    }
    if (fabs(targetPos.x - unit.position.x) < 0.1) {
        vel = 0.0;
    }
    UnitAction action;
    action.velocity = vel;
    action.jump = jump;
    action.jumpDown = jumpDown;
    action.aim = aim;
    action.shoot = true;
    action.reload = false;
    action.swapWeapon = unit.weapon && unit.weapon->typ == ROCKET_LAUNCHER;
    action.plantMine = false;

    debug.draw(CustomData::Log(
        std::string("Unit pos: ") + unit.position.toString()));
    debug.draw(CustomData::Log(
        std::string("Jump state: ") + unit.jumpState.toString()));
    if (unit.weapon) {
        debug.draw(CustomData::Log(
            std::string("Weapon: ") + unit.weapon->toString()));
    }

    std::unordered_map<int, Unit> units;
    for (const Unit& u : game.units) {
        units[u.id] = u;
    }

//    nextUnit = simulation->simulate(action, units, unit.id)[unit.id];
//    prevAction = action;

    if (nearestEnemy != nullptr) {
        action.shoot = shouldShoot(unit, nearestEnemy->id, aim, game, debug);
    }

    auto bestAction = avoidBullets(unit, game, targetPos, targetImportance, action, debug);
    if (bestAction) {
        action.velocity = bestAction->velocity;
        action.jump = bestAction->jump;
        action.jumpDown = bestAction->jumpDown;
    }

    std::cerr << "ACTUAL ACTION: " << action.toString() << "\n";

    return action;
}

Vec2Double MyStrategy::predictShootAngle(const Unit& unit, const Unit& enemyUnit, const Game& game, Debug& debug) {
    Simulation sim(game, unit.playerId, debug, ColorFloat(1.0, 1.0, 1.0, 0.3), true, false, false, 3);

    Vec2Double enemyPosition = enemyUnit.position;
    if (enemyUnit.jumpState.canCancel || !areSame(enemyUnit.jumpState.maxTime, 0.0)) {
        return Vec2Double(enemyPosition.x - unit.position.x,
                          enemyPosition.y - unit.position.y);
    }

    double speed = unit.weapon->params.bullet.speed;

    std::unordered_map<int, UnitAction> params;
    int simulationMaxTicks = 40;
    const auto& actions = StrategyGenerator::getActions(simulationMaxTicks, 0, false, false);

    if (unit.weapon && unit.weapon->fireTimer) {
        for (int ticks = 0; ticks < *(unit.weapon->fireTimer) * 60.0; ++ticks) {
            params[enemyUnit.id] = actions[ticks];
            sim.simulate(params);
            enemyPosition = sim.units[enemyUnit.id].position;
        }
    }

    for (int ticks = 0; ticks < simulationMaxTicks; ++ticks) {
        double distSqr = distanceSqr(enemyPosition, unit.position);
        double bulletDist = speed * ticks / 60.0;
        params[enemyUnit.id] = actions[ticks];
        sim.simulate(params);
        enemyPosition = sim.units[enemyUnit.id].position;
        if (distSqr < bulletDist * bulletDist) {
            break;
        }
    }
    return Vec2Double(enemyPosition.x - unit.position.x,
                      enemyPosition.y - unit.position.y);
}

std::optional<UnitAction> MyStrategy::avoidBullets(const Unit& unit,
                                                   const Game& game,
                                                   const Vec2Double& targetPos,
                                                   double targetImportance,
                                                   const UnitAction& targetAction,
                                                   Debug& debug) {
    std::cerr << "Current tick: " << game.currentTick << "\n";
    int actionTicks = 30;
    std::vector<std::vector<UnitAction>> actionSets = {
        StrategyGenerator::getActions(actionTicks, 1, true, false),
        StrategyGenerator::getActions(actionTicks, 0, true, false),
        StrategyGenerator::getActions(actionTicks, -1, true, false),
        StrategyGenerator::getActions(actionTicks, 1, false, true),
        StrategyGenerator::getActions(actionTicks, 0, false, true),
        StrategyGenerator::getActions(actionTicks, -1, false, true),
        StrategyGenerator::getActions(actionTicks, 1, false, false),
        StrategyGenerator::getActions(actionTicks, 0, false, true),
        StrategyGenerator::getActions(actionTicks, -1, false, false)
    };

    std::vector<std::vector<UnitAction>> enemyActionSets = {
        StrategyGenerator::getActions(actionTicks, 1, true, false),
        StrategyGenerator::getActions(actionTicks, -1, true, false),
        StrategyGenerator::getActions(actionTicks, 1, false, true),
        StrategyGenerator::getActions(actionTicks, -1, false, true)
    };

    std::optional<UnitAction> bestAction;
    int enemyUnitId = -1;
    for (const Unit& u : game.units) {
        if (u.playerId != unit.playerId) {
            enemyUnitId = u.id;
        }
    }

    std::unordered_map<int, UnitAction> params;
    std::vector<ColorFloat> colors = {
        ColorFloat(1.0, 0.0, 0.0, 0.3),
        ColorFloat(0.0, 1.0, 0.0, 0.3),
        ColorFloat(0.0, 0.0, 1.0, 0.3),
        ColorFloat(1.0, 1.0, 0.0, 0.3)
    };

    int colorIndex = 0;
    int bestEnemyActionIndex = 0;
    std::shared_ptr<Simulation> bestEnemySim = nullptr;
    for (auto& actionSet : enemyActionSets) {
        Simulation sim(game, unit.playerId, debug, colors[colorIndex], true, true, true, 20);
        for (int i = 0; i < actionTicks; ++i) {
            auto myAction = StrategyGenerator::getActions(1, 0, false, false)[0];
            StrategyGenerator::updateAction(sim.units, unit.id, myAction);
            StrategyGenerator::updateAction(sim.units, enemyUnitId, actionSet[i]);
            params[unit.id] = myAction;
            params[enemyUnitId] = actionSet[i];
            sim.simulate(params);
        }
        if (bestEnemySim == nullptr || compareSimulations(sim, *bestEnemySim, actionSet[0], enemyActionSets[bestEnemyActionIndex][0], game,
                                                          unit, actionTicks, unit.position, 0.0, targetAction) < 0) {
            if (bestEnemySim == nullptr) {
                std::cerr << "First simulation\n";
                for (const auto& event : sim.events) {
                    std::cerr << event.toString() << '\n';
                }
                std::cerr << '\n';
            }
            bestEnemySim = std::make_shared<Simulation>(sim);
            bestEnemyActionIndex = colorIndex;
        }
        ++colorIndex;
    }

    std::cerr << "Finish enemy simulation!!!\n";

    colorIndex = 0;
    bool noEvents = true;
    std::shared_ptr<Simulation> bestSim = nullptr;
    for (auto& actionSet : actionSets) {
        Simulation sim(game, unit.playerId, debug, colors[colorIndex], true, true, true, 20);
        for (int i = 0; i < actionTicks; ++i) {
            StrategyGenerator::updateAction(sim.units, unit.id, actionSet[i]);
            StrategyGenerator::updateAction(sim.units, enemyUnitId, enemyActionSets[bestEnemyActionIndex][i]);
            params[unit.id] = actionSet[i];
            params[enemyUnitId] = enemyActionSets[bestEnemyActionIndex][i];
            sim.simulate(params);
        }
//        debug.draw(CustomData::Rect(
//            Vec2Float(sim.units[unit.id].position.x - sim.units[unit.id].size.x / 2, sim.units[unit.id].position.y),
//            Vec2Float(sim.units[unit.id].size.x, sim.units[unit.id].size.y),
//            colors[colorIndex]
//        ));
        if (bestSim == nullptr || compareSimulations(sim, *bestSim, actionSet[0], *bestAction, game,
                                                     unit, actionTicks, targetPos, targetImportance, targetAction) > 0) {
            if (bestSim == nullptr) {
                std::cerr << "First simulation\n";
                for (const auto& event : sim.events) {
                    std::cerr << event.toString() << '\n';
                }
                std::cerr << '\n';
            }
            bestSim = std::make_shared<Simulation>(sim);
            bestAction = actionSet[0];
        }

        if (!sim.events.empty()) {
            noEvents = false;
        }
        ++colorIndex;
    }

    if (noEvents) {
        return std::nullopt;
    }
    if (bestAction) {
        std::cerr << "Best action: " << bestAction->toString() << "\n";
    }
    return bestAction;
}

bool MyStrategy::shouldShoot(Unit unit, int enemyUnitId, Vec2Double aim, const Game& game, Debug& debug) {
    if (!unit.weapon) {
        return false;
    }
    if (unit.weapon->fireTimer && *(unit.weapon->fireTimer) > 1 / game.properties.ticksPerSecond) {
        return false;
    }

    double aimAngle = atan2(aim.y, aim.x);
    int angleStepsRange = 0;
    int angleStepsCount = 2 * angleStepsRange + 1;

    std::vector<Damage> damages;
    for (int i = -angleStepsRange; i <= angleStepsRange; ++i) {
        Simulation sim(game, unit.playerId, debug, ColorFloat(0.0, 1.0, 1.0, 0.5), true, true, false, 5);

        auto bulletPos = unit.position;
        bulletPos.y += unit.size.y / 2;

        double angle = aimAngle + unit.weapon->spread * i / 10;
        auto bulletVel = Vec2Double(cos(angle) * unit.weapon->params.bullet.speed,
                                    sin(angle) * unit.weapon->params.bullet.speed);
        sim.bullets = {Bullet(
            unit.weapon->typ,
            unit.id,
            unit.playerId,
            bulletPos,
            bulletVel,
            unit.weapon->params.bullet.damage,
            unit.weapon->params.bullet.size,
            unit.weapon->params.explosion
        )};
        std::unordered_map<int, UnitAction> params;
        auto myAction = StrategyGenerator::getActions(1, 0, false, false)[0];
        for (int j = 0; j < 30; ++j) {
            StrategyGenerator::updateAction(sim.units, enemyUnitId, myAction);
            params[enemyUnitId] = myAction;
            sim.simulate(params);
            if (sim.bullets.empty()) {
                break;
            }
        }
        Damage damage{};
        for (const Unit& u : game.units) {
            if (u.id == unit.id) {
                damage.me = u.health - sim.units[u.id].health;
            } else {
                damage.enemy = u.health - sim.units[u.id].health;
            }
        }
        damages.push_back(damage);
    }
    double damageMeCount = 0;
    double damageEnemyCount = 0;
    for (const Damage& dmg : damages) {
        if (dmg.me > 0) {
            ++damageMeCount;
        }
        if (dmg.enemy > 0) {
            ++damageEnemyCount;
        }
    }
    std::cerr << "damageMeCount: " << damageMeCount << ", P: " << std::to_string(damageMeCount / angleStepsCount) << '\n';
    std::cerr << "damageEnemyCount: " << damageEnemyCount << ", P: " << std::to_string(damageEnemyCount / angleStepsCount) << '\n';
    if (damageEnemyCount == 0) {
        return false;
    }
//    if ((double(damageEnemyCount) / angleStepsCount) < 0.1) {
//        return false;
//    }
    return damageMeCount / angleStepsCount < 0.05 || damageEnemyCount > 0;
}

int MyStrategy::compareSimulations(const Simulation& sim1, const Simulation& sim2,
                                   const UnitAction& action1, const UnitAction& action2,
                                   const Game& game, const Unit& unit, int actionTicks,
                                   const Vec2Double& targetPos, double targetImportance,
                                   const UnitAction& targetAction) {
    int unitId = unit.id;
    int indexSim1 = 0;
    int indexSim2 = 0;

    auto events1 = sim1.events;
    auto events2 = sim2.events;

//    while (indexSim1 < events1.size() && indexSim2 < events2.size()) {
//        if (events1[indexSim1].tick > events2[indexSim2].tick) {
//            if (events1[indexSim1].tick - events2[indexSim2].tick == 1) {
//                events1[indexSim1].tick = events2[indexSim2].tick;
//            }
//            ++indexSim1;
//        } else {
//            if (events2[indexSim2].tick - events1[indexSim1].tick == 1) {
//                events2[indexSim2].tick = events1[indexSim1].tick;
//            }
//            ++indexSim2;
//        }
//    }

    for (const auto& event : events1) {
        std::cerr << event.toString() << '\n';
    }

    double score1 = 0.0;
    for (const auto& event : events1) {
        double timeMultiplier = (actionTicks + 1.0 - event.tick) / actionTicks;
        if (targetImportance > 1.1) {
            timeMultiplier *= timeMultiplier;
        }
        double eventScore = event.damage * timeMultiplier;
        if (!event.real) {
            double prob = event.probability;
//            for (int i = 0; i < event.shootTick; ++i) {
//                prob *= 0.95;
//            }
            eventScore *= std::min(prob, 1.0);
        }
        if (event.unitId == unitId) {
            eventScore = -eventScore;
        }
        score1 += eventScore;
    }

    std::cerr << "score: " << score1 << '\n';

    double score2 = 0.0;
    for (const auto& event : events2) {
        double timeMultiplier = (actionTicks + 1.0 - event.tick) / actionTicks;
        if (targetImportance > 1.1) {
            timeMultiplier *= timeMultiplier;
        }
        double eventScore = event.damage * timeMultiplier;
        if (!event.real) {
            double prob = event.probability;
//            for (int i = 0; i < event.shootTick; ++i) {
//                prob *= 0.95;
//            }
            eventScore *= std::min(prob, 1.0);
        }
        if (event.unitId == unitId) {
            eventScore = -eventScore;
        }
        score2 += eventScore;
    }

    std::cerr << "best score: " << score2 << '\n';

    if (!areSame(score1, score2, (targetImportance > 1.1) ? 2.0 : 1e-2)) {
        if (score1 > score2) {
            return 1;
        } else {
            return -1;
        }
    }

    std::cerr << "Scores are same" << '\n';
    std::cerr << "targetAction: " << targetAction.toString() << '\n';

    if (areSame(action1.velocity, targetAction.velocity) && !areSame(action2.velocity, targetAction.velocity)) {
        std::cerr << "WIN. Right direction\n";
        return 1;
    }
    if (!areSame(action1.velocity, targetAction.velocity) && areSame(action2.velocity, targetAction.velocity)) {
        std::cerr << "LOSE. Wrong direction\n";
        return -1;
    }

    if (action1.jump == targetAction.jump && action2.jump != targetAction.jump) {
        std::cerr << "WIN. Right jump\n";
        return 1;
    }
    if (action1.jump != targetAction.jump && action2.jump == targetAction.jump) {
        std::cerr << "LOSE. Wrong jump\n";
        return -1;
    }

    if (action1.jumpDown == targetAction.jumpDown && action2.jumpDown != targetAction.jumpDown) {
        std::cerr << "WIN. Right jumpDown\n";
        return 1;
    }
    if (action1.jumpDown != targetAction.jumpDown && action2.jumpDown == targetAction.jumpDown) {
        std::cerr << "LOSE. Wrong jumpDown\n";
        return -1;
    }

    std::cerr << "No target position decision. Choose by score\n";
    if (score1 > score2) {
        return 1;
    } else {
        return -1;
    }
//    double targetDistance1 = distanceSqr(sim1.units.at(unitId).position, sim1.units.at(enemyId).position);
//    double targetDistance2 = distanceSqr(sim2.units.at(unitId).position, sim2.units.at(enemyId).position);
//    std::cerr << "target distance: " << targetDistance1 << '\n';
//    std::cerr << "best target distance: " << targetDistance2 << '\n';
//    if (targetDistance1 < targetDistance2) {
//        return 1;
//    } else {
//        return -1;
//    }
}

