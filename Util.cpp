#include <unordered_set>
#include "Util.hpp"
#include "model/Tile.hpp"


bool intersectRects(const Rect& a, const Rect& b) {
    return !(a.left > b.right || a.right < b.left || a.top < b.bottom || a.bottom > b.top);
}

bool checkTile(double x, double y, Tile tile, const Game& game) {
    return game.level.tiles[int(x)][int(y)] == tile;
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