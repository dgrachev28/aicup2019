#ifndef _MY_STRATEGY_HPP_
#define _MY_STRATEGY_HPP_

#include "Debug.hpp"
#include "model/CustomData.hpp"
#include "model/Game.hpp"
#include "model/Unit.hpp"
#include "model/UnitAction.hpp"
#include "Simulation.hpp"

class MyStrategy {
public:
    MyStrategy();

    static std::unordered_map<std::string, int> PERF;

    UnitAction getAction(const Unit& unit, const Game& game, Debug& debug);

    Vec2Double predictShootAngle2(const Unit& unit, const Unit& enemyUnit, const Game& game, Debug& debug, bool simulateFallDown = true);

    std::optional<UnitAction> avoidBullets(
        const Unit& unit,
        int enemyUnitId,
        const Game& game,
        const Vec2Double& targetPos,
        double targetImportance,
        const UnitAction& targetAction,
        Debug& debug
    );

    bool shouldShoot(Unit unit, const Unit& enemyUnit, Vec2Double aim, const Game& game, Debug& debug);

    std::unordered_map<int, double> calculateHitProbability(
        const Unit& unit,
        const Unit& enemyUnit,
        const Game& game,
        Debug& debug
    );

    void addRealBulletHits(
        const std::vector<std::vector<DamageEvent>>& events,
        int bulletDamage,
        std::vector<std::unordered_map<int, std::vector<bool>>>& bulletHits
    );

    std::unordered_map<int, double> calculateHitProbability(const std::vector<std::unordered_map<int, std::vector<bool>>>& bulletHits);

    int compareSimulations(
        const Simulation& sim1,
        const Simulation& sim2,
        const UnitAction& action1,
        const UnitAction& action2,
        double targetDistance1,
        double targetDistance2,
        const Game& game,
        const Unit& unit,
        int actionTicks,
        const Vec2Double& targetPos,
        double targetImportance,
        const UnitAction& targetAction
    );

    void buildPathGraph(const Unit& unit, const Game& game, Debug& debug);

    void pathDfs(int x, int y, const std::vector<UnitAction>& actions, const Unit& unit, const Game& game, Debug& debug);

    void floydWarshall();

    int getPathsIndex(const Vec2Double& vec);

    Vec2Double fromPathsIndex(int idx);

    Vec2Double findTargetPosition(const Unit& unit, const Unit* nearestEnemy, const Game& game, Debug& debug, double& targetImportance);

    double calculatePathDistance(const Vec2Double& src, const Vec2Double& dst, const Unit& unit, const Game& game, Debug& debug, Vec2Double& simSrcPosision);

    Vec2Double findNearestTile(const Vec2Double& src);

    void updateAction(std::unordered_map<int, Unit> units, int unitId, int enemyUnitId, UnitAction& action, const Game& game, Debug& debug);

private:
    std::shared_ptr<Simulation> simulation;
    std::optional<Unit> nextUnit;
    std::optional<UnitAction> prevAction;
    std::array<std::array<int16_t, 1200>, 1200> paths;
    std::array<bool, 1200> isPathFilled;
    std::unordered_map<int, std::optional<LootBox>> unitTargetWeapons;
    bool pathsBuilt;
    int pathDrawLastTick;
};

struct Damage {
    double me;
    double enemy;
};

#endif