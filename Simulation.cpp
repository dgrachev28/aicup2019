#include <utility>

#include "Simulation.hpp"
#include "Util.hpp"
#include "MyStrategy.hpp"
#include <algorithm>
#include <chrono>
#include <unordered_set>

Simulation::Simulation(Game game,
                       int myPlayerId,
                       Debug& debug,
                       ColorFloat color,
                       bool simMove,
                       bool simBullets,
                       bool simShoot,
                       int microTicks,
                       bool calcHitProbability)
    : game(std::move(game))
    , myPlayerId(myPlayerId)
    , debug(debug)
    , events(std::vector<DamageEvent>())
    , color(color)
    , microTicks(microTicks)
    , simMove(simMove)
    , simBullets(simBullets)
    , simShoot(simShoot)
    , calcHitProbability(calcHitProbability) {

    bullets = this->game.bullets;
    startTick = this->game.currentTick;
    shootBulletsCount = calcHitProbability ? 12 : 0;
    for (const Unit& u : this->game.units) {
        units[u.id] = u;
        if (calcHitProbability) {
            bulletHits[u.id] = std::vector<bool>(2 * shootBulletsCount + 1, false);
        }
    }
    ticksMultiplier = 1.0 / (game.properties.ticksPerSecond  * microTicks);
}

void Simulation::simulate(const std::unordered_map<int, UnitAction>& actions, std::optional<int> microTicks, bool simSuicide) {
    auto t1 = std::chrono::high_resolution_clock::now();
    if (microTicks) {
        ticksMultiplier = 1.0 / (game.properties.ticksPerSecond * *microTicks);
        this->microTicks = *microTicks;
    }

    ++game.currentTick;
    for (const auto& [unitId, action] : actions) {
        if (units[unitId].weapon && simShoot) {
            Weapon& weapon = *units[unitId].weapon;
            double aimAngle = atan2(action.aim.y, action.aim.x);
            weapon.spread += findAngle(*(weapon.lastAngle), aimAngle);
            weapon.spread = std::clamp(
                weapon.spread,
                weapon.params.minSpread,
                weapon.params.maxSpread
            );
            *(weapon.lastAngle) = aimAngle;
        }
    }
    for (int i = 0; i < this->microTicks; ++i) {
        for (const auto& [unitId, action] : actions) {
            if (simMove) {
                move(action, unitId);
            }
            if (simShoot) {
                simulateShoot(action, unitId);
            }
//            if (simBullets) {
//                simulateHealthPack(unitId);
//            }
            if (simSuicide) {
                simulateSuicide(unitId);
            }
        }
        if (simBullets) {
            simulateBullets();
        }
    }
    if (MyStrategy::PERF.find("simulate") == MyStrategy::PERF.end()) {
        MyStrategy::PERF["simulate"] = 0;
    }
    MyStrategy::PERF["simulate"] += std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - t1).count();
}

void Simulation::move(const UnitAction& action, int unitId) {
    moveX(action, unitId);
    moveY(action, unitId);
}

void Simulation::moveX(const UnitAction& action, int unitId) {
    Unit& unit = units[unitId];
    const double vel = std::clamp(
        action.velocity,
        -game.properties.unitMaxHorizontalSpeed,
        game.properties.unitMaxHorizontalSpeed
    );

    const double moveDistance = vel * ticksMultiplier;

    auto unitRect = Rect(unit);
    unitRect.left += moveDistance;
    unitRect.right += moveDistance;
    bool unitsCollision = checkUnitsCollision(unitRect, unitId, units);
    if (!checkWallCollision(unitRect, game) && !unitsCollision) {
        unit.position.x += moveDistance;
    } else if (!unitsCollision) {
        if (moveDistance < 0) {
            unit.position.x = int(unit.position.x) + unit.size.x / 2 + 1e-9;
        } else {
            unit.position.x = int(unit.position.x + 1) - unit.size.x / 2 - 1e-9;
        }
    }
}

