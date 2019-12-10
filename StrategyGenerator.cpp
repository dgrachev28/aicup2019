#include "StrategyGenerator.hpp"
#include "Util.hpp"

std::vector<UnitAction> StrategyGenerator::getActions(int actionsCount, int moveDirection, bool jump, bool jumpDown){
    std::vector<UnitAction> actions;
    for (int i = 0; i < actionsCount; ++i) {
        UnitAction action;
        action.velocity = 100 * moveDirection;
        action.jump = jump;
        action.jumpDown = jumpDown;
        action.shoot = true;
        action.reload = false;
        action.swapWeapon = false;
        action.plantMine = false;
        actions.push_back(action);
    }
    return actions;
}

void StrategyGenerator::updateAction(std::unordered_map<int, Unit> units, int unitId, UnitAction& action) {
    const Unit& unit = units[unitId];
    const Unit* nearestEnemy = nullptr;
    for (const auto& [id, other] : units) {
        if (other.playerId != unit.playerId) {
            if (nearestEnemy == nullptr ||
                distanceSqr(unit.position, other.position) <
                distanceSqr(unit.position, nearestEnemy->position)) {
                nearestEnemy = &other;
            }
        }
    }
    action.aim = Vec2Double(0.0, 0.0);
    if (nearestEnemy != nullptr) {
        action.aim = Vec2Double(nearestEnemy->position.x - unit.position.x,
                                nearestEnemy->position.y - unit.position.y);
    }
}


