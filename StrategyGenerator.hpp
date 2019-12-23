#ifndef _STRATEGYGENERATOR_HPP_
#define _STRATEGYGENERATOR_HPP_


#include <vector>
#include <unordered_map>
#include "model/UnitAction.hpp"
#include "model/Unit.hpp"

struct ActionChain {
    int actionsCount;
    double move;
    bool jump;
    bool jumpDown;
};

class StrategyGenerator {
public:
    static std::vector<UnitAction> getActions(int actionsCount, double move, bool jump, bool jumpDown, bool shoot = true);

    static std::vector<UnitAction> getActions(int actionsCount, double move, bool jump, bool jumpDown,
                                              std::vector<UnitAction> tailActions);

    static std::vector<UnitAction> getActions(ActionChain actionChain);
    static std::vector<UnitAction> getActions(std::vector<ActionChain> actionChains);
};


#endif