void Simulation::moveY(const UnitAction& action, int unitId) {
    Unit& unit = units[unitId];
    bool padCollision = checkJumpPadCollision(unit, game);
    if (!padCollision && !areSame(unit.jumpState.speed, game.properties.jumpPadJumpSpeed)
        && (!unit.jumpState.canJump || !action.jump)) { // падение вниз
        return fallDown(action, unitId);
    }
    if (game.currentTick == 0) { // первый тик
        unit.jumpState = JumpState(true, game.properties.unitJumpSpeed, game.properties.unitJumpTime, true);
    }
    if (padCollision) { // начало прыжка с батута
        unit.jumpState.speed = game.properties.jumpPadJumpSpeed;
        unit.jumpState.maxTime = game.properties.jumpPadJumpTime;
        unit.jumpState.canCancel = false;
    }
    if (!unit.jumpState.canCancel) { // прыжок с батута
        if (unit.jumpState.maxTime <= 0.0) {
            unit.jumpState = JumpState(false, 0.0, 0.0, false);
            fallDown(action, unitId);
        } else {
            unit.jumpState.maxTime -= ticksMultiplier;
            const double moveDistance = game.properties.jumpPadJumpSpeed * ticksMultiplier;
            auto unitRect = Rect(unit);
            unitRect.top += moveDistance;
            unitRect.bottom += moveDistance;
            if (checkWallCollision(unitRect, game) || checkUnitsCollision(unitRect, unitId, units)) {
                unit.jumpState.canJump = false;
            } else {
                unit.position.y += moveDistance;
            }
        }
        return;
    }
    if (action.jump) { // прыжок с земли
        if (areSame(unit.jumpState.maxTime, 0.0)) {
            unit.jumpState = JumpState(false, 0.0, 0.0, false);
            fallDown(action, unitId);
        } else {
            unit.jumpState.maxTime -= ticksMultiplier;
            const double moveDistance = game.properties.unitJumpSpeed * ticksMultiplier;
            auto unitRect = Rect(unit);
            unitRect.top += moveDistance;
            unitRect.bottom += moveDistance;
            if (checkWallCollision(unitRect, game) || checkUnitsCollision(unitRect, unitId, units)) {
                unit.jumpState.canJump = false;
            } else {
                unit.position.y += moveDistance;
            }
        }
    }
}

void Simulation::fallDown(const UnitAction& action, int unitId) {
    Unit& unit = units[unitId];
    const double moveDistance = game.properties.unitFallSpeed * ticksMultiplier;

    auto unitRect = Rect(unit);
    bool collisionBeforeMove = checkWallCollision(unitRect, game, action.jumpDown);
    unitRect.top -= moveDistance;
    unitRect.bottom -= moveDistance;

    if (checkWallCollision(unitRect, game, action.jumpDown, collisionBeforeMove)
        || checkUnitsCollision(unitRect, unitId, units)) {
        unit.jumpState = JumpState(true, game.properties.unitJumpSpeed, game.properties.unitJumpTime, true);
    } else {
        unit.jumpState = JumpState(false, 0.0, 0.0, false);
        unit.position.y -= moveDistance;
    }
}

