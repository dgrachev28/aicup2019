#include <iostream>
#include <cmath>
#include <chrono>
#include "MyStrategy.hpp"
#include "Util.hpp"
#include "StrategyGenerator.hpp"

std::unordered_map<std::string, int> MyStrategy::PERF;

MyStrategy::MyStrategy() {
    for (auto& p: paths) {
        p.fill(10000);
    }
    isPathFilled.fill(false);
    pathsBuilt = false;
}

UnitAction MyStrategy::getAction(const Unit& unit, const Game& game, Debug& debug) {
        if (!pathsBuilt) {
        auto t1 = std::chrono::high_resolution_clock::now();
        buildPathGraph(unit, game, debug);
        if (MyStrategy::PERF.find("buildPathGraph") == MyStrategy::PERF.end()) {
            MyStrategy::PERF["buildPathGraph"] = 0;
        }
        MyStrategy::PERF["buildPathGraph"] += std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - t1).count();
        t1 = std::chrono::high_resolution_clock::now();
        floydWarshall();
        if (MyStrategy::PERF.find("floydWarshall") == MyStrategy::PERF.end()) {
            MyStrategy::PERF["floydWarshall"] = 0;
        }
        MyStrategy::PERF["floydWarshall"] += std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - t1).count();
        pathsBuilt = true;

        int isPathFilledCount = 0;
        for (bool isFilled : isPathFilled) {
            if (isFilled) {
                ++isPathFilledCount;
            }
        }
        std::cerr << "PATHS SIZE:" << isPathFilledCount << '\n';
    }

    if (isPathFilled[getPathsIndex(unit.position)] && game.currentTick != pathDrawLastTick) {
        for (int i = 0; i < 1200; ++i) {
            if (isPathFilled[i]) {
                Vec2Double tile = fromPathsIndex(i);
                debug.draw(CustomData::Rect(
                    Vec2Float(tile.x - 0.4, tile.y + 0.4),
                    Vec2Float(0.8, 0.8),
                    ColorFloat(0.0, 1.0, 0.0, 1.0 - std::max(0.05, 1.0 - paths[getPathsIndex(unit.position)][i] / 200.0))
                ));
            }
        }
        pathDrawLastTick = game.currentTick;
    }

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

    std::vector<const Unit*> myUnits;
    std::vector<const Unit*> enemyUnits;
    for (const Unit& other : game.units) {
        if (other.playerId == unit.playerId) {
            myUnits.push_back(&other);
        } else {
            enemyUnits.push_back(&other);
        }
    }

    const Unit* nearestEnemy = nullptr;
    double minDistance = 10000000.0;
    for (const Unit* enemy : enemyUnits) {
        Vec2Double simSrcPosition;
        double distance = calculatePathDistance(unit.position, enemy->position, unit, game, debug, simSrcPosition);
        if (distance < minDistance) {
            minDistance = distance;
            nearestEnemy = enemy;
        }
    }

    double targetImportance;
    const auto& targetPos = findTargetPosition(unit, nearestEnemy, game, debug, targetImportance);

    debug.draw(CustomData::Log(
        std::string("Target pos: ") + targetPos.toString()));
    Vec2Double aim = Vec2Double(0, 0);
    if (nearestEnemy != nullptr && unit.weapon) {
        aim = predictShootAngle2(unit, *nearestEnemy, game, debug);
    }

    UnitAction action;
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
        action.shoot = shouldShoot(unit, *nearestEnemy, aim, game, debug);
        auto bestAction = avoidBullets(unit, nearestEnemy->id, game, targetPos, targetImportance, action, debug);
        if (bestAction) {
            action.velocity = bestAction->velocity;
            action.jump = bestAction->jump;
            action.jumpDown = bestAction->jumpDown;
        }
    }

    std::cerr << "ACTUAL ACTION: " << action.toString() << "\n";

    return action;
}

