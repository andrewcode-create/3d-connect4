#pragma once

#include "ai.hpp"
#include "minimaxBase2.hpp"
#include "3d-connect4-board.hpp"
#include <array>
#include <algorithm>
#include <random>
#include <utility>

namespace b2_v1 {
// A fast variant of connect3dMove for AI use
struct connect3dMoveFast {
    uint8_t movenum;
    connect3dMoveFast(int m) : movenum(m) {}
    connect3dMoveFast() : movenum(255) {}

    double deltaHeuristic = 0;

    bool isValid() const { return movenum < 16; }
    uint8_t deflate() const { return movenum; }

    static inline uint16_t getMask(const connect3dMoveFast& move) {
        return 1ULL<<move.movenum;
    }

    operator connect3dMove() const { return connect3dMove(movenum); }
    

};

// A fast game board using bitboards for AI use
struct connect3dBoardFast : public mm2::board_t<connect3dMoveFast>{
    using MoveType = connect3dMoveFast;

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
    // each player has their own bitboard
    uint64_t boardA = 0;
    uint64_t boardB = 0;
    player playerTurn = player::A;

    double currentScore = 0;

    struct MoveFactory {
        connect3dBoardFast* board = nullptr;
        std::array<connect3dMoveFast, 16> moves;
        int count = 0;
        int idx = 0;
        bool heuristicCalculated = false;

        // gets next best move by heuristic
        // returns an invalid move if no moves left
        connect3dMoveFast getNextBestMove() {
            if (idx >= count) return connect3dMoveFast();

            // Move ordering: 
            // calculate heuristics of each move
            // then swap best to front
            if (!heuristicCalculated && board) {
                double initH = board->heuristic();
                player p = board->playerTurn;
                for (int i = 0; i < count; ++i) {
                    board->makeMove(moves[i]);
                    double diff = board->heuristic() - initH;
                    if (p == player::A) moves[i].deltaHeuristic = diff;
                    else moves[i].deltaHeuristic = -diff;
                    board->undoMove(moves[i]);
                }
                heuristicCalculated = true;
            }

            // swap best move to front
            int bestIdx = idx;
            for (int i = idx + 1; i < count; ++i) {
                if (moves[i].deltaHeuristic > moves[bestIdx].deltaHeuristic) {
                    bestIdx = i;
                }
            }
            if (bestIdx != idx) {
                std::swap(moves[idx], moves[bestIdx]);
            }
            return moves[idx++];
        }
    };

    connect3dBoardFast() : boardA(0), boardB(0) {}

    // Static lookup table for winning lines and cell-to-line mapping
    static const std::pair<std::vector<uint64_t>, std::vector<std::vector<int>>>& getLookup() {
        static const auto data = []() {
            std::vector<uint64_t> masks;
            std::vector<std::vector<int>> cellLines(64);
            
            auto addLine = [&](int i1, int i2, int i3, int i4) {
                int idx = masks.size();
                uint64_t m = (1ULL << i1) | (1ULL << i2) | (1ULL << i3) | (1ULL << i4);
                masks.push_back(m);
                cellLines[i1].push_back(idx);
                cellLines[i2].push_back(idx);
                cellLines[i3].push_back(idx);
                cellLines[i4].push_back(idx);
            };

            // 1. Stacks (Vertical)
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    int i = r * 4 + c;
                    addLine(i, i + 16, i + 32, i + 48);
                }
            }
            // 2. Rows (Horizontal along c)
            for (int d = 0; d < 4; ++d) {
                for (int r = 0; r < 4; ++r) {
                    int i = d * 16 + r * 4;
                    addLine(i, i + 1, i + 2, i + 3);
                }
            }
            // 3. Columns (Horizontal along r)
            for (int d = 0; d < 4; ++d) {
                for (int c = 0; c < 4; ++c) {
                    int i = d * 16 + c;
                    addLine(i, i + 4, i + 8, i + 12);
                }
            }
            // 4. Planar Diagonals
            for (int d = 0; d < 4; ++d) {
                int i = d * 16;
                addLine(i, i + 5, i + 10, i + 15);
                addLine(i + 3, i + 6, i + 9, i + 12);
            }
            // 5. Stairs (Vertical Diagonals)
            for (int r = 0; r < 4; ++r) {
                int i = r * 4;
                addLine(i, i + 17, i + 34, i + 51);
                addLine(i + 3, i + 18, i + 33, i + 48);
            }
            for (int c = 0; c < 4; ++c) {
                int i = c;
                addLine(i, i + 20, i + 40, i + 60);
                addLine(i + 12, i + 24, i + 36, i + 48);
            }
            // 6. Space Diagonals
            addLine(0, 21, 42, 63);
            addLine(3, 22, 41, 60);
            addLine(12, 25, 38, 51);
            addLine(15, 26, 37, 48);

