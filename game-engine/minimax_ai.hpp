#pragma once

#include "ai.hpp"
#include "minimaxBase.hpp"
#include "3d-connect4-board.hpp"
#include <array>
#include <algorithm>
#include <random>
#include <utility>

struct ZobristKeys {
    std::array<std::array<uint64_t, 64>, 2> pieces;
    uint64_t sideToMove;

    static const ZobristKeys& get() {
        static ZobristKeys instance;
        return instance;
    }

private:
    ZobristKeys() {
        std::mt19937_64 rng(0x123456789ABCDEF);
        std::uniform_int_distribution<uint64_t> dist;
        for (int p = 0; p < 2; ++p) {
            for (int i = 0; i < 64; ++i) {
                pieces[p][i] = dist(rng);
            }
        }
        sideToMove = dist(rng);
    }
};

// Adapter class to make connect3dBoard compatible with minimax library
class MinimaxAdapterBoard : public board_t<connect3dMove, 16> {
private:
    connect3dBoard board;
    double currentScore = 0;
    uint64_t zHash = 0;

public:
    MinimaxAdapterBoard(const connect3dBoard& b) : board(b) {
        const auto& lines = getLookupTables().first;
        for (size_t i = 0; i < lines.size(); ++i) {
            currentScore += evaluateLine(i);
        }

        const auto& z = ZobristKeys::get();
        for (int i = 0; i < 64; ++i) {
            if (board.board[i] == player::A) zHash ^= z.pieces[0][i];
            else if (board.board[i] == player::B) zHash ^= z.pieces[1][i];
        }
        if (board.playerTurn == player::B) zHash ^= z.sideToMove;
    }

    std::array<connect3dMove, 16> findMoves(player play, connect3dMove bestMove) override {
        std::array<connect3dMove, 16> moves;
        int idx = 0;

        // Try best move first if valid (optimization for alpha-beta pruning)
        if (bestMove.isValid()) {
            if (isMoveLegal(bestMove)) {
                moves[idx++] = bestMove;
            }
        }

        std::array<connect3dMove, 16> otherMoves;
        int otherCount = 0;

        for (int i = 0; i < 16; ++i) {
            connect3dMove m(i);
            // Skip if we already added it as bestMove
            if (bestMove.isValid() && m.movenum == bestMove.movenum) continue;

            if (isMoveLegal(m)) {
                otherMoves[otherCount++] = m;
            }
        }

        static std::random_device rd;
        static std::mt19937 g(rd());
        std::shuffle(otherMoves.begin(), otherMoves.begin() + otherCount, g);

        for (int i = 0; i < otherCount; ++i) {
            moves[idx++] = otherMoves[i];
        }

        // Fill remaining slots with invalid moves
        while (idx < 16) {
            moves[idx++] = connect3dMove();
        }

        return moves;
    }

    bool isMoveLegal(const connect3dMove& m) const {
        // A column is full if the top cell (layer 3) is occupied.
        // Layer 3 indices are 48 to 63. Column c corresponds to index c + 48.
        return board.board[m.movenum + 48] == player::NONE;
    }

    void makeMove(connect3dMove m) override {
        // Find the index where the piece will land
        int col = m.movenum;
        int idx = -1;
        for (int d = 0; d < 4; ++d) {
            int temp = d * 16 + col;
            if (board.board[temp] == player::NONE) {
                idx = temp;
                break;
            }
        }

        if (idx != -1) {
            const auto& lines = getLookupTables().second[idx];
            for (int lineIdx : lines) currentScore -= evaluateLine(lineIdx);
            
            int pIdx = (board.playerTurn == player::A) ? 0 : 1;
            zHash ^= ZobristKeys::get().pieces[pIdx][idx];
            zHash ^= ZobristKeys::get().sideToMove;

            board.makeMove(m);
            
            for (int lineIdx : lines) currentScore += evaluateLine(lineIdx);
        } else {
            board.makeMove(m);
        }
    }

