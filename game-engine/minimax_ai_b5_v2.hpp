#pragma once

#include "ai.hpp"
#include "minimaxBase5.hpp"
#include "3d-connect4-board.hpp"
#include <array>
#include <algorithm>
#include <random>
#include <utility>

namespace b5_v2 {
struct ZobristKeys {
    std::array<uint64_t, 64> piecesA;
    std::array<uint64_t, 64> piecesB;
    uint64_t sideToMove;

    static const ZobristKeys& get() {
        static ZobristKeys instance;
        return instance;
    }

private:
    ZobristKeys() {
        std::mt19937_64 rng(0x123456789ABCDEF);
        std::uniform_int_distribution<uint64_t> dist;
        for (int i = 0; i < 64; ++i) {
            piecesA[i] = dist(rng);
            piecesB[i] = dist(rng);
        }
        sideToMove = dist(rng);
    }
};

// A fast variant of connect3dMove for AI use
struct connect3dMoveFast {
    uint8_t movenum;
    bool hasHeuristic = false;
    connect3dMoveFast(int m) : movenum(m) {}
    connect3dMoveFast() : movenum(255) {}

    int16_t deltaHeuristic = 0;

    bool isValid() const { return movenum < 16; }
    uint8_t deflate() const { return movenum; }

    static inline uint16_t getMask(const connect3dMoveFast& move) {
        return 1ULL<<move.movenum;
    }

    operator connect3dMove() const { return connect3dMove(movenum); }
    

};

// A fast game board using bitboards for AI use
struct connect3dBoardFast : public mm5::board_t<connect3dMoveFast>{
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
    std::array<uint64_t, 8> zHashes;