void Simulation::simulateBullets() {
    std::vector<Bullet> updatedBullets = {};
    std::vector<Bullet> explodedUnitsBullets;
    for (Bullet bullet : bullets) {
        bool dontPush = false;
        for (auto& explodedBullet : explodedUnitsBullets) {
            if (!bullet.real && bullet.virtualParams->shootTick == explodedBullet.virtualParams->shootTick &&
                areSame(bullet.virtualParams->shootPosition.x, explodedBullet.virtualParams->shootPosition.x) &&
                areSame(bullet.virtualParams->shootPosition.y, explodedBullet.virtualParams->shootPosition.y)) {
                dontPush = true;
                break;
            }
        }
        if (dontPush) {
            continue;
        }
        bullet.position.x += bullet.velocity.x * ticksMultiplier;
        bullet.position.y += bullet.velocity.y * ticksMultiplier;
        Rect bulletRect(bullet);
        if (checkWallCollision(bulletRect, game)) {
            explode(bullet, std::nullopt);
            continue;
        }
        bool exploded = false;
        for (const auto&[unitId, unit]: units) {
            bool virtualExplode = !bullet.real && !calcHitProbability &&
                                  units[bullet.unitId].playerId != unit.playerId &&
                                  distanceSqr(bullet.virtualParams->shootPosition, Vec2Double(unit.position.x, unit.position.y + unit.size.y / 2)) <
                                  distanceSqr(bullet.virtualParams->shootPosition, bullet.position);

            if ((virtualExplode || intersectRects(bulletRect, Rect(unit))) && bullet.unitId != unitId) {
                explode(bullet, unitId);
                if (!bullet.real && !calcHitProbability) {
                    explodedUnitsBullets.push_back(bullet);
                }
                exploded = true;
                break;
            }
        }
        if (!exploded) {
            updatedBullets.push_back(bullet);
        }
    }
    bullets = {};
    for (const auto& bullet : updatedBullets) {
        bool dontPush = false;
        for (auto& explodedBullet : explodedUnitsBullets) {
            if (!bullet.real && bullet.virtualParams->shootTick == explodedBullet.virtualParams->shootTick &&
                areSame(bullet.virtualParams->shootPosition.x, explodedBullet.virtualParams->shootPosition.x) &&
                areSame(bullet.virtualParams->shootPosition.y, explodedBullet.virtualParams->shootPosition.y)) {
                dontPush = true;
                break;
            }
        }
        if (!dontPush) {
            bullets.push_back(bullet);
        }
    }
}

void Simulation::simulateHealthPack(int unitId) {
    std::optional<int> lootBoxIdx = std::nullopt;
    for (int i = 0; i < game.lootBoxes.size(); ++i) {
        if (std::dynamic_pointer_cast<Item::HealthPack>(game.lootBoxes[i].item)) {
            if (intersectRects(Rect(game.lootBoxes[i]), Rect(units[unitId]))) {
                lootBoxIdx = i;
                units[unitId].health += game.properties.healthPackHealth;
                units[unitId].health = std::clamp(units[unitId].health, 0.0, double(game.properties.unitMaxHealth));
                events.push_back(DamageEvent{
                    game.currentTick - startTick,
                    unitId,
                    double(-game.properties.healthPackHealth),
                    true,
                    0.0,
                    0,
                    0.0,
                    0.0
                });
                break;
            }
        }
    }
    if (lootBoxIdx) {
        game.lootBoxes.erase(game.lootBoxes.begin() + *lootBoxIdx);
    }
}

void Simulation::simulateSuicide(int unitId) {
    Unit& unit = units[unitId];
    if (areSame(unit.position.y, int(unit.position.y), 0.01) && unit.jumpState.canJump &&
        (game.level.tiles[int(unit.position.x)][int(unit.position.y) - 1] == WALL ||
         game.level.tiles[int(unit.position.x)][int(unit.position.y) - 1] == PLATFORM) &&
        unit.weapon && (!unit.weapon->fireTimer || unit.weapon->fireTimer <= 1 / 60.0) && unit.mines > 0) {

        double mineRadius = 3.0 - 1.0 / 6 - 0.001;
        Rect mineExplosion(unit.position.x - mineRadius, unit.position.y + 0.25 + mineRadius,
                           unit.position.x + mineRadius, unit.position.y + 0.25 - mineRadius);

        int myKilled = 0;
        int myUnitsCount = 0;
        int enemyUnitsCount = 0;
        std::unordered_set<int> killedEnemyUnits;
        for (const Unit& u: game.units) {
            if (unit.playerId == u.playerId) {
                ++myUnitsCount;
            } else {
                ++enemyUnitsCount;
            }

            if (intersectRects(Rect(u), mineExplosion)) {
                if (u.health <= 50 || unit.mines > 1) {
                    if (unit.playerId == u.playerId) {
                        ++myKilled;
                    } else {
                        killedEnemyUnits.insert(u.id);
                    }
                }
            }
        }

        if ((killedEnemyUnits.size() == 2 || (killedEnemyUnits.size() == 1 && myKilled == 0) ||
             (killedEnemyUnits.size() == 1 && myKilled == 1 && enemyUnitsCount <= myUnitsCount))) {
            for (int killedEnemyUnitId : killedEnemyUnits) {
                events.push_back(DamageEvent{
                    game.currentTick - startTick,
                    killedEnemyUnitId,
                    100.0,
                    false,
                    1.0,
                    0,
                    0.0,
                    1.0
                });
            }
        }
    }
}

