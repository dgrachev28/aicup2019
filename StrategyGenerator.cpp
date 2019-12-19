#include "StrategyGenerator.hpp"
#include "Util.hpp"

std::vector<UnitAction> StrategyGenerator::getActions(int actionsCount, double move, bool jump, bool jumpDown) {
    std::vector<UnitAction> actions;
    for (int i = 0; i < actionsCount; ++i) {
        UnitAction action;
        action.velocity = 10 * move;
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

std::vector<UnitAction> StrategyGenerator::getActions(int actionsCount, double move, bool jump, bool jumpDown, std::vector<UnitAction> tailActions) {
    std::vector<UnitAction> actions = getActions(actionsCount, move, jump, jumpDown);
    actions.insert(actions.end(), tailActions.begin(), tailActions.end());
    return actions;
}

std::vector<UnitAction> StrategyGenerator::getActions(ActionChain actionChain) {
    return getActions(actionChain.actionsCount, actionChain.move, actionChain.jump, actionChain.jumpDown);
}

std::vector<UnitAction> StrategyGenerator::getActions(std::vector<ActionChain> actionChains) {
    std::vector<UnitAction> actions;
    for (const auto& chain: actionChains) {
        const auto& vec = getActions(chain);
        actions.insert(actions.end(), vec.begin(), vec.end());
    }
    return actions;
}