Vec2Double MyStrategy::predictShootAngle2(const Unit& unit, const Unit& enemyUnit, const Game& game, Debug& debug, bool simulateFallDown) {
    Simulation sim(game, unit.playerId, debug, ColorFloat(1.0, 1.0, 1.0, 0.3), true, false, false, 1);
    double lastAngle = *(unit.weapon->lastAngle);

    Vec2Double aim;
    double targetAngle;
    bool fastMoveAngle;
    Vec2Double enemyPosition = enemyUnit.position;
    if (!simulateFallDown || enemyUnit.jumpState.canCancel) {
        aim = Vec2Double(enemyPosition.x - unit.position.x,
                         enemyPosition.y - unit.position.y);
    } else {
        double speed = unit.weapon->params.bullet.speed;

        std::unordered_map<int, UnitAction> params;
        int simulationMaxTicks = 30;
        const auto& actions = StrategyGenerator::getActions(simulationMaxTicks, 0, false, false);

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
    }
    double xDiff = enemyPosition.x - unit.position.x;
    double yDiff = enemyPosition.y - unit.position.y;

    std::vector<double> enemyUnitAngles = {
        atan2(yDiff + unit.size.y / 2, xDiff + unit.size.x / 2),
        atan2(yDiff + unit.size.y / 2, xDiff - unit.size.x / 2),
        atan2(yDiff - unit.size.y / 2, xDiff + unit.size.x / 2),
        atan2(yDiff - unit.size.y / 2, xDiff - unit.size.x / 2)
    };

    double minEnemyAngle = enemyUnitAngles[0];
    double maxEnemyAngle = enemyUnitAngles[0];

    for (double angle : enemyUnitAngles) {
        minEnemyAngle = minAngle(angle, minEnemyAngle);
        maxEnemyAngle = maxAngle(angle, maxEnemyAngle);
    }

    double minSpread = sumAngles(lastAngle, -unit.weapon->spread);
    double maxSpread = sumAngles(lastAngle, unit.weapon->spread);


    double hitProb;
    if ((isLessAngle(minEnemyAngle, minSpread) && isGreaterAngle(maxEnemyAngle, maxSpread)) ||
        (isLessAngle(minSpread, minEnemyAngle) && isGreaterAngle(maxSpread, maxEnemyAngle))) {
        hitProb = 1.0;
    } else if (isLessAngle(minEnemyAngle, minSpread) && isLessAngle(maxEnemyAngle, maxSpread)) {
        hitProb = findAngle(minSpread, maxEnemyAngle) / findAngle(minSpread, maxSpread);
        targetAngle = sumAngles(minEnemyAngle, findAngle(minEnemyAngle, maxSpread) / 2);
    } else {
        hitProb = findAngle(minEnemyAngle, maxSpread) / findAngle(minSpread, maxSpread);
        targetAngle = sumAngles(maxEnemyAngle, -findAngle(minSpread, maxEnemyAngle) / 2);
    }

    if (isLessAngle(maxSpread, minEnemyAngle)) {
        hitProb = 0.0;
        targetAngle = sumAngles(minSpread, findAngle(minSpread, maxEnemyAngle) / 2);
    } else if (isLessAngle(maxEnemyAngle, minSpread)) {
        hitProb = 0.0;
        targetAngle = sumAngles(minEnemyAngle, findAngle(minEnemyAngle, maxSpread) / 2);
    }

    bool dontMove = hitProb > 0.85;

    aim = Vec2Double(enemyPosition.x - unit.position.x,
                     enemyPosition.y - unit.position.y);
    fastMoveAngle = !(unit.weapon->fireTimer && unit.weapon->fireTimer > 1. / 60);
    if (findAngle(atan2(aim.y, aim.x), lastAngle) > unit.weapon->params.maxSpread * 1.2) {
        targetAngle = atan2(aim.y, aim.x);
        fastMoveAngle = true;
    } else if (areSame(unit.weapon->spread, unit.weapon->params.minSpread)) {
        targetAngle = atan2(aim.y, aim.x);
    } else if (dontMove) {
        return Vec2Double(cos(lastAngle), sin(lastAngle));
    }

//    double anglesAbs = fabs(targetAngle - lastAngle);
//    double step = std::max(2.0, 57.2957 * anglesAbs / 10) * unit.weapon->params.recoil / 60;
    double step = unit.weapon->params.recoil / 60;
    double finalTargetAngle;
    if (fastMoveAngle) {
        finalTargetAngle = targetAngle;
    } else if (isLessAngle(lastAngle, targetAngle)) {
        finalTargetAngle = lastAngle + step;
    } else {
        finalTargetAngle = lastAngle - step;
    }

    return Vec2Double(cos(finalTargetAngle), sin(finalTargetAngle));
}