void Simulation::explode(const Bullet& bullet,
                         std::optional<int> unitId) {
    if (unitId) {
        if (!bullet.real && bullet.playerId == units[*unitId].playerId) {
            return;
        }
        if (calcHitProbability && !bullet.real) {
            bulletHits[*unitId][bullet.virtualParams->angleIndex] = true;
        } else {
            units[*unitId].health -= bullet.damage;

            double angle = 0.0;
            double rawProb = 0.0;
            double prob = calculateHitProbability(bullet, units[*unitId], angle, rawProb);
            events.push_back(DamageEvent{
                game.currentTick - startTick,
                *unitId,
                bullet.damage,
                bullet.real,
                prob,
                bullet.real ? 0 : game.currentTick - bullet.virtualParams->shootTick,
                angle,
                rawProb
            });
        }
    }
    if (bullet.explosionParams) {
        Rect explosion(
            bullet.position.x - bullet.explosionParams->radius,
            bullet.position.y + bullet.explosionParams->radius,
            bullet.position.x + bullet.explosionParams->radius,
            bullet.position.y - bullet.explosionParams->radius
        );

        for (const auto& [id, unit]: units) {
            if (intersectRects(explosion, Rect(unit))) {
                if (calcHitProbability && !bullet.real) {
                    bulletHits[*unitId][bullet.virtualParams->angleIndex] = true;
                } else {
                    units[id].health -= bullet.explosionParams->damage;

                    double angle = 0.0;
                    double rawProb = 0.0;
                    double prob = 1.0;
                    events.push_back(DamageEvent{
                        game.currentTick - startTick,
                        id,
                        double(bullet.explosionParams->damage),
                        bullet.real,
                        prob,
                        bullet.real ? 0 : game.currentTick - bullet.virtualParams->shootTick,
                        angle,
                        rawProb
                    });
                }
            }
        }
    }
}

double Simulation::calculateHitProbability(const Bullet& bullet, const Unit& targetUnit, double& angle, double& rawProb, bool explosion) {
    if (bullet.real) {
        return 1.0;
    }
    Rect target(targetUnit);
    Vec2Double shootPosition = bullet.virtualParams->shootPosition;
    double deltaAngle1 = findAngle(Vec2Double(target.right - shootPosition.x, target.top - shootPosition.y),
                                   Vec2Double(target.left - shootPosition.x, target.bottom - shootPosition.y));

    double deltaAngle2 = findAngle(Vec2Double(target.left - shootPosition.x, target.top - shootPosition.y),
                                   Vec2Double(target.right - shootPosition.x, target.bottom - shootPosition.y));

    double deltaAngle = deltaAngle1 > deltaAngle2 ? deltaAngle1 : deltaAngle2;
    double spreadAngle = 2 * bullet.virtualParams->spread;

    Vec2Double aim(
        targetUnit.position.x - shootPosition.x,
        targetUnit.position.y + targetUnit.size.y / 2 - shootPosition.y
    );

    angle = fmod(atan2(aim.y, aim.x) + 4 * M_PI, M_PI) * 57.2958;
    if (angle > 90.0) {
        angle = 180.0 - angle;
    }
    double shootTicks = game.currentTick - bullet.virtualParams->shootTick;

    double prob = std::min(1.0, deltaAngle / spreadAngle);
    rawProb = prob;
    prob *= std::max(0.0, 1 - shootTicks / (12 - (angle / (90.0 / 6))));
    return prob;
//    if (explosion) {
//        if (bullet.unitId == targetUnit.id) {
//            return 0.5;
//        }
//        return 1.0;
//    }
}

