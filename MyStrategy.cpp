#include <iostream>
#include <cmath>
#include "MyStrategy.hpp"
#include "Util.hpp"
#include "StrategyGenerator.hpp"

MyStrategy::MyStrategy() {}

double distanceSqr(Vec2Double a, Vec2Double b) {
    return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
}

UnitAction MyStrategy::getAction(const Unit& unit, const Game& game,
                                 Debug& debug) {
    if (simulation) {
        simulation->game.currentTick = game.currentTick;
        simulation->bullets = game.bullets;
    } else {
        simulation = Simulation(game, true, true, false);
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
    if (unit.weapon == nullptr && nearestWeapon != nullptr) {
        targetPos = nearestWeapon->position;
    } else if (nearestHealthPack != nullptr && unit.health <= 90.0
               && !((unit.position.x < nearestEnemy->position.x && nearestEnemy->position.x < nearestHealthPack->position.x)
               || (unit.position.x > nearestEnemy->position.x && nearestEnemy->position.x > nearestHealthPack->position.x))) {
        targetPos = nearestHealthPack->position;
    } else if (nearestEnemy != nullptr) {
        if (distanceSqr(unit.position, nearestEnemy->position) > 81.0) {
            targetPos = nearestEnemy->position;
        } else {
            targetPos = unit.position.x > nearestEnemy->position.x ? Vec2Double(50.0, 50.0) : Vec2Double(0.0, 50.0);
        }
    }
    debug.draw(CustomData::Log(
        std::string("Target pos: ") + targetPos.toString()));
    Vec2Double aim = Vec2Double(0, 0);
    if (nearestEnemy != nullptr) {
        aim = Vec2Double(nearestEnemy->position.x - unit.position.x,
                         nearestEnemy->position.y - unit.position.y);
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

    auto bestAction = avoidBullets(unit, game, debug);
    if (bestAction) {
        action.velocity = bestAction->velocity;
        action.jump = bestAction->jump;
        action.jumpDown = bestAction->jumpDown;
    }

    return action;
}

std::optional<UnitAction> MyStrategy::avoidBullets(const Unit& unit, const Game& game, Debug& debug) {
    StrategyGenerator gen;
    std::vector<std::vector<UnitAction>> actionSets = {
        gen.getActions(30, true, true, false),
        gen.getActions(30, false, true, false),
        gen.getActions(30, true, false, true),
        gen.getActions(30, false, false, true)
    };

    UnitAction bestAction;
    int bestHealth = -1000;
    int worstHealth = unit.health;
    for (const auto& actionSet : actionSets) {
        Simulation sim(game, true, true, false);
        sim.bullets = game.bullets;
        std::unordered_map<int, Unit> units;
        for (const Unit& u : game.units) {
            units[u.id] = u;
        }
        for (const UnitAction& action : actionSet) {
            units = sim.simulate(action, units, unit.id);
        }

        if (units[unit.id].health < worstHealth) {
            worstHealth = units[unit.id].health;
        }
        if (units[unit.id].health > bestHealth) {
            bestHealth = units[unit.id].health;
            bestAction = actionSet[0];
        }
    }
//    std::cerr << "Best health: " << bestHealth << "\n";
//    std::cerr << "Worst health: " << worstHealth << "\n";

    if (worstHealth == unit.health) {
        return std::nullopt;
    }
    std::cerr << "Best action: " << bestAction.toString() << "\n";
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
    int angleStepsRange = 15;
    int angleStepsCount = 2 * angleStepsRange + 1;

    std::vector<Damage> damages;
    for (int i = -angleStepsRange; i <= angleStepsRange; ++i) {
        Simulation sim(game, false, true, false);
        sim.bullets = game.bullets;

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
        std::unordered_map<int, Unit> units;
        for (const Unit& u : game.units) {
            units[u.id] = u;
        }
        for (int j = 0; j < 30; ++j) {
            units = sim.simulate(UnitAction(), units, unit.id);
            if (sim.bullets.empty()) {
                break;
            }
        }
        Damage damage{};
        for (const Unit& u : game.units) {
            if (u.id == unit.id) {
                damage.me = u.health - units[u.id].health;
            } else {
                damage.enemy = u.health - units[u.id].health;
            }
        }
        damages.push_back(damage);
    }
    int damageMeCount = 0;
    int damageEnemyCount = 0;
    for (const Damage& dmg : damages) {
        if (dmg.me > 0) {
            ++damageMeCount;
        }
        if (dmg.enemy > 0) {
            ++damageEnemyCount;
        }
    }
//    std::cerr << "damageMeCount: " << damageMeCount << ", P: " << std::to_string(double(damageMeCount) / angleStepsCount) << '\n';
//    std::cerr << "damageEnemyCount: " << damageEnemyCount << ", P: " << std::to_string(double(damageEnemyCount) / angleStepsCount) << '\n';
    if (damageEnemyCount == 0) {
        return false;
    }
    if ((double(damageEnemyCount) / angleStepsCount) < 0.1) {
        return false;
    }
    return double(damageMeCount) / angleStepsCount < 0.05 || damageEnemyCount > 0;
}
