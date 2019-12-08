#ifndef _STRATEGYGENERATOR_HPP_
#define _STRATEGYGENERATOR_HPP_


#include <vector>
#include <unordered_map>
#include "model/UnitAction.hpp"
#include "model/Unit.hpp"

class StrategyGenerator {
public:
    std::vector<UnitAction> getActions(int actionsCount, bool moveRight, bool jump, bool jumpDown) const;
    void updateAction(std::unordered_map<int, Unit> units, int unitId, UnitAction& action) const;
};


#endif
