#ifndef _MODEL_UNIT_HPP_
#define _MODEL_UNIT_HPP_

#include "../Stream.hpp"
#include <string>
#include <stdexcept>
#include "Vec2Double.hpp"
#include <stdexcept>
#include "Vec2Double.hpp"
#include <stdexcept>
#include "JumpState.hpp"
#include <memory>
#include <stdexcept>
#include "Weapon.hpp"
#include <stdexcept>
#include "WeaponType.hpp"
#include <stdexcept>
#include "WeaponParams.hpp"
#include <stdexcept>
#include "BulletParams.hpp"
#include <memory>
#include <stdexcept>
#include "ExplosionParams.hpp"
#include <memory>
#include <memory>
#include <memory>
#include <optional>

class Unit {
public:
    int playerId;
    int id;
    double health;
    Vec2Double position;
    Vec2Double size;
    JumpState jumpState;
    bool walkedRight;
    bool stand;
    bool onGround;
    bool onLadder;
    int mines;
    std::optional<Weapon> weapon;
    Unit();
    Unit(int playerId, int id, double health, Vec2Double position, Vec2Double size, JumpState jumpState, bool walkedRight, bool stand, bool onGround, bool onLadder, int mines, Weapon weapon);
    static Unit readFrom(InputStream& stream);
    void writeTo(OutputStream& stream) const;
    std::string toString() const;
};

struct DamageEvent {
    int tick;
    int unitId;
    double damage;

    bool real;
    double probability;
    int shootTick;
    double aimAngle;
    double rawProb;

    std::string toString() const {
        auto result = "tick = " + std::to_string(tick)
               + ", unitId = " + std::to_string(unitId)
               + ", damage = " + std::to_string(damage)
               + ", real = " + (real ? "true" : "false");
        if (!real) {
            result += ", probability = " + std::to_string(probability)
                      + ", shootTick = " + std::to_string(shootTick)
                      + ", aimAngle = " + std::to_string(aimAngle)
                      + ", rawProb = " + std::to_string(rawProb);
        }
        return result;
    }
};

#endif
