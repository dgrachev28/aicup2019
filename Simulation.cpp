#include <utility>

#include "Simulation.hpp"
#include "Util.hpp"
#include <algorithm>

Simulation::Simulation(Game game, bool simMove, bool simBullets, bool simShoot, int microTicks)
    : game(std::move(game)), microTicks(microTicks), simMove(simMove), simBullets(simBullets), simShoot(simShoot) {
}

std::unordered_map<int, Unit> Simulation::simulate(const UnitAction& action, std::unordered_map<int, Unit> units, int unitId) {
    if (units[unitId].weapon && simShoot) {
        double deltaAngle = fabs(*(units[unitId].weapon->lastAngle) - atan2(action.aim.y, action.aim.x));
        if (deltaAngle > M_PI_2) {
            deltaAngle = fabs(deltaAngle - M_PI);
        }
        if (deltaAngle > M_PI_2) {
            deltaAngle = fabs(deltaAngle - M_PI);
        }
        units[unitId].weapon->spread += deltaAngle;
        units[unitId].weapon->spread = std::clamp(
            units[unitId].weapon->spread,
            units[unitId].weapon->params.minSpread,
            units[unitId].weapon->params.maxSpread
        );
    }
    for (int i = 0; i < microTicks; ++i) {
        if (simMove) {
            units[unitId] = move(action, units[unitId]);
        }
        if (simBullets) {
            units = simulateBullets(units);
        }
        if (simShoot) {
            units[unitId] = simulateShoot(action, units[unitId]);
        }
    }
    return units;
}

Unit Simulation::move(const UnitAction& action, Unit unit) {
    unit = moveX(action, unit);
    unit = moveY(action, unit);
    // TODO: check collision with other units in moveX and moveY
    return unit;
}

Unit Simulation::moveX(const UnitAction& action, Unit unit) {
    const double vel = std::clamp(
        action.velocity,
        -game.properties.unitMaxHorizontalSpeed,
        game.properties.unitMaxHorizontalSpeed
    );

    const double moveDistance = vel / (game.properties.ticksPerSecond * microTicks);

    auto unitRect = Rect(unit);
    unitRect.left += moveDistance;
    unitRect.right += moveDistance;
    if (!checkWallCollision(unitRect, game)) {
        unit.position.x += moveDistance;
    } else {
        if (moveDistance < 0) {
            unit.position.x = int(unit.position.x) + unit.size.x / 2;
        } else {
            unit.position.x = int(unit.position.x + 1) - unit.size.x / 2;
        }
    }

    return unit;
}

Unit Simulation::moveY(const UnitAction& action, Unit unit) {
    bool padCollision = checkJumpPadCollision(unit, game);
    if (!padCollision && !areSame(unit.jumpState.speed, game.properties.jumpPadJumpSpeed)
        && (!unit.jumpState.canJump || !action.jump)) { // падение вниз
        return fallDown(action, unit);
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
            fallDown(action, unit);
        } else {
            unit.jumpState.maxTime -= 1 / (game.properties.ticksPerSecond * microTicks);
            const double moveDistance = game.properties.jumpPadJumpSpeed / (game.properties.ticksPerSecond * microTicks);
            auto unitRect = Rect(unit);
            unitRect.top += moveDistance;
            unitRect.bottom += moveDistance;
            if (checkWallCollision(unitRect, game)) {
                unit.jumpState.canJump = false;
            } else {
                unit.position.y += moveDistance;
            }
        }
        return unit;
    }
    if (action.jump) { // прыжок с земли
        if (areSame(unit.jumpState.maxTime, 0.0)) {
            unit.jumpState = JumpState(false, 0.0, 0.0, false);
            fallDown(action, unit);
        } else {
            unit.jumpState.maxTime -= 1 / (game.properties.ticksPerSecond * microTicks);
            const double moveDistance = game.properties.unitJumpSpeed / (game.properties.ticksPerSecond  * microTicks);
            auto unitRect = Rect(unit);
            unitRect.top += moveDistance;
            unitRect.bottom += moveDistance;
            if (checkWallCollision(unitRect, game)) {
                unit.jumpState.canJump = false;
            } else {
                unit.position.y += moveDistance;
            }
        }
    }
    return unit;
}

