#include <iostream>
#include <cmath>
#include "MyStrategy.hpp"
#include "Util.hpp"
#include "StrategyGenerator.hpp"

MyStrategy::MyStrategy() {}


UnitAction MyStrategy::getAction(const Unit& unit, const Game& game,
                                 Debug& debug) {

    if (game.currentTick < 10) {
        UnitAction action;
        action.velocity = 0.0;
        action.jump = false;
        action.jumpDown = !action.jump;
        action.aim = Vec2Double(0.0, 0.0);
        action.shoot = true;
        action.reload = false;
        action.swapWeapon = false;
        action.plantMine = false;
        return action;
    }
    if (simulation) {
        simulation->game.currentTick = game.currentTick;
        simulation->bullets = game.bullets;
    } else {
        simulation = std::make_shared<Simulation>(Simulation(game, debug, ColorFloat(1.0, 0.0, 0.0, 0.5), true, true, false));
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
    if (!unit.weapon && nearestWeapon != nullptr) {
        targetPos = nearestWeapon->position;
    } else if (nearestHealthPack != nullptr && unit.health <= 90.0
               && !((unit.position.x < nearestEnemy->position.x && nearestEnemy->position.x < nearestHealthPack->position.x)
               || (unit.position.x > nearestEnemy->position.x && nearestEnemy->position.x > nearestHealthPack->position.x))) {
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
        double desiredDistance = (nearestEnemy->weapon && nearestEnemy->weapon->typ == ROCKET_LAUNCHER) ? 81.0 : 9.0;
        if (distanceSqr(unit.position, nearestEnemy->position) > desiredDistance) {
            targetPos = nearestEnemy->position;
        } else {
            targetPos = unit.position.x > nearestEnemy->position.x ? Vec2Double(50.0, 50.0) : Vec2Double(0.0, 50.0);
        }
    }
    debug.draw(CustomData::Log(
        std::string("Target pos: ") + targetPos.toString()));
    Vec2Double aim = Vec2Double(0, 0);
    if (nearestEnemy != nullptr) {
        double aimY = nearestEnemy->position.y;
//        if (!nearestEnemy->jumpState.canJump && nearestEnemy->jumpState.maxTime < 1e-9) {
//            aimY -= 0.3 * nearestEnemy->size.y;
//        } else {
//            aimY += 0.3 * nearestEnemy->size.y;
//        }
        aim = Vec2Double(nearestEnemy->position.x - unit.position.x,
                         aimY - unit.position.y);
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
    UnitAction action;
    action.velocity = (targetPos.x - unit.position.x) * 100;
    action.jump = jump;
    action.jumpDown = !action.jump;
    action.aim = aim;
    action.shoot = true;
    action.reload = false;
    action.swapWeapon = false;
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

    action.shoot = shouldShoot(unit, aim, game, debug);

    auto bestAction = avoidBullets(unit, game, targetPos, targetImportance, debug);
    if (bestAction) {
        action.velocity = bestAction->velocity;
        action.jump = bestAction->jump;
        action.jumpDown = bestAction->jumpDown;
    }

    return action;
}

std::optional<UnitAction> MyStrategy::avoidBullets(const Unit& unit, const Game& game, const Vec2Double& targetPos, double targetImportance, Debug& debug) {
    int actionTicks = 30;
    StrategyGenerator gen;
    std::vector<std::vector<UnitAction>> actionSets = {
        gen.getActions(actionTicks, 1, true, false),
        gen.getActions(actionTicks, -1, true, false),
        gen.getActions(actionTicks, 1, false, true),
        gen.getActions(actionTicks, -1, false, true)
    };

    std::vector<std::vector<UnitAction>> enemyActionSets = {
        gen.getActions(actionTicks, 0, false, false)
    };

    std::optional<UnitAction> bestAction;
    int enemyUnitId = -1;
    for (const Unit& u : game.units) {
        if (u.playerId != unit.playerId) {
            enemyUnitId = u.id;
        }
    }

    double bestScore = -1000;
    std::unordered_map<int, UnitAction> params;
    std::vector<ColorFloat> colors = {
        ColorFloat(1.0, 0.0, 0.0, 0.3),
        ColorFloat(0.0, 1.0, 0.0, 0.3),
        ColorFloat(0.0, 0.0, 1.0, 0.3),
        ColorFloat(1.0, 1.0, 0.0, 0.3)
    };
    int colorIndex = 0;
    bool noEvents = true;
    std::shared_ptr<Simulation> bestSim = nullptr;
    for (auto& actionSet : actionSets) {
        Simulation sim(game, debug, colors[colorIndex], true, true, true);
        for (int i = 0; i < actionTicks; ++i) {
            gen.updateAction(sim.units, unit.id, actionSet[i]);
            gen.updateAction(sim.units, enemyUnitId, enemyActionSets[0][i]);
            params[unit.id] = actionSet[i];
            params[enemyUnitId] = enemyActionSets[0][i];
            sim.simulate(params);
        }
//        debug.draw(CustomData::Rect(
//            Vec2Float(sim.units[unit.id].position.x - sim.units[unit.id].size.x / 2, sim.units[unit.id].position.y),
//            Vec2Float(sim.units[unit.id].size.x, sim.units[unit.id].size.y),
//            colors[colorIndex]
//        ));
        if (bestSim == nullptr || compareSimulations(sim, *bestSim, actionSet[0], *bestAction, game,
                                                     unit, actionTicks, targetPos, targetImportance) > 0) {
            bestSim = std::make_shared<Simulation>(sim);
            for (const auto& event : sim.events) {
                std::cerr << event.toString() << '\n';
            }
            bestAction = actionSet[0];
        }

        if (!sim.events.empty()) {
            noEvents = false;
        }
        ++colorIndex;
    }

    std::cerr << "Current tick: " << game.currentTick << "\n";
    if (noEvents) {
        return std::nullopt;
    }
    if (bestAction) {
        std::cerr << "Best action: " << bestAction->toString() << "\n";
        std::cerr << "Best score: " << bestScore << "\n";
    }
    return bestAction;
}

bool MyStrategy::shouldShoot(Unit unit, Vec2Double aim, const Game& game, Debug& debug) {
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
        Simulation sim(game, debug, ColorFloat(0.0, 1.0, 1.0, 0.5), false, true, false, 5);

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
        for (int j = 0; j < 30; ++j) {
            sim.simulate(std::unordered_map<int, UnitAction>());
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
    std::cerr << "damageMeCount: " << damageMeCount << ", P: " << std::to_string(double(damageMeCount) / angleStepsCount) << '\n';
    std::cerr << "damageEnemyCount: " << damageEnemyCount << ", P: " << std::to_string(double(damageEnemyCount) / angleStepsCount) << '\n';
    if (damageEnemyCount == 0) {
        return false;
    }
//    if ((double(damageEnemyCount) / angleStepsCount) < 0.1) {
//        return false;
//    }
    return double(damageMeCount) / angleStepsCount < 0.05 || damageEnemyCount > 0;
}

int MyStrategy::compareSimulations(const Simulation& sim1, const Simulation& sim2,
                                   const UnitAction& action1, const UnitAction& action2,
                                   const Game& game, const Unit& unit,
                                   int actionTicks, const Vec2Double& targetPos, double targetImportance) {
    int unitId = unit.id;
    int indexSim1 = 0;
    int indexSim2 = 0;

    auto events1 = sim1.events;
    auto events2 = sim2.events;

    while (indexSim1 < events1.size() && indexSim2 < events2.size()) {
        if (events1[indexSim1].tick > events2[indexSim2].tick) {
            if (events1[indexSim1].tick - events2[indexSim2].tick == 1) {
                events1[indexSim1].tick = events2[indexSim2].tick;
            }
            ++indexSim1;
        } else {
            if (events2[indexSim2].tick - events1[indexSim1].tick == 1) {
                events2[indexSim2].tick = events1[indexSim1].tick;
            }
            ++indexSim2;
        }
    }

    for (const auto& event : events1) {
        std::cerr << event.toString() << '\n';
    }

    double score1 = 0.0;
    for (const auto& event : events1) {
        double eventScore = event.damage * (actionTicks + 1 - event.tick) / actionTicks;
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
        double eventScore = event.damage * (actionTicks + 1 - event.tick) / actionTicks;
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

    if (!areSame(score1, score2, targetImportance * 1e-3)) {
        if (score1 > score2) {
            return 1;
        } else {
            return -1;
        }
    }

    std::cerr << "Scores are same" << '\n';
    std::cerr << "Target position:" << targetPos.toString() << '\n';

    if (action1.velocity * (targetPos.x - unit.position.x) > 0 && action2.velocity * (targetPos.x - unit.position.x) < 0) {
        std::cerr << "WIN. Right direction\n";
        return 1;
    }
    if (action1.velocity * (targetPos.x - unit.position.x) < 0 && action2.velocity * (targetPos.x - unit.position.x) > 0) {
        std::cerr << "LOSE. Wrong direction\n";
        return -1;
    }

    if ((targetPos.x < unit.position.x &&
         game.level.tiles[size_t(unit.position.x - 1)][size_t(unit.position.y)] == Tile::WALL) ||
        (targetPos.x > unit.position.x &&
         game.level.tiles[size_t(unit.position.x + 1)][size_t(unit.position.y)] == Tile::WALL)) {
        if (action1.jump && !action2.jump) {
            std::cerr << "WIN. We need to jump\n";
            return 1;
        }
        if (!action1.jump && action2.jump) {
            std::cerr << "LOSE. We don't need to jump\n";
            return -1;
        }
    } else {
        if (!action1.jump && action2.jump) {
            std::cerr << "WIN. We don't need to jump\n";
            return 1;
        }
        if (action1.jump && !action2.jump) {
            std::cerr << "LOSE. We need to jump\n";
            return -1;
        }
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