std::optional<UnitAction> MyStrategy::avoidBullets(const Unit& unit,
                                                   int enemyUnitId,
                                                   const Game& game,
                                                   const Vec2Double& targetPos,
                                                   double targetImportance,
                                                   const UnitAction& targetAction,
                                                   Debug& debug) {
    std::cerr << "Current tick: " << game.currentTick << "\n";
    std::cerr << "Current unit: " << unit.id << "\n";
    std::cerr << "Target position: (" << targetPos.x << ", " << targetPos.y << ")\n";
    int actionTicks = 45;
    std::vector<std::vector<UnitAction>> actionSets;
    if (!unit.jumpState.canJump && areSame(unit.jumpState.maxTime, 0.0)) {
        actionSets = {
            StrategyGenerator::getActions(actionTicks, 1, false, false),
            StrategyGenerator::getActions(actionTicks, 0, false, false),
            StrategyGenerator::getActions(actionTicks, -1, false, false),
            StrategyGenerator::getActions(actionTicks, 1, false, true),
            StrategyGenerator::getActions(actionTicks, 0, false, true),
            StrategyGenerator::getActions(actionTicks, -1, false, true)
        };
    } else {
        actionSets = {
            StrategyGenerator::getActions(actionTicks, 1, true, false),
            StrategyGenerator::getActions(actionTicks, 0, true, false),
            StrategyGenerator::getActions(actionTicks, -1, true, false),
            StrategyGenerator::getActions(actionTicks, 1, false, false),
            StrategyGenerator::getActions(actionTicks, 0, false, false),
            StrategyGenerator::getActions(actionTicks, -1, false, false),
            StrategyGenerator::getActions(actionTicks, 1, false, true),
            StrategyGenerator::getActions(actionTicks, 0, false, true),
            StrategyGenerator::getActions(actionTicks, -1, false, true)
        };
    }

    std::optional<UnitAction> bestAction;
//
//    std::vector<std::vector<UnitAction>> enemyActionSets;
    std::vector<int> enemyUnitIds;
    for (const Unit& u : game.units) {
        if (u.playerId != unit.playerId) {
            enemyUnitIds.push_back(u.id);
//            std::vector<UnitAction> actions = {
//                StrategyGenerator::getActions(1, 1, true, false)[0],
//                StrategyGenerator::getActions(1, -1, true, false)[0],
//                StrategyGenerator::getActions(1, 1, false, true)[0],
//                StrategyGenerator::getActions(1, -1, false, true)[0]
//            };
//            enemyActionSets.push_back(actions);
        }
    }

    std::unordered_map<int, UnitAction> params;

//    std::vector<int> bestEnemyActionIndex = {0, 0};
//
//    for (int enemyIdx = 0; enemyIdx < enemyUnitIds.size(); ++enemyIdx) {
//        int colorIndex = 0;
//        std::shared_ptr<Simulation> bestEnemySim = nullptr;
//        for (auto& actionSet : enemyActionSets[enemyIdx]) {
//            Simulation sim(game, unit.playerId, debug, ColorFloat(1.0, 0.0, 0.0, 0.3), true, true, true, 5);
//            for (int i = 0; i < actionTicks; ++i) {
//                auto myAction = StrategyGenerator::getActions(1, 0, false, false)[0];
//                updateAction(sim.units, unit.id, myAction, game, debug);
//                updateAction(sim.units, enemyUnitIds[enemyIdx], actionSet, game, debug);
//                params[unit.id] = myAction;
//                params[enemyUnitIds[enemyIdx]] = actionSet;
//                sim.simulate(params);
//            }
//            if (bestEnemySim == nullptr || compareSimulations(sim, *bestEnemySim, actionSet, enemyActionSets[enemyIdx][bestEnemyActionIndex[enemyIdx]],
//                                                              0.0, 0.0, game,
//                                                              unit, actionTicks, unit.position, 0.0, targetAction) < 0) {
//                bestEnemySim = std::make_shared<Simulation>(sim);
//                bestEnemyActionIndex[enemyIdx] = colorIndex;
//            }
//            ++colorIndex;
//        }
//
//        std::cerr << "=========Best enemy action: " << enemyActionSets[enemyIdx][bestEnemyActionIndex[enemyIdx]].toString() << '\n';
//    }

    auto defaultAction = StrategyGenerator::getActions(1, 0, false, false)[0];
    bool noEvents = true;
    double bestTargetDistance = 0.0;
    std::shared_ptr<Simulation> bestSim = nullptr;
    for (auto& actionSet : actionSets) {
        double targetDistance = 0.0;
        Simulation sim(game, unit.playerId, debug, ColorFloat(1.0, 0.0, 0.0, 0.3), true, true, true, 10);
        for (int i = 0; i < actionTicks; ++i) {
            updateAction(sim.units, unit.id, enemyUnitId, actionSet[i], sim.game, debug);
            params[unit.id] = actionSet[i];

            for (int j = 0; j < enemyUnitIds.size(); ++j) {
//                if (i < 3) {
//                    updateAction(sim.units, enemyUnitIds[j], enemyActionSets[j][bestEnemyActionIndex[j]], sim.game, debug);
//                    params[enemyUnitIds[j]] = enemyActionSets[j][bestEnemyActionIndex[j]];
//                } else {
                    updateAction(sim.units, enemyUnitIds[j], unit.id, defaultAction, sim.game, debug);
                    params[enemyUnitIds[j]] = defaultAction;
//                }
            }

            if (i == 6) {
                Vec2Double simSrcPosition;
                targetDistance = calculatePathDistance(sim.units[unit.id].position, targetPos, sim.units[unit.id], sim.game, debug, simSrcPosition);
                if (getPathsIndex(simSrcPosition) == getPathsIndex(unit.position)) {
                    targetDistance += 6;
                }
            }
            int microticks = i < 20 ? 5 : 1;
            sim.simulate(params, i == 0 ? 50 : microticks);
        }

//        debug.draw(CustomData::Rect(
//            Vec2Float(sim.units[unit.id].position.x - sim.units[unit.id].size.x / 2, sim.units[unit.id].position.y),
//            Vec2Float(sim.units[unit.id].size.x, sim.units[unit.id].size.y),
//            colors[colorIndex]
//        ));

        std::cerr << "Consider action: " << actionSet[0].toString() << '\n';
        for (const auto& event : sim.events) {
            std::cerr << event.toString() << '\n';
        }
        if (bestSim == nullptr ||
            compareSimulations(sim, *bestSim, actionSet[0], *bestAction, targetDistance, bestTargetDistance,
                               game, unit, actionTicks, targetPos, targetImportance, targetAction) > 0) {
            bestSim = std::make_shared<Simulation>(sim);
            bestAction = actionSet[0];
            bestTargetDistance = targetDistance;
        }

        if (!sim.events.empty()) {
            noEvents = false;
        }
    }

    std::cerr << "========= FINISHED CHOOSE DIRECTION. Best action: " << bestAction->toString() << "\n";

//    int firstChainActionTicks = 2;
//
//    const auto& tailChainActions = StrategyGenerator::getActions(
//        actionTicks - firstChainActionTicks,
//        bestAction->velocity / 10,
//        bestAction->jump,
//        bestAction->jumpDown
//    );
//
//    std::vector<std::vector<UnitAction>> chainedActionSets = {
//        StrategyGenerator::getActions(firstChainActionTicks, 1, true, false, tailChainActions),
//        StrategyGenerator::getActions(firstChainActionTicks, 0, true, false, tailChainActions),
//        StrategyGenerator::getActions(firstChainActionTicks, -1, true, false, tailChainActions),
//        StrategyGenerator::getActions(firstChainActionTicks, 1, false, false, tailChainActions),
//        StrategyGenerator::getActions(firstChainActionTicks, 0, false, false, tailChainActions),
//        StrategyGenerator::getActions(firstChainActionTicks, -1, false, false, tailChainActions),
//        StrategyGenerator::getActions(firstChainActionTicks, 1, false, true, tailChainActions),
//        StrategyGenerator::getActions(firstChainActionTicks, 0, false, true, tailChainActions),
//        StrategyGenerator::getActions(firstChainActionTicks, -1, false, true, tailChainActions)
//    };
//
//    for (auto& actionSet : chainedActionSets) {
//        double targetDistance = 0.0;
//        Simulation sim(game, unit.playerId, debug, ColorFloat(1.0, 0.0, 0.0, 0.3), true, true, true, 10);
//        for (int i = 0; i < actionTicks; ++i) {
//            updateAction(sim.units, unit.id, actionSet[i], sim.game, debug);
//            params[unit.id] = actionSet[i];
//
//            for (int j = 0; j < enemyUnitIds.size(); ++j) {
//                if (i < 3) {
//                    updateAction(sim.units, enemyUnitIds[j], enemyActionSets[j][bestEnemyActionIndex[j]], sim.game, debug);
//                    params[enemyUnitIds[j]] = enemyActionSets[j][bestEnemyActionIndex[j]];
//                } else {
//                    updateAction(sim.units, enemyUnitIds[j], defaultAction, sim.game, debug);
//                    params[enemyUnitIds[j]] = defaultAction;
//                }
//            }
//            int microticks = i < 20 ? 5 : 1;
//            sim.simulate(params, i == 0 ? 50 : microticks);
//            if (i == 10) {
//                targetDistance = calculatePathDistance(sim.units[unit.id].position, targetPos, sim.units[unit.id], sim.game, debug);
//            }
//        }
//
//        std::cerr << "Consider action (chained): " << actionSet[0].toString() << '\n';
//        for (const auto& event : sim.events) {
//            std::cerr << event.toString() << '\n';
//        }
//        if (bestSim == nullptr || compareSimulations(sim, *bestSim, actionSet[0], *bestAction, targetDistance, bestTargetDistance, game,
//                                                     unit, actionTicks, targetPos, 0.3, tailChainActions[0]) > 0) {
//            bestSim = std::make_shared<Simulation>(sim);
//            bestAction = actionSet[0];
//            bestTargetDistance = targetDistance;
//        }
//
//        if (!sim.events.empty()) {
//            noEvents = false;
//        }
//    }


//    if (noEvents) {
//        return std::nullopt;
//    }
    if (bestAction) {
        std::cerr << "Best action: " << bestAction->toString() << "\n";
    }
    return bestAction;
}