    int32_t currentScore = 0;

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
                int16_t initH = board->heuristic();
                player p = board->playerTurn;
                int16_t multiplier = (p == player::A) ? 1 : -1;
                for (int i = 0; i < count; ++i) {
                    board->makeMove(moves[i]);
                    int16_t diff = board->heuristic() - initH;
                    moves[i].deltaHeuristic = diff * multiplier;
                    moves[i].hasHeuristic = true;
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

    connect3dBoardFast() : boardA(0), boardB(0) { zHashes.fill(0); }

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

    // Static lookup table for symmetries
    static const std::array<std::array<int, 64>, 8>& getSymmetryTable() {
        static const auto table = []() {
            std::array<std::array<int, 64>, 8> t;
            for (int z = 0; z < 4; ++z) {
                for (int r = 0; r < 4; ++r) {
                    for (int c = 0; c < 4; ++c) {
                        int i = z * 16 + r * 4 + c;
                        t[0][i] = i;
                        t[1][i] = z * 16 + c * 4 + (3 - r);
                        t[2][i] = z * 16 + (3 - r) * 4 + (3 - c);
                        t[3][i] = z * 16 + (3 - c) * 4 + r;
                        t[4][i] = z * 16 + r * 4 + (3 - c);
                        t[5][i] = z * 16 + (3 - r) * 4 + c;
                        t[6][i] = z * 16 + c * 4 + r;
                        t[7][i] = z * 16 + (3 - c) * 4 + (3 - r);
                    }
                }
            }
            return t;
        }();
        return table;
    }

    static inline int16_t scoreFromCounts(int cntA, int cntB) {
        if (cntA > 0 && cntB > 0) return 0;
        if (cntA == 0 && cntB == 0) return 0;
        
        int k = (cntA > 0) ? cntA : cntB;
        int16_t v = 0;
        if (k == 1) v = 1;
        else if (k == 2) v = 10;
        else if (k == 3) v = 100;
        else if (k >= 4) v = 10000;

        return (cntA > 0) ? v : -v;
    }

    // Calculate score for a specific line based on current board state
    int16_t getLineScore(int lineIdx) const {
        uint64_t mask = getLookup().first[lineIdx];
        int cntA = std::popcount(boardA & mask);
        int cntB = std::popcount(boardB & mask);
        return scoreFromCounts(cntA, cntB);
    }

    connect3dBoardFast(const connect3dBoard& b) {
        zHashes.fill(0);
        const auto& keys = ZobristKeys::get();
        const auto& sym = getSymmetryTable();
        for (int i = 0; i < 64; ++i) {
            if (b.board[i] == player::A) {
                boardA |= (1ULL << i);
                for(int s=0; s<8; ++s) zHashes[s] ^= keys.piecesA[sym[s][i]];
            }
            else if (b.board[i] == player::B) {
                boardB |= (1ULL << i);
                for(int s=0; s<8; ++s) zHashes[s] ^= keys.piecesB[sym[s][i]];
            }
        }
        playerTurn = b.playerTurn;
        if (playerTurn == player::B) for(int s=0; s<8; ++s) zHashes[s] ^= keys.sideToMove;
        
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
        
        if (m.hasHeuristic) {
            // deltaHeuristic is the improvement for the mover (always positive for a good move).
            // If A moves, score increases (add delta).
            // If B moves, score decreases (subtract delta).
            int16_t diff = (playerTurn == player::A) ? m.deltaHeuristic : -m.deltaHeuristic;
            currentScore += diff;
        } else {
            int idx = m.movenum + (layer << 4);
            const auto& lines = getLookup().second[idx];
            
            // Subtract score of affected lines before the move
            for (int lineIdx : lines) {
                currentScore -= getLineScore(lineIdx);
            }
        }

        int idx = m.movenum + (layer << 4);
        const auto& keys = ZobristKeys::get();
        const auto& sym = getSymmetryTable();

        if (playerTurn == player::A) {
            boardA |= moveMask;
            for(int s=0; s<8; ++s) zHashes[s] ^= keys.piecesA[sym[s][idx]];
        } else {
            boardB |= moveMask;
            for(int s=0; s<8; ++s) zHashes[s] ^= keys.piecesB[sym[s][idx]];
        }
        for(int s=0; s<8; ++s) zHashes[s] ^= keys.sideToMove;
        playerTurn = (playerTurn == player::A) ? player::B : player::A;

        if (!m.hasHeuristic) {
            int idx = m.movenum + (layer << 4);
            const auto& lines = getLookup().second[idx];
            // Add score of affected lines after the move
            for (int lineIdx : lines) {
                currentScore += getLineScore(lineIdx);
            }
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
        
        if (m.hasHeuristic) {
            // Undo the score change. 
            // Note: playerTurn here is the opponent of the one who made the move (before we flip it back).
            int16_t diff = (playerTurn == player::B) ? m.deltaHeuristic : -m.deltaHeuristic;
            currentScore -= diff;
        } else {
            int idx = m.movenum + (layer << 4);
            const auto& lines = getLookup().second[idx];

            // Subtract score of affected lines before undoing
            for (int lineIdx : lines) {
                currentScore -= getLineScore(lineIdx);
            }
        }

        const auto& keys = ZobristKeys::get();
        const auto& sym = getSymmetryTable();
        for(int s=0; s<8; ++s) zHashes[s] ^= keys.sideToMove;
        playerTurn = (playerTurn == player::A) ? player::B : player::A;
        
        int idx = m.movenum + (layer << 4);

        if (playerTurn == player::A) {
            boardA &= ~moveMask;
            for(int s=0; s<8; ++s) zHashes[s] ^= keys.piecesA[sym[s][idx]];
        } else {
            boardB &= ~moveMask;
            for(int s=0; s<8; ++s) zHashes[s] ^= keys.piecesB[sym[s][idx]];
        }

        if (!m.hasHeuristic) {
            int idx = m.movenum + (layer << 4);
            const auto& lines = getLookup().second[idx];
            // Add score of affected lines after undoing (restoring previous state)
            for (int lineIdx : lines) {
                currentScore += getLineScore(lineIdx);
            }
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

    int16_t heuristic() override {
        return (int16_t)currentScore;
    }

    uint64_t hash() const override {
        uint64_t m = zHashes[0];
        for(int i=1; i<8; ++i) if(zHashes[i] < m) m = zHashes[i];
        return m;
    }

private:
    void getColumnValues(std::array<uint8_t, 16>& vals) const {
        for (int i = 0; i < 16; i++) {
            uint8_t val = 0;
            // L1 (Bottom)
            if (boardA & (1ULL << i)) val += 1;
            else if (boardB & (1ULL << i)) val += 16;
            
            // L2
            if (boardA & (1ULL << (i+16))) val += 1;
            else if (boardB & (1ULL << (i+16))) val += 8;

            // L3
            if (boardA & (1ULL << (i+32))) val += 1;
            else if (boardB & (1ULL << (i+32))) val += 4;

            // L4 (Top)
            if (boardA & (1ULL << (i+48))) val += 1;
            else if (boardB & (1ULL << (i+48))) val += 2;
            
            vals[i] = val;
        }
    }

public:
    std::array<uint8_t, 10> compressPositionNoRotation() const {
        std::array<uint8_t, 16> vals;
        getColumnValues(vals);
        
        std::array<uint8_t, 10> compressed;
        compressed.fill(0);
        int bitOffset = 0;
        for (int i = 0; i < 16; i++) {
            uint8_t v = vals[i];
            int byteIdx = bitOffset / 8;
            int bitInByte = bitOffset % 8;
            compressed[byteIdx] |= (v << bitInByte);
            if (bitInByte > 3) {
                compressed[byteIdx + 1] |= (v >> (8 - bitInByte));
            }
            bitOffset += 5;
        }
        return compressed;
    }

    std::array<uint8_t, 10> compressPosition() const override {

        //return compressPositionNoRotation();
        std::array<uint8_t, 16> vals;
        getColumnValues(vals);
        
        std::array<uint8_t, 10> minCompressed;
        minCompressed.fill(255);

        const auto& symTable = getSymmetryTable();

        for (int s = 0; s < 8; s++) {
            std::array<uint8_t, 16> permutedVals;
            for(int src=0; src<16; ++src) {
                permutedVals[symTable[s][src]] = vals[src];
            }

            std::array<uint8_t, 10> currentCompressed;
            currentCompressed.fill(0);
            int bitOffset = 0;
            for (int i = 0; i < 16; i++) {
                uint8_t v = permutedVals[i];
                int byteIdx = bitOffset / 8;
                int bitInByte = bitOffset % 8;
                currentCompressed[byteIdx] |= (v << bitInByte);
                if (bitInByte > 3) {
                    currentCompressed[byteIdx + 1] |= (v >> (8 - bitInByte));
                }
                bitOffset += 5;
            }
            
            if (currentCompressed < minCompressed) {
                minCompressed = currentCompressed;
            }
        }
        return minCompressed;
    }
};
}


class MinimaxAI_b5_v2 : public AI_base {
public:
    std::vector<mm5::TTEntry> tt;

    MinimaxAI_b5_v2() {
        tt.resize((1024 * 1024 * 4 + sizeof(mm5::TTEntry)) / sizeof(mm5::TTEntry)); // 4MB + 1 ttentry 
    }

    evalReturn getNextMove(connect3dBoard board) override {
        b5_v2::connect3dBoardFast adapter(board);
        mm5::stat_t stats;
        b5_v2::connect3dMoveFast bestMove;
        
        // Depth 5 provides a good balance of strength and speed for branching factor 16
        int depth = 7; 

        int16_t score = mm5::minimax(adapter, board.getPlayerTurn(), 0, depth, &bestMove, stats, tt);
        
        return {((double) score) / 16000.0, bestMove, stats.nodesExplored, stats.hashCollisions};
    }
};