void Simulation::simulateShoot(const UnitAction& action, int unitId) {
    Unit& unit = units[unitId];
    if (!unit.weapon || (unit.playerId == myPlayerId && unit.weapon->typ == ROCKET_LAUNCHER)) {
        return;
    }
    if (!action.shoot || (unit.weapon->fireTimer && *(unit.weapon->fireTimer) > 1e-9)) {
        unit.weapon->fireTimer = *(unit.weapon->fireTimer) - ticksMultiplier;
        unit.weapon->spread -= unit.weapon->params.aimSpeed * ticksMultiplier;
        unit.weapon->spread = std::clamp(unit.weapon->spread, unit.weapon->params.minSpread, unit.weapon->params.maxSpread);
    } else {
        int enemyUnitId;
        for (const auto& [id, other] : units) {
            // TODO: THIS IS NOT CORRECT
            if (other.playerId != unit.playerId) {
                enemyUnitId = other.id;
            }
        }
        Rect targetUnit(units[enemyUnitId]);

        if (--unit.weapon->magazine == 0) {
            unit.weapon->magazine = unit.weapon->params.magazineSize;
            unit.weapon->fireTimer = unit.weapon->params.reloadTime;
        } else {
            unit.weapon->fireTimer = unit.weapon->params.fireRate;
        }
        createBullets(action, unitId, targetUnit);
        unit.weapon->spread += unit.weapon->params.recoil;
        unit.weapon->spread = std::clamp(unit.weapon->spread, unit.weapon->params.minSpread, unit.weapon->params.maxSpread);
//        if (unit.weapon->typ == ASSAULT_RIFLE) {
//            unit.weapon->spread -= unit.weapon->params.aimSpeed * ticksMultiplier;
//            unit.weapon->spread = std::clamp(unit.weapon->spread,
//                                             unit.weapon->params.minSpread,
//                                             unit.weapon->params.maxSpread);
//        }
        unit.weapon->lastFireTick = game.currentTick;

//        debug.draw(CustomData::Rect(
//            Vec2Float(unit.position.x - unit.size.x / 2, unit.position.y),
//            Vec2Float(unit.size.x, unit.size.y),
//            color
//        ));
    }
}

void Simulation::createBullets(const UnitAction& action, int unitId, const Rect& targetUnit) {
    Unit& unit = units[unitId];
    double aimAngle = atan2(action.aim.y, action.aim.x);
    int angleStepsCount = 2 * shootBulletsCount + 1;

    for (int i = -shootBulletsCount; i <= shootBulletsCount; ++i) {
        auto bulletPos = unit.position;
        bulletPos.y += unit.size.y / 2;
        double angle = aimAngle + unit.weapon->spread * i / (shootBulletsCount == 0 ? 1 : shootBulletsCount);
        auto bulletVel = Vec2Double(cos(angle) * unit.weapon->params.bullet.speed,
                                    sin(angle) * unit.weapon->params.bullet.speed);

        Bullet bullet(
            unit.weapon->typ,
            unit.id,
            unit.playerId,
            bulletPos,
            bulletVel,
            double(unit.weapon->params.bullet.damage),
                unit.weapon->params.bullet.size,
            unit.weapon->params.explosion,
            false,
            VirtualBulletParams{
                game.currentTick,
                bulletPos,
                unit.weapon->spread,
                i + shootBulletsCount
            }
        );
        if (unit.weapon->params.explosion) {
            bullet.explosionParams = std::make_shared<ExplosionParams>(
                ExplosionParams(unit.weapon->params.explosion->radius,
                                unit.weapon->params.explosion->damage));
        }
        bullets.push_back(bullet);
    }

}