bool MyStrategy::shouldShoot(Unit unit, const Unit& enemyUnit, Vec2Double aim, const Game& game, Debug& debug) {
    if (!unit.weapon) {
        return false;
    }
    if (unit.weapon->fireTimer && *(unit.weapon->fireTimer) > 1 / game.properties.ticksPerSecond) {
        return false;
    }

    auto hitProbabilities = calculateHitProbability(unit, enemyUnit, game, debug);
    for (const Unit& u : game.units) {
        std::cerr << "unit id: " << u.id << ", hit probability: " << hitProbabilities[u.id] << '\n';
        if (u.playerId == unit.playerId) {
            if (hitProbabilities[u.id] > 0.09) {
                return false;
            }
        } else {
            if (unit.weapon->typ == ASSAULT_RIFLE && hitProbabilities[u.id] > 0.0) {
                return true;
            }
            if (unit.weapon->typ != ASSAULT_RIFLE && hitProbabilities[u.id] > 0.14) {
                return true;
            }
        }
    }
    return false;
}

int MyStrategy::compareSimulations(const Simulation& sim1, const Simulation& sim2,
                                   const UnitAction& action1, const UnitAction& action2,
                                   double targetDistance1, double targetDistance2,
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

    double score1 = 0.0;
    for (const auto& event : events1) {
        double timeMultiplier = 1.0;
        for (int i = 0; i < event.tick; ++i) {
            timeMultiplier *= 0.9;
        }
        double eventScore = event.damage * timeMultiplier;

        if (eventScore < 0 && event.unitId == unit.id) {
            eventScore = -std::min(100 - unit.health, 50.0);
        } else if (!event.real) {
            double prob = event.probability;
//            for (int i = 0; i < event.shootTick; ++i) {
//                if (event.damage == 30 || event.damage == 50) {
//                    prob *= 0.85;
//                } else {
//                    prob *= 0.93;
//                }
//            }
            eventScore *= std::min(prob, 1.0);
        }
        if (event.unitId == unitId) {
            eventScore = -1.2 * eventScore;
        } else if (sim1.units.at(event.unitId).playerId == unit.playerId) {
            eventScore = 0;
        }
        score1 += eventScore;
    }

    std::cerr << "score: " << score1 << '\n';

    double score2 = 0.0;
    for (const auto& event : events2) {
        double timeMultiplier = 1.0;
        for (int i = 0; i < event.tick; ++i) {
            timeMultiplier *= 0.9;
        }
        double eventScore = event.damage * timeMultiplier;

        if (eventScore < 0 && event.unitId == unit.id) {
            eventScore = -std::min(100 - unit.health, 50.0);
        } else if (!event.real) {
            double prob = event.probability;
//            for (int i = 0; i < event.shootTick; ++i) {
//                if (event.damage == 30 || event.damage == 50) {
//                    prob *= 0.85;
//                } else {
//                    prob *= 0.93;
//                }
//            }
            eventScore *= std::min(prob, 1.0);
        }
        if (event.unitId == unitId) {
            eventScore = -1.2 * eventScore;
        } else if (sim2.units.at(event.unitId).playerId == unit.playerId) {
            eventScore = 0;
        }
        score2 += eventScore;
    }

    std::cerr << "best score: " << score2 << '\n';

    double distDiffScore = (targetDistance1 - targetDistance2) / 6 * targetImportance;
    std::cerr << "Dist diff score: " << distDiffScore << '\n';
//    if (!areSame(score1, score2, targetImportance)) {
    if (score1 - distDiffScore > score2) {
        return 1;
    } else {
        return -1;
    }
}

