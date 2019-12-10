#ifndef _STRATEGYGENERATOR_HPP_
#define _STRATEGYGENERATOR_HPP_


#include <vector>
#include <unordered_map>
#include "model/UnitAction.hpp"
#include "model/Unit.hpp"

class StrategyGenerator {
public:
    static std::vector<UnitAction> getActions(int actionsCount, int moveDirection, bool jump, bool jumpDown);
    static void updateAction(std::unordered_map<int, Unit> units, int unitId, UnitAction& action);
};


#endif