            return std::make_pair(masks, cellLines);
        }();
        return data;
    }

    // Calculate score for a specific line based on current board state
    double getLineScore(int lineIdx) const {
        uint64_t mask = getLookup().first[lineIdx];
        int cntA = std::popcount(boardA & mask);
        int cntB = std::popcount(boardB & mask);
        
        // If both players have pieces in the line, it's blocked and worth 0
        if (cntA > 0 && cntB > 0) return 0.0;
        if (cntA == 0 && cntB == 0) return 0.0;
        
        // Scoring: 1 piece -> 1, 2 pieces -> 10, 3 pieces -> 100, 4 pieces -> 10000
        auto val = [](int k) {
            if (k == 1) return 1.0;
            if (k == 2) return 10.0;
            if (k == 3) return 100.0;
            if (k >= 4) return 10000.0;
            return 0.0;
        };

        if (cntA > 0) return val(cntA);
        else return -val(cntB);
    }

    connect3dBoardFast(const connect3dBoard& b) {
        for (int i = 0; i < 64; ++i) {
            if (b.board[i] == player::A) boardA |= (1ULL << i);
            else if (b.board[i] == player::B) boardB |= (1ULL << i);
        }
        playerTurn = b.playerTurn;
        
        currentScore = 0;
        // Initialize score by summing up all lines
        const auto& masks = getLookup().first;
        for (size_t i = 0; i < masks.size(); ++i) {
            currentScore += getLineScore(i);
        }
    }

    inline bool isMoveLegal(const connect3dMoveFast& m) const {
        // Move is legal if top layer is empty in move spot
        uint64_t moveMask = (uint64_t)connect3dMoveFast::getMask(m) << 48;
        return !((boardA | boardB) & moveMask);
    }

    void makeMove(connect3dMoveFast m) override {
        uint64_t moveMask = connect3dMoveFast::getMask(m);
        uint64_t filled = boardA | boardB;
        int layer = 0;
        while (filled & moveMask) {
            // move up a layer
            moveMask <<= 16;
            layer++;
        }
        
        int idx = m.movenum + (layer << 4);
        const auto& lines = getLookup().second[idx];
        
        // Subtract score of affected lines before the move
        for (int lineIdx : lines) {
            currentScore -= getLineScore(lineIdx);
        }

        if (playerTurn == player::A) {
            boardA |= moveMask;
        } else {
            boardB |= moveMask;
        }
        playerTurn = (playerTurn == player::A) ? player::B : player::A;

        // Add score of affected lines after the move
        for (int lineIdx : lines) {
            currentScore += getLineScore(lineIdx);
        }
    }

    void undoMove(connect3dMoveFast m) override {
        uint64_t moveMask = (uint64_t)connect3dMoveFast::getMask(m) << 48;
        uint64_t empty = (~boardA) & (~boardB);
        int layer = 3;
        while (empty & moveMask) {
            // move down a layer
            moveMask >>= 16;
            layer--;
        }
        
        int idx = m.movenum + (layer << 4);
        const auto& lines = getLookup().second[idx];

        // Subtract score of affected lines before undoing
        for (int lineIdx : lines) {
            currentScore -= getLineScore(lineIdx);
        }

        playerTurn = (playerTurn == player::A) ? player::B : player::A;
        if (playerTurn == player::A) {
            boardA &= ~moveMask;
        } else {
            boardB &= ~moveMask;
        }

        // Add score of affected lines after undoing (restoring previous state)
        for (int lineIdx : lines) {
            currentScore += getLineScore(lineIdx);
        }
    }

    player checkWin(connect3dMoveFast* m) const override {
        if (m && m->isValid()) {
            int col = m->movenum;
            uint64_t all = boardA | boardB;
            int z = 0;
            
            // Find the z-level of the piece just placed (topmost in column)
            if ((all >> 48) & (1ULL << col)) z = 3;
            else if ((all >> 32) & (1ULL << col)) z = 2;
            else if ((all >> 16) & (1ULL << col)) z = 1;
            
            int y = (col >> 2) & 3;
            int x = col & 3;

            uint64_t playerBoard = (playerTurn == player::A) ? boardB : boardA; // reversed because last move was opposite player

            static constexpr int dirs[13][3] = {
                {1,0,0}, {0,1,0}, {0,0,1}, 
                {1,1,0}, {1,-1,0}, {1,0,1}, {1,0,-1}, {0,1,1}, {0,1,-1}, 
                {1,1,1}, {1,1,-1}, {1,-1,1}, {1,-1,-1}
            };

            for (const auto& d : dirs) {
                int count = 1;
                // Positive direction
                for (int k = 1; k < 4; ++k) {
                    int nx = x + k*d[0];
                    int ny = y + k*d[1];
                    int nz = z + k*d[2];
                    if (nx >= 0 && nx < 4 && ny >= 0 && ny < 4 && nz >= 0 && nz < 4) {
                        if (playerBoard & (1ULL << (nz*16 + ny*4 + nx))) count++;
                        else break;
                    } else break;
                }
                // Negative direction
                for (int k = 1; k < 4; ++k) {
                    int nx = x - k*d[0];
                    int ny = y - k*d[1];
                    int nz = z - k*d[2];
                    if (nx >= 0 && nx < 4 && ny >= 0 && ny < 4 && nz >= 0 && nz < 4) {
                        if (playerBoard & (1ULL << (nz*16 + ny*4 + nx))) count++;
                        else break;
                    } else break;
                }
                
                if (count >= 4) return (playerTurn == player::A) ? player::B : player::A;
            }
        }

        return player::NONE;
    }

    MoveFactory createMoveFactory(player p) {
        uint64_t boards = boardA | boardB;
        MoveFactory factory;
        factory.board = this;

        int numMoves = 0;

        for (int i = 0; i < 16; i++) {
            if (!((boards >> (i + 48)) & 1)) {
                factory.moves[numMoves++] = connect3dMoveFast(i);
            }
        }
        factory.count = numMoves;

        static thread_local std::mt19937 g(std::random_device{}());
        std::shuffle(factory.moves.begin(), factory.moves.begin() + numMoves, g);

        return factory;
    }

    double heuristic() override {
        return currentScore / 10000.0;
    }

};
}


class MinimaxAI_b2_v1 : public AI_base {
public:
    MinimaxAI_b2_v1() {
        //tt.resize(1024 * 1024 * 4); // ~4 million entries
    }

    evalReturn getNextMove(connect3dBoard board) override {
        b2_v1::connect3dBoardFast adapter(board);
        mm2::stat_t stats;
        b2_v1::connect3dMoveFast bestMove;
        
        // Depth 4 provides a good balance of strength and speed for branching factor 16
        int depth = 6; 

        double score = mm2::minimax(adapter, board.getPlayerTurn(), 0, depth, &bestMove, stats/*, tt*/);
        
        return {score, bestMove, stats.nodesExplored};
    }
};