void MyStrategy::buildPathGraph(const Unit& unit, const Game& game, Debug& debug) {
    std::vector<UnitAction> actions = {
        StrategyGenerator::getActions(1, 1, true, false)[0],
        StrategyGenerator::getActions(1, 0.5, true, false)[0],
        StrategyGenerator::getActions(1, 0.25, true, false)[0],
        StrategyGenerator::getActions(1, 0.1, true, false)[0],
        StrategyGenerator::getActions(1, 0, true, false)[0],
        StrategyGenerator::getActions(1, -0.1, true, false)[0],
        StrategyGenerator::getActions(1, -0.25, true, false)[0],
        StrategyGenerator::getActions(1, -0.5, true, false)[0],
        StrategyGenerator::getActions(1, -1, true, false)[0],
        StrategyGenerator::getActions(1, 1, false, true)[0],
        StrategyGenerator::getActions(1, 0.25, false, true)[0],
        StrategyGenerator::getActions(1, 0.1, false, true)[0],
        StrategyGenerator::getActions(1, 0, false, true)[0],
        StrategyGenerator::getActions(1, -0.1, false, true)[0],
        StrategyGenerator::getActions(1, -0.25, false, true)[0],
        StrategyGenerator::getActions(1, -1, false, true)[0],
        StrategyGenerator::getActions(1, 1, false, false)[0],
        StrategyGenerator::getActions(1, -1, false, false)[0]
    };
    pathDfs(int(unit.position.x), int(unit.position.y), actions, unit, game, debug);
}

void MyStrategy::pathDfs(int x, int y, const std::vector<UnitAction>& actions, const Unit& unit, const Game& game, Debug& debug) {
    Vec2Double pos(x, y);
    if (isPathFilled[getPathsIndex(pos)]) {
        return;
    }
    isPathFilled[getPathsIndex(pos)] = true;
    for (const auto& action : actions) {
        Simulation sim(game, unit.playerId, debug, ColorFloat(1.0, 1.0, 1.0, 0.3), true, false, false, 1);
        sim.units[unit.id].position.x = x + 0.5;
        sim.units[unit.id].position.y = y;
        std::unordered_map<int, Unit> units;
        for (const auto& [id, u] : sim.units) {
            if (id == unit.id) {
                units[id] = u;
                break;
            }
        }
        sim.units = units;

        std::unordered_map<int, UnitAction> params;
        for (int tick = 1; tick < 200; ++tick) {
            params[unit.id] = action;
            sim.simulate(params);
            Vec2Double simPosition = sim.units[unit.id].position;
            if ((simPosition.y - int(simPosition.y) < game.properties.unitFallSpeed / 60 + 1e-5 ||
                 game.level.tiles[int(simPosition.x)][int(simPosition.y)] == JUMP_PAD) &&
                 !areSame(sim.units[unit.id].jumpState.speed, 20.0) &&
                (int(simPosition.y) != y || int(simPosition.x) != x)) {
                if (game.level.tiles[int(simPosition.x)][int(simPosition.y)] != PLATFORM &&
                    (game.level.tiles[int(simPosition.x)][int(simPosition.y)] != EMPTY ||
                     (game.level.tiles[int(simPosition.x)][int(simPosition.y - 1)] != EMPTY &&
                      game.level.tiles[int(simPosition.x)][int(simPosition.y - 1)] != JUMP_PAD))) {

                    double restTime = fabs(int(simPosition.x) + 0.5 - simPosition.x) * 6;
                    int posIdx = getPathsIndex(pos);
                    int simPosIdx = getPathsIndex(simPosition);
                    if (tick + std::round(restTime) < paths[posIdx][simPosIdx]) {
                        paths[posIdx][simPosIdx] = tick + std::round(restTime);
                        pathDfs(int(simPosition.x), int(simPosition.y), actions, unit, game, debug);
                    }
                    break;
                }
            }
        }
    }
}

