#ifndef _UTIL_HPP_
#define _UTIL_HPP_


#include <cmath>
#include "model/Game.hpp"

struct Rect {
    Rect(double left, double top, double right, double bottom)
        : left(left), top(top), right(right), bottom(bottom) {
    }

    Rect(const Unit& unit) {
        top = unit.position.y + unit.size.y;
        right = unit.position.x + unit.size.x / 2;
        bottom = unit.position.y;
        left = unit.position.x - unit.size.x / 2;
    }

    Rect(const Bullet& bullet) {
        top = bullet.position.y + bullet.size / 2;
        right = bullet.position.x + bullet.size / 2;
        bottom = bullet.position.y - bullet.size / 2;
        left = bullet.position.x - bullet.size / 2;
    }

    Rect(const LootBox& lootBox) {
        top = lootBox.position.y + lootBox.size.y;
        right = lootBox.position.x + lootBox.size.x / 2;
        bottom = lootBox.position.y;
        left = lootBox.position.x - lootBox.size.x / 2;
    }

    double left;
    double top;
    double right;
    double bottom;
};

bool intersectRects(const Rect& a, const Rect& b);

bool checkUnitsCollision(const Rect& unit, int unitId, const std::unordered_map<int, Unit>& units);

bool checkWallCollision(const Rect& unit, const Game& game, bool jumpDown = true, bool collisionBeforeMove = false);

bool checkJumpPadCollision(const Rect& unit, const Game& game);

bool checkLadderCollision(const Unit& unit, const Game& game);

bool checkLadderCollision(const Rect& rect, const Game& game);

bool areSame(double a, double b, double precision = 1e-7);

double distanceSqr(const Vec2Double& a, const Vec2Double& b);

double length(const Vec2Double& a);

double findAngle(const Vec2Double& a, const Vec2Double& b);

double findAngle(double a, double b);

bool isBeetweenAngles(double a, double bound1, double bound2);

double sumAngles(double a, double b);

double minAngle(double a, double b);

double maxAngle(double a, double b);

bool isLessAngle(double a, double b);

bool isGreaterAngle(double a, double b);

#endif
