#pragma once
#include <vector>
#include <sstream>
#include <array> 
#include <iostream>
#include <bit>
#include <cmath>
#include <random>


enum player {
    A='A', B='B', NONE=0
};

struct connect3dMove {
    // move number 0 through 16, corresponding to where to play for the next player.
    uint8_t movenum;
    connect3dMove(int m) {
        movenum = m;
    }
    connect3dMove() : movenum(255) {} // Default to invalid

    bool isValid() const { return movenum < 16; }
    uint8_t deflate() const { return movenum; }
};

struct connect3dBoard {
    // who is next turn to play
    player playerTurn = player::A;
    
    // board
    std::array<player, 64> board = {};
    /* 
     * top (4th) layer:
     * 60  61  62  63
     * 56  57  58  59
     * 52  53  54  55
     * 48  49  50  51
     * 
     * third layer:
     * 44  45  46  47
     * 40  41  42  43
     * 36  37  38  39
     * 32  33  34  35
     * 
     * second layer:
     * 28  29  30  31
     * 24  25  26  27
     * 20  21  22  23
     * 16  17  18  19
     * 
     * bottom (1st) layer:
     * 12  13  14  15
     * 8   9   10  11
     * 4   5   6   7
     * 0   1   2   3
     * 
    */

    // returns the player that won the game, otherwise NONE. Not optimized.
    player checkWin() const {
        auto check = [&](int i1, int i2, int i3, int i4) {
            if (board[i1] != player::NONE &&
                board[i1] == board[i2] &&
                board[i2] == board[i3] &&
                board[i3] == board[i4]) {
                return board[i1];
            }
            return player::NONE;
        };

        // 1. Stacks (Vertical)
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                int i = r * 4 + c;
                if (auto p = check(i, i + 16, i + 32, i + 48); p != player::NONE) return p;
            }
        }

        // 2. Rows (Horizontal along c)
        for (int d = 0; d < 4; ++d) {
            for (int r = 0; r < 4; ++r) {
                int i = d * 16 + r * 4;
                if (auto p = check(i, i + 1, i + 2, i + 3); p != player::NONE) return p;
            }
        }

        // 3. Columns (Horizontal along r)
        for (int d = 0; d < 4; ++d) {
            for (int c = 0; c < 4; ++c) {
                int i = d * 16 + c;
                if (auto p = check(i, i + 4, i + 8, i + 12); p != player::NONE) return p;
            }
        }

        // 4. Planar Diagonals
        for (int d = 0; d < 4; ++d) {
            int i = d * 16;
            if (auto p = check(i, i + 5, i + 10, i + 15); p != player::NONE) return p;
            if (auto p = check(i + 3, i + 6, i + 9, i + 12); p != player::NONE) return p;
        }

        // 5. Stairs (Vertical Diagonals)
        for (int r = 0; r < 4; ++r) {
            int i = r * 4;
            if (auto p = check(i, i + 17, i + 34, i + 51); p != player::NONE) return p;
            if (auto p = check(i + 3, i + 18, i + 33, i + 48); p != player::NONE) return p;
        }
        for (int c = 0; c < 4; ++c) {
            int i = c;
            if (auto p = check(i, i + 20, i + 40, i + 60); p != player::NONE) return p;
            if (auto p = check(i + 12, i + 24, i + 36, i + 48); p != player::NONE) return p;
        }

        // 6. Space Diagonals
        if (auto p = check(0, 21, 42, 63); p != player::NONE) return p;
        if (auto p = check(3, 22, 41, 60); p != player::NONE) return p;
        if (auto p = check(12, 25, 38, 51); p != player::NONE) return p;
        if (auto p = check(15, 26, 37, 48); p != player::NONE) return p;

        return player::NONE;
    }

    // makes a move on the game board. Does not check wins. Not optimized.
    void makeMove(connect3dMove m) {
    
        // propagate the peg downwards until hits the bottom or another peg
        int newIndex;
        for (newIndex = m.movenum+48; newIndex >= 16 && board[newIndex-16] == player::NONE; newIndex-=16);

        if (board[newIndex] != player::NONE) {
            throw "Invalid move.";
        }

        // set board state for the move
        board[newIndex] = playerTurn;

        // change who's turn it is
        playerTurn = (playerTurn == player::A) ? player::B : player::A;
    }

    // returns current player to move. Not optimized.
    player getPlayerTurn() const {
        return playerTurn;
    }

    // returns possible moves. Not optimized. 
    std::vector<connect3dMove> findMoves() {
        std::vector<connect3dMove> moves = std::vector<connect3dMove>();
        // itterate through top layer, return all empty spots.
        for(int i = 48; i < 64; i++) {
            if(board[i] == player::NONE) moves.push_back(connect3dMove(i-48));
        }
        return moves;
    }

    // return string of a board position
    static std::string toString(const connect3dBoard& board) {
        std::stringstream ss;
        
        player winner = board.checkWin();

        if (winner == player::NONE) {
            ss << "Player " << (char)board.playerTurn << "'s turn.\n";
        } else {
            ss << "Player " << (char)winner << " won!\n";
        }

        ss << "3D Connect Four Board (view from top):\n\n";
        for (int d = 3; d >= 0; d--) {
            ss << "Layer " << d +1 << (d == 3 ? " (Top)" : "") << (d == 0 ? " (Bottom)" : "") << "\n";
            ss << "  ---------------\n";
            for (int r = 3; r >= 0; r--) {
                ss << r << " | ";
                for (int c = 0; c < 4; c++) {
                    player p = board.board[d * 16 + r * 4 + c];
                    if (p == player::A) ss << "A ";
                    else if (p == player::B) ss << "B ";
                    else ss << "- ";
                }
                ss << "|\n";
            }
            ss << "  ---------------\n";
            ss << "    0 1 2 3\n\n";
        }
        return ss.str();

    }
};