void MyStrategy::floydWarshall() {
    for (int k = 0; k < 1200; ++k) {
        paths[k][k] = 0;
    }
    for (int k = 0; k < 1200; ++k) {
        if (!isPathFilled[k]) {
            continue;
        }
        std::cerr << k << ' ';
        for (int i = 0; i < 1200; ++i) {
            if (isPathFilled[i]) {
                for (int j = 0; j < 1200; ++j) {
                    if (isPathFilled[j]) {
                        paths[i][j] = std::min<int16_t>(paths[i][j], paths[i][k] + paths[k][j]);
                    }
                }
            }
        }
    }
}

int MyStrategy::getPathsIndex(const Vec2Double& vec) {
    return int(vec.y) * 40 + int(vec.x);
}

Vec2Double MyStrategy::fromPathsIndex(int idx) {
    return Vec2Double(idx % 40 + 0.5, idx / 40);
}

Vec2Double MyStrategy::findTargetPosition(const Unit& unit, const Unit* nearestEnemy, const Game& game, Debug& debug, double& targetImportance) {
    if (unitTargetWeapons.find(unit.id) == unitTargetWeapons.end()) {
        unitTargetWeapons[unit.id] = std::nullopt;
    }

    std::vector<LootBox> healthPacks;
    std::vector<LootBox> weapons;
    std::vector<double> myHPDistance;
    std::vector<double> enemyHPDistance;
    for (const LootBox& lootBox : game.lootBoxes) {
        if (std::dynamic_pointer_cast<Item::HealthPack>(lootBox.item)) {
            Vec2Double simSrcPosition;
            double myDistance = calculatePathDistance(unit.position, lootBox.position, unit, game, debug, simSrcPosition);
            double enemyDistance = calculatePathDistance(nearestEnemy->position, lootBox.position, *nearestEnemy, game, debug, simSrcPosition);
            healthPacks.push_back(lootBox);
            myHPDistance.push_back(myDistance);
            enemyHPDistance.push_back(enemyDistance);
        } else if (std::dynamic_pointer_cast<Item::Weapon>(lootBox.item)) {
            weapons.push_back(lootBox);
        }
    }

    if (game.currentTick % 10 == 0) {
        for (const Unit& u : game.units) {
            if (u.playerId != unit.playerId) {
                continue;
            }
            if (u.weapon && u.weapon->typ != ROCKET_LAUNCHER) {
                unitTargetWeapons[u.id] = std::nullopt;
                continue;
            }
            double minDistance = 10000.0;
            for (const auto& weapon : weapons) {
                if (std::dynamic_pointer_cast<Item::Weapon>(weapon.item)->weaponType == ROCKET_LAUNCHER) {
                    continue;
                }
                bool skip = false;
                for (const auto&[uid, targetWeapon] : unitTargetWeapons) {
                    if (targetWeapon && areSame(targetWeapon->position.x, weapon.position.x) &&
                        areSame(targetWeapon->position.y, weapon.position.y) && uid != u.id) {
                        skip = true;
                    }
                }
                if (skip) {
                    continue;
                }

                Vec2Double simSrcPosition;
                double distance = calculatePathDistance(u.position, weapon.position, u, game, debug, simSrcPosition);
                if (distance < minDistance) {
                    minDistance = distance;
                    unitTargetWeapons[u.id] = weapon;
                }
            }
        }
    }

    Vec2Double targetPos = unit.position;
    targetImportance = 0.2;
    if (!unit.weapon || unit.weapon->typ == ROCKET_LAUNCHER) {
        targetPos = unitTargetWeapons[unit.id]->position;
        targetImportance = 2.0;
    } else if (!healthPacks.empty() && unit.health <= 90.0) {
        LootBox* bestHealthPack = nullptr;
        double minDistance = 10000.0;
        for (int i = 0; i < healthPacks.size(); ++i) {
            std::cerr << "Iteration " << i << ", myHPDistance: " << myHPDistance[i] << '\n';
            std::cerr << "Iteration " << i << ", enemyHPDistance: " << enemyHPDistance[i] << '\n';
            if (myHPDistance[i] < enemyHPDistance[i] && myHPDistance[i] < minDistance) {
                minDistance = myHPDistance[i];
                bestHealthPack = &healthPacks[i];
            }
        }
        std::cerr << "min distance: " << minDistance << '\n';
        double minDistanceDiff = 10000.0;
        if (bestHealthPack == nullptr) {
            for (int i = 0; i < healthPacks.size(); ++i) {
                double distanceDiff = myHPDistance[i] - enemyHPDistance[i];
                if (distanceDiff < minDistanceDiff) {
                    minDistanceDiff = distanceDiff;
                    bestHealthPack = &healthPacks[i];
                }
            }
        }

        std::cerr << "min distance diff: " << minDistance << '\n';

        targetPos = bestHealthPack->position;
        targetImportance = 2.0;
    } else if (!healthPacks.empty()) {

        int bestTileIndex = 0;
        int bestWinHealthPackPathNum = 0;
        double minHealthPackDistanceSum = 200000.0;

        for (int key = 0; key < 1200; ++key) {
            if (!isPathFilled[key]) {
                continue;
            }
            double sum = 0.0;
            int winHealthPackPathNum = 0;

            for (int i = 0; i < healthPacks.size(); ++i) {
                double myDistance = paths[key][getPathsIndex(healthPacks[i].position)];
                if (myDistance < enemyHPDistance[i]) {
                    ++winHealthPackPathNum;
                } else {
                    --winHealthPackPathNum;
                }
//                sum += paths[key][getPathsIndex(healthPacks[i].position)];
            }
            sum = distanceSqr(fromPathsIndex(key), nearestEnemy->position);
            if (winHealthPackPathNum > bestWinHealthPackPathNum ||
                (winHealthPackPathNum == bestWinHealthPackPathNum && sum < minHealthPackDistanceSum)) {
                bestWinHealthPackPathNum = winHealthPackPathNum;
                minHealthPackDistanceSum = sum;
                bestTileIndex = key;
            }
        }
        targetPos = fromPathsIndex(bestTileIndex);
        std::cerr << "Best win healthpacks num: " << bestWinHealthPackPathNum << '\n';
        std::cerr << "Target position from paths: " << targetPos.toString() << '\n';
    } else if (nearestEnemy != nullptr) {
        double desiredDistance = (nearestEnemy->weapon && nearestEnemy->weapon->typ == ROCKET_LAUNCHER) ? 81.0 : 16.0;
        if (distanceSqr(unit.position, nearestEnemy->position) > desiredDistance) {
            targetPos = Vec2Double(nearestEnemy->position.x, nearestEnemy->position.y + nearestEnemy->size.y / 2);
        } else {
            targetPos = unit.position.x > nearestEnemy->position.x ? Vec2Double(50.0, 50.0) : Vec2Double(0.0, 50.0);
        }
    }
    return targetPos;
}

