#ifndef _STRATEGYGENERATOR_HPP_
#define _STRATEGYGENERATOR_HPP_


#include <vector>
#include "model/UnitAction.hpp"

class StrategyGenerator {
public:
    std::vector<UnitAction> getActions(int actionsCount, bool moveRight, bool jump, bool jumpDown);
};


#endif
