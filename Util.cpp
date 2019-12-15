#include <unordered_set>
#include "Util.hpp"
#include "model/Tile.hpp"


bool intersectRects(const Rect& a, const Rect& b) {
    return !(a.left > b.right || a.right < b.left || a.top < b.bottom || a.bottom > b.top);
}

bool checkTile(double x, double y, Tile tile, const Game& game) {
    return game.level.tiles[int(x)][int(y)] == tile;
}

bool checkUnitsCollision(const Rect& unit, int unitId, const std::unordered_map<int, Unit>& units) {
    for (const auto&[id, u] : units) {
        if (id != unitId && intersectRects(unit, Rect(u))) {
            return true;
        }
    }
    return false;
}

bool checkWallCollision(const Rect& unit, const Game& game, bool jumpDown, bool collisionBeforeMove) {
    auto result = checkTile(unit.right, unit.bottom, WALL, game)
                  || checkTile(unit.right, unit.top, WALL, game)
                  || checkTile(unit.left, unit.bottom, WALL, game)
                  || checkTile(unit.left, unit.top, WALL, game);
    if (!jumpDown) {
        result = result || checkLadderCollision(unit, game);

        if (!collisionBeforeMove) {
            result = result
                     || checkTile(unit.right, unit.bottom, PLATFORM, game)
                     || checkTile(unit.left, unit.bottom, PLATFORM, game);
        }
    }
    return result;
}

bool checkJumpPadCollision(const Rect& unit, const Game& game) {
    return game.level.tiles[int(unit.right)][int(unit.bottom)] == JUMP_PAD
           || game.level.tiles[int(unit.right)][int(unit.top)] == JUMP_PAD
           || game.level.tiles[int(unit.left)][int(unit.bottom)] == JUMP_PAD
           || game.level.tiles[int(unit.left)][int(unit.top)] == JUMP_PAD;
}

bool checkLadderCollision(const Rect& rect, const Game& game) {
    const int x = int((rect.right + rect.left) / 2);
    return game.level.tiles[x][int(rect.bottom)] == LADDER
           || game.level.tiles[x][int((rect.bottom + rect.top) / 2)] == LADDER;
}

bool checkLadderCollision(const Unit& unit, const Game& game) {
    return game.level.tiles[int(unit.position.x)][int(unit.position.y)] == LADDER
           || game.level.tiles[int(unit.position.x)][int(unit.position.y + unit.size.y / 2)] == LADDER;
}

bool areSame(double a, double b, double precision) {
    return std::fabs(a - b) < precision;
}

double distanceSqr(const Vec2Double& a, const Vec2Double& b) {
    return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
}

double length(const Vec2Double& a) {
    return sqrt(a.x * a.x + a.y * a.y);
}

double findAngle(const Vec2Double& a, const Vec2Double& b) {
    return acos((a.x * b.x + a.y * b.y) / (length(a) * length(b)));
}

double findAngle(double a, double b) {
    double angle = fabs(a - b);
    if (angle > M_PI) {
        angle = 2 * M_PI - angle;
    }
    return angle;
}

// Check if angle a lies between bound1 and bound2 angles
bool isBeetweenAngles(double a, double bound1, double bound2) {
    double bound1Delta = findAngle(bound1, a);
    double bound2Delta = findAngle(bound2, a);
    double boundDelta = findAngle(bound1, bound2);
    return areSame(bound1Delta + bound2Delta, boundDelta);
}

double sumAngles(double a, double b) {
    double result = a + b;
    if (result > M_PI) {
        return result - 2 * M_PI;
    } else if (result < -M_PI) {
        return result + 2 * M_PI;
    }
    return result;
}

double minAngle(double a, double b) {
    double angle = fabs(a - b);
    if (angle > M_PI) {
        return std::max(a, b);
    }
    return std::min(a, b);
}

double maxAngle(double a, double b) {
    double angle = fabs(a - b);
    if (angle > M_PI) {
        return std::min(a, b);
    }
    return std::max(a, b);
}

bool isLessAngle(double a, double b) {
    double angle = fabs(a - b);
    if (angle > M_PI) {
        return a > b;
    }
    return a < b;
}

bool isGreaterAngle(double a, double b) {
    double angle = fabs(a - b);
    if (angle > M_PI) {
        return a < b;
    }
    return a > b;
}