Vec2Double MyStrategy::findNearestTile(const Vec2Double& tile) {
    int x = int(tile.x);
    int y = int(tile.y);
    for (int radius = 1; radius < 6; ++radius) {
        for (int i = -radius; i <= radius; ++i) {
            for (int j = -radius; j <= radius; ++j) {
                if (isPathFilled[getPathsIndex(Vec2Double(x + i, y + j))]) {
                    return Vec2Double(x + i, y + j);
                }
            }
        }
    }
    std::cerr << "ERROR WITH PATH FINDING\n";
    return tile;
}

double MyStrategy::calculatePathDistance(const Vec2Double& src, const Vec2Double& dst,
                                         const Unit& unit, const Game& game, Debug& debug, Vec2Double& simSrcPosision) {
    int srcIdx = getPathsIndex(src);
    int dstIdx = getPathsIndex(dst);
    if (!isPathFilled[dstIdx]) {
        dstIdx = getPathsIndex(findNearestTile(dst));
    }
    if (isPathFilled[srcIdx]) {
        simSrcPosision = src;
        return paths[srcIdx][dstIdx];
    }

    std::vector<UnitAction> actions = {
        StrategyGenerator::getActions(1, 1, true, false)[0],
        StrategyGenerator::getActions(1, 0.5, true, false)[0],
        StrategyGenerator::getActions(1, 0.25, true, false)[0],
        StrategyGenerator::getActions(1, 0.1, true, false)[0],
        StrategyGenerator::getActions(1, 0, true, false)[0],
        StrategyGenerator::getActions(1, -0.1, true, false)[0],
        StrategyGenerator::getActions(1, -0.25, true, false)[0],
        StrategyGenerator::getActions(1, -0.5, true, false)[0],
        StrategyGenerator::getActions(1, -1, true, false)[0],
        StrategyGenerator::getActions(1, 1, false, true)[0],
        StrategyGenerator::getActions(1, 0.25, false, true)[0],
        StrategyGenerator::getActions(1, 0, false, true)[0],
        StrategyGenerator::getActions(1, -0.25, false, true)[0],
        StrategyGenerator::getActions(1, -1, false, true)[0],
        StrategyGenerator::getActions(1, 1, false, false)[0],
        StrategyGenerator::getActions(1, -1, false, false)[0]
    };

    double minPathDistance = 1000.0;

    for (const auto& action : actions) {
        Simulation sim(game, unit.playerId, debug, ColorFloat(1.0, 1.0, 1.0, 0.3), true, false, false, 1);
        sim.units[unit.id] = unit;

        std::unordered_map<int, UnitAction> params;
        for (int tick = 1; tick < 200; ++tick) {
            params[unit.id] = action;
            sim.simulate(params);
            Vec2Double simPosition = sim.units[unit.id].position;
            if ((simPosition.y - int(simPosition.y) < game.properties.unitFallSpeed / 60 + 1e-5 ||
                 game.level.tiles[int(simPosition.x)][int(simPosition.y)] == JUMP_PAD) &&
                (int(simPosition.y) != unit.position.y || int(simPosition.x) != unit.position.x)) {
                if (isPathFilled[getPathsIndex(simPosition)]) {
                    double restTime = fabs(int(simPosition.x) + 0.5 - simPosition.x) * 6;
                    double pathDistance = tick + std::round(restTime) + paths[getPathsIndex(simPosition)][dstIdx];
                    if (pathDistance < minPathDistance) {
                        simSrcPosision = simPosition;
                        minPathDistance = pathDistance;
                    }
                    if (!areSame(unit.jumpState.speed, 20.0)) {
                        break;
                    }
                }
            }
        }
    }

    return minPathDistance;
}

