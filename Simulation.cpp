#include <utility>

#include "Simulation.hpp"
#include "Util.hpp"
#include <algorithm>

Simulation::Simulation(Game game,
                       int myPlayerId,
                       Debug& debug,
                       ColorFloat color,
                       bool simMove,
                       bool simBullets,
                       bool simShoot,
                       int microTicks)
    : game(std::move(game))
    , myPlayerId(myPlayerId)
    , debug(debug)
    , events(std::vector<DamageEvent>())
    , color(color)
    , microTicks(microTicks)
    , simMove(simMove)
    , simBullets(simBullets)
    , simShoot(simShoot) {

    bullets = this->game.bullets;
    startTick = this->game.currentTick;
    for (const Unit& u : this->game.units) {
        units[u.id] = u;
    }
    ticksMultiplier = 1.0 / (game.properties.ticksPerSecond  * microTicks);
}

void Simulation::simulate(std::unordered_map<int, UnitAction> actions, std::optional<int> microTicks) {
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
            if (simBullets) {
                simulateHealthPack(unitId);
            }
        }
        if (simBullets) {
            simulateBullets();
        }
    }
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
            unit.position.x = int(unit.position.x) + unit.size.x / 2;
        } else {
            unit.position.x = int(unit.position.x + 1) - unit.size.x / 2;
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
        if (areSame(unit.jumpState.maxTime, 0.0)) {
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
        unit.position.y -= moveDistance;
    }
}

void Simulation::simulateBullets() {
    std::vector<Bullet> updatedBullets = {};
    std::vector<Bullet> explodedUnitsBullets;
    for (Bullet bullet : bullets) {
        // TODO: should I check unit collision here before bullet movement?
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
        for (const auto& [unitId, unit]: units) {
            if (intersectRects(bulletRect, Rect(unit)) && bullet.unitId != unitId) {
                explode(bullet, unitId);
                if (!bullet.real) {
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
                    0
                });
                break;
            }
        }
    }
    if (lootBoxIdx) {
        game.lootBoxes.erase(game.lootBoxes.begin() + *lootBoxIdx);
    }
}

void Simulation::explode(const Bullet& bullet,
                         std::optional<int> unitId) {
    if (unitId) {
        if (!bullet.real && bullet.playerId == units[*unitId].playerId) {
            return;
        }

        units[*unitId].health -= bullet.damage;
        events.push_back(DamageEvent{
            game.currentTick - startTick,
            *unitId,
            bullet.damage,
            bullet.real,
            calculateHitProbability(bullet, units[*unitId]),
            bullet.real ? 0 : game.currentTick - bullet.virtualParams->shootTick
        });
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
                units[id].health -= bullet.explosionParams->damage;
                events.push_back(DamageEvent{
                    game.currentTick - startTick,
                    id,
                    double(bullet.explosionParams->damage),
                    bullet.real,
                    calculateHitProbability(bullet, unit, true),
                    bullet.real ? 0 : game.currentTick - bullet.virtualParams->shootTick
                });
            }
        }
    }
}

double Simulation::calculateHitProbability(const Bullet& bullet, const Unit& targetUnit, bool explosion) {
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

    if (explosion) {
        if (bullet.unitId == targetUnit.id) {
            return 0.5;
        }
        return 1.0;
    }
    return std::min(1.2, deltaAngle / spreadAngle);
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
    int angleStepsRange = 1;
    int angleStepsCount = 2 * angleStepsRange + 1;

    for (int i = -angleStepsRange; i <= angleStepsRange; ++i) {
        auto bulletPos = unit.position;
        bulletPos.y += unit.size.y / 2;
        double angle = aimAngle + unit.weapon->spread * i * 0.667 / (angleStepsRange == 0 ? 1 : angleStepsRange);
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
                unit.weapon->spread
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
