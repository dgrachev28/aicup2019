#ifndef _MODEL_BULLET_HPP_
#define _MODEL_BULLET_HPP_

#include "../Stream.hpp"
#include <string>
#include <stdexcept>
#include "WeaponType.hpp"
#include <stdexcept>
#include "Vec2Double.hpp"
#include <stdexcept>
#include "Vec2Double.hpp"
#include <memory>
#include <stdexcept>
#include <optional>
#include "ExplosionParams.hpp"

class Bullet {
public:
    WeaponType weaponType;
    int unitId;
    int playerId;
    Vec2Double position;
    Vec2Double velocity;
    double damage;
    double size;
    std::shared_ptr<ExplosionParams> explosionParams;
    double probability;
    int shootTick;
    bool real = true;
    Bullet();
    Bullet(WeaponType weaponType, int unitId, int playerId, Vec2Double position, Vec2Double velocity, double damage, double size, std::shared_ptr<ExplosionParams> explosionParams, double probability = 0.0, int shootTick = 0, bool real = true);
    static Bullet readFrom(InputStream& stream);
    void writeTo(OutputStream& stream) const;
    std::string toString() const;
};

#endif