void MyStrategy::updateAction(const std::unordered_map<int, Unit>& units, int unitId, int enemyUnitId, UnitAction& action,
                              const Game& game, Debug& debug) {
    auto t1 = std::chrono::high_resolution_clock::now();
    const Unit& unit = units.at(unitId);

    action.aim = predictShootAngle2(unit, units.at(enemyUnitId), game, debug, false);
    if (MyStrategy::PERF.find("updateAction") == MyStrategy::PERF.end()) {
        MyStrategy::PERF["updateAction"] = 0;
    }
    MyStrategy::PERF["updateAction"] += std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - t1).count();
}

std::unordered_map<int, double> MyStrategy::calculateHitProbability(
    const Unit& unit,
    const Unit& enemyUnit,
    const Game& game,
    Debug& debug
) {
    int actionTicks = 25;
    std::vector<std::vector<UnitAction>> enemyActionSets = {
        StrategyGenerator::getActions(actionTicks, 1, true, false, false),
        StrategyGenerator::getActions(actionTicks, -1, true, false, false),
        StrategyGenerator::getActions(actionTicks, 1, false, true, false),
        StrategyGenerator::getActions(actionTicks, -1, false, true, false)
    };

    const auto& tailActions = StrategyGenerator::getActions(actionTicks - 2, 0, false, false, false);
    auto myActions = StrategyGenerator::getActions(2, 0, false, false, tailActions);

    std::vector<std::unordered_map<int, std::vector<bool>>> bulletHits;
    std::vector<std::vector<DamageEvent>> events;
    for (auto& enemyActionSet : enemyActionSets) {
        Simulation sim(game, unit.playerId, debug, ColorFloat(1.0, 0.0, 0.0, 0.3), true, true, true, 1, true);
//        sim.bullets = std::vector<Bullet>();

        std::unordered_map<int, UnitAction> params;
        for (int i = 0; i < actionTicks; ++i) {
            if (myActions[i].shoot) {
                updateAction(sim.units, unit.id, enemyUnit.id, myActions[i], sim.game, debug);
            }
            params[unit.id] = myActions[i];
            if (i == 0) {
                params[enemyUnit.id] = StrategyGenerator::getActions(1, 0, false, false, false)[0];
            } else {
                params[enemyUnit.id] = enemyActionSet[i];
            }
            sim.simulate(params, i == 0 ? 10 : 1);
            if (sim.bullets.empty()) {
                break;
            }
        }

        bulletHits.push_back(sim.bulletHits);
        events.push_back(sim.events);
    }

    addRealBulletHits(events, unit.weapon->params.bullet.damage, bulletHits);
    return calculateHitProbability(bulletHits);
}


void MyStrategy::addRealBulletHits(const std::vector<std::vector<DamageEvent>>& events,
                                   int bulletDamage,
                                   std::vector<std::unordered_map<int, std::vector<bool>>>& bulletHits) {
    std::unordered_map<int, double> minUnitDamage;
    std::vector<std::unordered_map<int, double>> unitDamage;
    for (const auto& simEvents : events) {
        std::unordered_map<int, double> damage;
        for (const DamageEvent& event : simEvents) {
            if (damage.find(event.unitId) == damage.end()) {
                damage[event.unitId] = event.damage;
            } else {
                damage[event.unitId] += event.damage;
            }
        }

        if (minUnitDamage.empty()) {
            minUnitDamage = damage;
        } else {
            for (const auto&[unitId, dmg] : damage) {
                if (dmg < minUnitDamage[unitId]) {
                    minUnitDamage[unitId] = dmg;
                }
            }
        }
        unitDamage.push_back(damage);
    }

    for (int i = 0; i < unitDamage.size(); ++i) {
        for (const auto&[unitId, dmg] : unitDamage[i]) {
            if (dmg - minUnitDamage[unitId] >= bulletDamage) {
                std::fill(bulletHits[i][unitId].begin(), bulletHits[i][unitId].end(), true);
            }
        }
    }
}

std::unordered_map<int, double> MyStrategy::calculateHitProbability(
    const std::vector<std::unordered_map<int, std::vector<bool>>>& bulletHits
) {
    std::unordered_map<int, std::vector<bool>> hitsIntersection = bulletHits[0];
    int hitsSize = 0;

    for (int i = 1; i < bulletHits.size(); ++i) {
        for (const auto&[unitId, hits] : bulletHits[i]) {
            hitsSize = hits.size();
            for (int j = 0; j < hitsSize; ++j) {
                hitsIntersection[unitId][j] = hitsIntersection[unitId][j] && hits[j];
            }
        }
    }

    std::unordered_map<int, double> hitProbabilities;
    for (const auto&[unitId, hits] : hitsIntersection) {
        int hitsCount = 0;
        for (bool isHit : hits) {
            if (isHit) {
                ++hitsCount;
            }
        }
        hitProbabilities[unitId] = double(hitsCount) / hitsSize;
    }
    return hitProbabilities;
}

