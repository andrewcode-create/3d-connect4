#include <array>
#include <cassert>
#include <iostream>
#include <vector>
#include "3d-connect4-board.hpp"

static std::vector<std::array<int, 4>> lines() {
    std::vector<std::array<int, 4>> result;
    auto add = [&](int a, int b, int c, int d) { result.push_back({a, b, c, d}); };
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) add(r*4+c, r*4+c+16, r*4+c+32, r*4+c+48);
    for (int d = 0; d < 4; ++d) for (int r = 0; r < 4; ++r) { int i=d*16+r*4; add(i,i+1,i+2,i+3); }
    for (int d = 0; d < 4; ++d) for (int c = 0; c < 4; ++c) { int i=d*16+c; add(i,i+4,i+8,i+12); }
    for (int d = 0; d < 4; ++d) { int i=d*16; add(i,i+5,i+10,i+15); add(i+3,i+6,i+9,i+12); }
    for (int r = 0; r < 4; ++r) { int i=r*4; add(i,i+17,i+34,i+51); add(i+3,i+18,i+33,i+48); }
    for (int c = 0; c < 4; ++c) { int i=c; add(i,i+20,i+40,i+60); add(i+12,i+24,i+36,i+48); }
    add(0,21,42,63); add(3,22,41,60); add(12,25,38,51); add(15,26,37,48);
    return result;
}

int main() {
    auto allLines = lines();
    assert(allLines.size() == 76);
    for (const auto& line : allLines) {
        connect3dBoard board;
        for (int cell : line) board.board[cell] = player::A;
        assert(board.checkWin() == player::A);
    }
    connect3dBoard gravity;
    gravity.makeMove(connect3dMove(5));
    gravity.makeMove(connect3dMove(5));
    assert(gravity.board[5] == player::A);
    assert(gravity.board[21] == player::B);
    assert(gravity.playerTurn == player::A);
    std::cout << "native board tests passed\n";
}
