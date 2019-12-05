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
        simulation = Simulation(game);
    }
    if (nextUnit) {
//        if (!areSame(unit.position.x, nextUnit->position.x, 1e-2) || !areSame(unit.position.y, nextUnit->position.y, 1e-2)) {
//            std::cerr << "Unit position x: " << unit.position.x << ", simulation pos x: " << nextUnit->position.x << '\n';
//            std::cerr << "Unit position y: " << unit.position.y << ", simulation pos y: " << nextUnit->position.y << '\n';
//            std::cerr << "Unit jumpState: " << unit.jumpState.toString() << ", simulation jumpState: " << nextUnit->jumpState.toString() << '\n';
//        }
        if (unit.health != nextUnit->health) {
            std::cerr << "Unit health: " << unit.health << ", simulation health: " << nextUnit->health << '\n';
        }
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
    } else if (nearestHealthPack != nullptr && (unit.health < 99.5 || distanceSqr(unit.position, nearestEnemy->position) < 49.0)) {
        targetPos = nearestHealthPack->position;
    } else if (nearestEnemy != nullptr) {
        if (distanceSqr(unit.position, nearestEnemy->position) > 49.0) {
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

    std::unordered_map<int, Unit> units;
    for (const Unit& u : game.units) {
        units[u.id] = u;
    }

    nextUnit = simulation->simulate(action, units, unit.id)[unit.id];
    prevAction = action;

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
        gen.getActions(20, true, true, false),
        gen.getActions(20, false, true, false),
        gen.getActions(20, true, false, true),
        gen.getActions(20, false, false, true)
    };

    UnitAction bestAction;
    int bestHealth = -1000;
    int worstHealth = unit.health;
    for (const auto& actionSet : actionSets) {
        Simulation sim(game);
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
    std::cerr << "Best health: " << bestHealth << "\n";
    std::cerr << "Worst health: " << worstHealth << "\n";

    if (worstHealth == unit.health) {
        return std::nullopt;
    }
    std::cerr << "Best action: " << bestAction.toString() << "\n";
    return bestAction;
}
