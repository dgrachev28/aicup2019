#include <utility>

#include "Simulation.hpp"
#include "Util.hpp"
#include <algorithm>

Simulation::Simulation(Game game,
                       Debug& debug,
                       ColorFloat color,
                       bool simMove,
                       bool simBullets,
                       bool simShoot,
                       int microTicks)
    : game(std::move(game))
    , debug(debug)
    , color(color)
    , microTicks(microTicks)
    , simMove(simMove)
    , simBullets(simBullets)
    , simShoot(simShoot) {

    bullets = this->game.bullets;
    for (const Unit& u : this->game.units) {
        units[u.id] = u;
    }
}

void Simulation::simulate(std::unordered_map<int, UnitAction> actions) {
    for (const auto& [unitId, action] : actions) {
        if (units[unitId].weapon && simShoot) {
            Weapon& weapon = *units[unitId].weapon;
            double deltaAngle = fabs(*(weapon.lastAngle) - atan2(action.aim.y, action.aim.x));
            if (deltaAngle > M_PI_2) {
                deltaAngle = fabs(deltaAngle - M_PI);
            }
            if (deltaAngle > M_PI_2) {
                deltaAngle = fabs(deltaAngle - M_PI);
            }
            weapon.spread += deltaAngle;
            weapon.spread = std::clamp(
                weapon.spread,
                weapon.params.minSpread,
                weapon.params.maxSpread
            );
        }
    }
    for (int i = 0; i < microTicks; ++i) {
        for (const auto& [unitId, action] : actions) {
            if (simMove) {
                move(action, unitId);
            }
            if (simShoot) {
                simulateShoot(action, unitId);
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

    const double moveDistance = vel / (game.properties.ticksPerSecond * microTicks);

    auto unitRect = Rect(unit);
    unitRect.left += moveDistance;
    unitRect.right += moveDistance;
    if (!checkWallCollision(unitRect, game) && !checkUnitsCollision(unitRect, unitId, units)) {
        unit.position.x += moveDistance;
    } else {
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
            unit.jumpState.maxTime -= 1 / (game.properties.ticksPerSecond * microTicks);
            const double moveDistance = game.properties.jumpPadJumpSpeed / (game.properties.ticksPerSecond * microTicks);
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
            unit.jumpState.maxTime -= 1 / (game.properties.ticksPerSecond * microTicks);
            const double moveDistance = game.properties.unitJumpSpeed / (game.properties.ticksPerSecond  * microTicks);
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
    const double moveDistance = game.properties.unitFallSpeed / (game.properties.ticksPerSecond * microTicks);

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
    std::vector<Bullet> updatedBullets;
    for (Bullet bullet : bullets) {
        // TODO: should I check unit collision here before bullet movement?
        bullet.position.x += bullet.velocity.x / (game.properties.ticksPerSecond * microTicks);
        bullet.position.y += bullet.velocity.y / (game.properties.ticksPerSecond * microTicks);
        Rect bulletRect(bullet);
        if (checkWallCollision(bulletRect, game)) {
            explode(bullet, std::nullopt);
            continue;
        }
        bool exploded = false;
        for (const auto& [unitId, unit]: units) {
            if (intersectRects(bulletRect, Rect(unit)) && bullet.unitId != unitId) {
                explode(bullet, unitId);
                exploded = true;
                break;
            }
        }
        if (!exploded) {
            updatedBullets.push_back(bullet);
        }
    }
    bullets = updatedBullets;
}

void Simulation::explode(const Bullet& bullet,
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
}

void Simulation::simulateShoot(const UnitAction& action, int unitId) {
    Unit& unit = units[unitId];
    if (!unit.weapon) {
        return;
    }
    if (!action.shoot || (unit.weapon->fireTimer && *(unit.weapon->fireTimer) > 1e-9)) {
        *(unit.weapon->fireTimer) -= 1 / (game.properties.ticksPerSecond * microTicks);
        unit.weapon->spread -= unit.weapon->params.aimSpeed / (game.properties.ticksPerSecond * microTicks);
        unit.weapon->spread = std::clamp(unit.weapon->spread, unit.weapon->params.minSpread, unit.weapon->params.maxSpread);
    } else {
        if (--unit.weapon->magazine == 0) {
            unit.weapon->magazine = unit.weapon->params.magazineSize;
            unit.weapon->fireTimer = std::make_shared<double>(unit.weapon->params.reloadTime);
        } else {
            unit.weapon->fireTimer = std::make_shared<double>(unit.weapon->params.fireRate);
        }
        createBullets(action, unitId);
        unit.weapon->spread += unit.weapon->params.recoil;
        unit.weapon->spread = std::clamp(unit.weapon->spread, unit.weapon->params.minSpread, unit.weapon->params.maxSpread);
//        if (unit.weapon->typ == ASSAULT_RIFLE) {
//            unit.weapon->spread -= unit.weapon->params.aimSpeed / (game.properties.ticksPerSecond * microTicks);
//            unit.weapon->spread = std::clamp(unit.weapon->spread,
//                                             unit.weapon->params.minSpread,
//                                             unit.weapon->params.maxSpread);
//        }
        unit.weapon->lastFireTick = std::make_shared<int>(game.currentTick);

//        debug.draw(CustomData::Rect(
//            Vec2Float(unit.position.x - unit.size.x / 2, unit.position.y),
//            Vec2Float(unit.size.x, unit.size.y),
//            color
//        ));
    }
}

void Simulation::createBullets(const UnitAction& action, int unitId) {
    Unit& unit = units[unitId];
    double aimAngle = atan2(action.aim.y, action.aim.x);
    int angleStepsRange = 0;
    int angleStepsCount = 2 * angleStepsRange + 1;

    for (int i = -angleStepsRange; i <= angleStepsRange; ++i) {
        auto bulletPos = unit.position;
        bulletPos.y += unit.size.y / 2;

        double angle = aimAngle + unit.weapon->spread * i / (angleStepsRange == 0 ? 1 : angleStepsRange);
        auto bulletVel = Vec2Double(cos(angle) * unit.weapon->params.bullet.speed,
                                    sin(angle) * unit.weapon->params.bullet.speed);
        Bullet bullet(
            unit.weapon->typ,
            unit.id,
            unit.playerId,
            bulletPos,
            bulletVel,
            double(unit.weapon->params.bullet.damage) / angleStepsCount,
                unit.weapon->params.bullet.size,
            unit.weapon->params.explosion
        );
        if (unit.weapon->params.explosion) {
            bullet.explosionParams = std::make_shared<ExplosionParams>(
                ExplosionParams(unit.weapon->params.explosion->radius,
                                unit.weapon->params.explosion->damage / angleStepsCount));
        }
        bullets.push_back(bullet);
    }

}

