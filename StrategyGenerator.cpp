#include "StrategyGenerator.hpp"

std::vector<UnitAction> StrategyGenerator::getActions(int actionsCount, bool moveRight, bool jump, bool jumpDown) {
    std::vector<UnitAction> actions;
    for (int i = 0; i < actionsCount; ++i) {
        UnitAction action;
        action.velocity = moveRight ? 100 : -100;
        action.jump = jump;
        action.jumpDown = jumpDown;
//        action.shoot = true;
//        action.reload = false;
//        action.swapWeapon = false;
//        action.plantMine = false;
        actions.push_back(action);
    }
    return actions;
}