    void undoMove(connect3dMove m) override {
        // Find the highest piece in the column and remove it
        int col = m.movenum;
        int idx = -1;
        for (int d = 3; d >= 0; --d) {
            int temp = d * 16 + col;
            if (board.board[temp] != player::NONE) {
                idx = temp;
                break;
            }
        }

        if (idx != -1) {
            const auto& lines = getLookupTables().second[idx];
            for (int lineIdx : lines) currentScore -= evaluateLine(lineIdx);

            player pieceOwner = (board.playerTurn == player::A) ? player::B : player::A;
            int pIdx = (pieceOwner == player::A) ? 0 : 1;
            zHash ^= ZobristKeys::get().pieces[pIdx][idx];
            zHash ^= ZobristKeys::get().sideToMove;

            board.board[idx] = player::NONE;
            board.playerTurn = (board.playerTurn == player::A) ? player::B : player::A;

            for (int lineIdx : lines) currentScore += evaluateLine(lineIdx);
        }
    }

    player checkWin(connect3dMove* m) const override {
        if (m && m->isValid()) {
            int col = m->movenum;
            int idx = -1;
            for (int d = 3; d >= 0; --d) {
                int temp = d * 16 + col;
                if (board.board[temp] != player::NONE) {
                    idx = temp;
                    break;
                }
            }
            if (idx != -1) {
                const auto& lines = getLookupTables().second[idx];
                for (int lineIdx : lines) {
                    player p = checkLineWin(lineIdx);
                    if (p != player::NONE) return p;
                }
                return player::NONE;
            }
        }
        return board.checkWin();
    }

    double heuristic() override {
        return currentScore / 10000.0;
    }

    #if transpositionTableEnabled
    uint64_t getHash() const override { return zHash; }
    #endif

private:
    static const std::pair<std::vector<std::array<int, 4>>, std::vector<std::vector<int>>>& getLookupTables() {
        static const auto tables = []() {
            std::vector<std::array<int, 4>> lines;
            std::vector<std::vector<int>> cellLines(64);
            
            auto addLine = [&](int i1, int i2, int i3, int i4) {
                int lineIdx = lines.size();
                lines.push_back({i1, i2, i3, i4});
                cellLines[i1].push_back(lineIdx);
                cellLines[i2].push_back(lineIdx);
                cellLines[i3].push_back(lineIdx);
                cellLines[i4].push_back(lineIdx);
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

            return std::make_pair(lines, cellLines);
        }();
        return tables;
    }

    double evaluateLine(int lineIdx) {
        const auto& line = getLookupTables().first[lineIdx];
        int a = 0, b = 0;
        for (int i : line) {
            player p = board.board[i];
            if (p == player::A) a++;
            else if (p == player::B) b++;
        }

        if (a > 0 && b > 0) return 0.0;
        if (a == 0 && b == 0) return 0.0;

        if (a > 0) {
            if (a == 1) return 1.0;
            if (a == 2) return 10.0;
            if (a == 3) return 100.0;
        } else {
            if (b == 1) return -1.0;
            if (b == 2) return -10.0;
            if (b == 3) return -100.0;
        }
        return 0.0;
    }

    player checkLineWin(int lineIdx) const {
        const auto& line = getLookupTables().first[lineIdx];
        int a = 0, b = 0;
        for (int i : line) {
            player p = board.board[i];
            if (p == player::A) a++;
            else if (p == player::B) b++;
        }
        if (a == 4) return player::A;
        if (b == 4) return player::B;
        return player::NONE;
    }
};

class MinimaxAI : public AI_base {
    std::vector<TTEntry<connect3dMove>> tt;
public:
    MinimaxAI() {
        tt.resize(1024 * 1024 * 4); // ~4 million entries
    }

    evalReturn getNextMove(connect3dBoard board) override {
        MinimaxAdapterBoard adapter(board);
        stat_t stats;
        connect3dMove bestMove;
        
        // Depth 4 provides a good balance of strength and speed for branching factor 16
        int depth = 4; 

        double score = minimax(adapter, board.getPlayerTurn(), 0, depth, &bestMove, stats/*, tt*/);
        
        return {score, bestMove};
    }
};