Unit Simulation::fallDown(const UnitAction& action, Unit unit) {
    const double moveDistance = game.properties.unitFallSpeed / (game.properties.ticksPerSecond * microTicks);

    auto unitRect = Rect(unit);
    bool collisionBeforeMove = checkWallCollision(unitRect, game, action.jumpDown);
    unitRect.top -= moveDistance;
    unitRect.bottom -= moveDistance;
    if (checkWallCollision(unitRect, game, action.jumpDown, collisionBeforeMove)) {
        unit.jumpState = JumpState(true, game.properties.unitJumpSpeed, game.properties.unitJumpTime, true);
    } else {
        unit.position.y -= moveDistance;
    }
    return unit;
}

std::unordered_map<int, Unit> Simulation::simulateBullets(std::unordered_map<int, Unit> units) {
    std::vector<Bullet> updatedBullets;
    for (Bullet bullet : bullets) {
        // TODO: should I check unit collision here before bullet movement?
        bullet.position.x += bullet.velocity.x / (game.properties.ticksPerSecond * microTicks);
        bullet.position.y += bullet.velocity.y / (game.properties.ticksPerSecond * microTicks);
        Rect bulletRect(bullet);
        if (checkWallCollision(bulletRect, game)) {
            units = explode(bullet, units, std::nullopt);
            continue;
        }
        bool exploded = false;
        for (const auto& [unitId, unit]: units) {
            if (intersectRects(bulletRect, Rect(unit)) && bullet.unitId != unitId) {
                units = explode(bullet, units, unitId);
                exploded = true;
                break;
            }
        }
        if (!exploded) {
            updatedBullets.push_back(bullet);
        }
    }
    bullets = updatedBullets;
    return units;
}

std::unordered_map<int, Unit> Simulation::explode(const Bullet& bullet,
                                                  std::unordered_map<int, Unit> units,
                                                  std::optional<int> unitId) {
    if (unitId) {
        units[*unitId].health -= bullet.damage;
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
            }
        }
    }
    return units;
}

Unit Simulation::simulateShoot(const UnitAction& action, Unit unit) {
    if (!unit.weapon) {
        return unit;
    }
    if (!action.shoot || *(unit.weapon->fireTimer) > 1e-9) {
        *(unit.weapon->fireTimer) -= 1 / (game.properties.ticksPerSecond * microTicks);
        unit.weapon->spread -= unit.weapon->params.aimSpeed / (game.properties.ticksPerSecond * microTicks);
        unit.weapon->spread = std::clamp(unit.weapon->spread, unit.weapon->params.minSpread, unit.weapon->params.maxSpread);
    } else {
        if (--unit.weapon->magazine == 0) {
            unit.weapon->magazine = unit.weapon->params.magazineSize;
            *(unit.weapon->fireTimer) = unit.weapon->params.reloadTime;
        } else {
            *(unit.weapon->fireTimer) = unit.weapon->params.fireRate;
        }
        unit.weapon->spread += unit.weapon->params.recoil;
        unit.weapon->spread = std::clamp(unit.weapon->spread, unit.weapon->params.minSpread, unit.weapon->params.maxSpread);
//        if (unit.weapon->typ == ASSAULT_RIFLE) {
//            unit.weapon->spread -= unit.weapon->params.aimSpeed / (game.properties.ticksPerSecond * microTicks);
//            unit.weapon->spread = std::clamp(unit.weapon->spread,
//                                             unit.weapon->params.minSpread,
//                                             unit.weapon->params.maxSpread);
//        }
        unit.weapon->lastFireTick = std::make_shared<int>(game.currentTick);
    }
    return unit;
}
