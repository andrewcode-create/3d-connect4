#include "minimax.hpp" 
#include <vector>
#include <sstream>
#include <array> 
#include <iostream>
#include <bit>
#include <cmath>
#include <random>
#include <unordered_map>

#define heuristicState true

// Helper class to manage Zobrist keys.
class ZobristKeys {
public:
    std::array<std::array<uint64_t, 64>, 2> pieceKeys; // [player][square_index]
    uint64_t turnKey;

    static const ZobristKeys& getInstance() {
        static ZobristKeys instance;
        return instance;
    }

private:
    ZobristKeys() {
        std::mt19937_64 gen(0xDEADBEEFCAFEBABD); // Fixed seed for reproducibility
        std::uniform_int_distribution<uint64_t> dist;
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 64; ++j) {
                pieceKeys[i][j] = dist(gen);
            }
        }
        turnKey = dist(gen);
    }
};

struct connect3dMove {
    inline uint8_t bit_index(uint64_t m) const { return __builtin_clzll(m); }
    uint64_t move = 0;
    player p = player::NONE;

    #if heuristicState
    bool isHeuristicSet = false;
    double modHeuristic = 0;
    #endif

    // compress to put in tt
    inline uint8_t deflate() const {
        uint8_t ind = bit_index(move);
        // player is A if leftmost bit is 0
        // player is B if leftmost bit is 1
        ind |= (p-65) << 7;
        return ind;
    }

    // inflate to add from tt
    connect3dMove(uint8_t compressed) {
        p = (player) (((compressed>>7) & (0b1)) + 65);

        // The move index (0-63) is stored in the lower 7 bits.
        uint8_t index = compressed & 0x7F; // 0x7F is a mask for the lower 7 bits
        move = 0x8000000000000000ULL >> index;

        isHeuristicSet = false;
        modHeuristic = 0;
    }

    connect3dMove(short r, short c, short d, player p) {
        move = 0x8000000000000000ULL >> (d * 16 + r * 4 + c);
        this->p = p;
    }
    connect3dMove(uint64_t move, player p) {
        this->move = move;
        this->p = p;
    }
    inline bool isValid() const {
        return move!=0 && p!=player::NONE;
    }
    inline short level() const {
        int idx = bit_index(move);
        return idx/16;
        
    }
    inline short row() const {
        int idx = bit_index(move);
        return (idx%16)/4;
    }
    inline short col() const {
        int idx = bit_index(move);
        return idx%4;
    }
    connect3dMove() = default;
};


class connect3dBoard : public board_t<connect3dMove, 16> {
public:
    // goes like this: r0c0d0 r0c1d0 r0c2d0 r0c3d0 r1c0d0 r1c1d0 ... r3c3d0 r0c0d1 r0c1d1 ... r3c3d3
    // bottom is d0, top is d3
    uint64_t boardA = 0;
    uint64_t boardB = 0;

    #if heuristicState
    double heuristicS = 0;
    #endif

    std::array<uint64_t, 8> z_hashes = {};
    uint64_t getHash() const override {
        return *std::min_element(z_hashes.begin(), z_hashes.end());
        //return z_hashes[0]+z_hashes[1]+z_hashes[2]+z_hashes[3]+z_hashes[4]+z_hashes[5]+z_hashes[6]+z_hashes[7];
    }



    static constexpr uint64_t pos(short r, short c, short d) {
        return 0x8000000000000000ULL >> (d * 16 + r * 4 + c);
    }

    // A static array to hold all 76 winning line masks.
    // This is pre-calculated once and shared by all instances of the class.
    static constexpr std::array<uint64_t, 76> win_masks = [] {

        auto pos = [](short r, short c, short d) constexpr {
            return 0x8000000000000000ULL >> (d * 16 + r * 4 + c);
        };
        
        std::array<uint64_t, 76> masks = {};
        int count = 0;

        // 1. Stacks (vertical through planes) - 16 lines
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                masks[count++] = pos(r, c, 0) | pos(r, c, 1) | pos(r, c, 2) | pos(r, c, 3);
            }
        }

        // 2. In-plane rows - 16 lines
        for (int d = 0; d < 4; d++) {
            for (int r = 0; r < 4; r++) {
                masks[count++] = pos(r, 0, d) | pos(r, 1, d) | pos(r, 2, d) | pos(r, 3, d);
            }
        }

        // 3. In-plane columns - 16 lines
        for (int d = 0; d < 4; d++) {
            for (int c = 0; c < 4; c++) {
                masks[count++] = pos(0, c, d) | pos(1, c, d) | pos(2, c, d) | pos(3, c, d);
            }
        }

        // 4. In-plane diagonals - 8 lines
        for (int d = 0; d < 4; d++) {
            masks[count++] = pos(0, 0, d) | pos(1, 1, d) | pos(2, 2, d) | pos(3, 3, d);
            masks[count++] = pos(0, 3, d) | pos(1, 2, d) | pos(2, 1, d) | pos(3, 0, d);
        }

        // 5. Inter-plane diagonals (stairs)
        // Row-stairs - 8 lines
        for (int r = 0; r < 4; r++) {
            masks[count++] = pos(r, 0, 0) | pos(r, 1, 1) | pos(r, 2, 2) | pos(r, 3, 3);
            masks[count++] = pos(r, 0, 3) | pos(r, 1, 2) | pos(r, 2, 1) | pos(r, 3, 0);
        }
        // Col-stairs - 8 lines
        for (int c = 0; c < 4; c++) {
            masks[count++] = pos(0, c, 0) | pos(1, c, 1) | pos(2, c, 2) | pos(3, c, 3);
            masks[count++] = pos(0, c, 3) | pos(1, c, 2) | pos(2, c, 1) | pos(3, c, 0);
        }

        // 6. Main space diagonals - 4 lines
        masks[count++] = pos(0, 0, 0) | pos(1, 1, 1) | pos(2, 2, 2) | pos(3, 3, 3);
        masks[count++] = pos(0, 0, 3) | pos(1, 1, 2) | pos(2, 2, 1) | pos(3, 3, 0);
        masks[count++] = pos(0, 3, 0) | pos(1, 2, 1) | pos(2, 1, 2) | pos(3, 0, 3);
        masks[count++] = pos(3, 0, 0) | pos(2, 1, 1) | pos(1, 2, 2) | pos(0, 3, 3);

        return masks;
    }();

    static constexpr std::array<std::array<uint64_t, 7>, 64> win_masks2 = [] {
        auto pos = [](short r, short c, short d) constexpr {
            return 0x8000000000000000ULL >> (d * 16 + r * 4 + c);
        };
        std::array<std::array<uint64_t, 7>, 64> masks = {};
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                for (int d = 0; d < 4; d++) {
                    int count = 0;
                    for (int i = 0; i < 76; i++) {
                        if ((win_masks[i] & pos(r,c,d)) !=0) {
                            masks[d*16+r*4+c][count++] = win_masks[i];
                        } 
                    }
                    while (count<7) {
                        masks[d*16+r*4+c][count++] = 0ULL;
                    }
                }
            }
        }
        return masks;
    }();

    static constexpr std::array<uint8_t, 64> win_masks2_size = [] {
        std::array<uint8_t, 64> ret = {};
        for (int a = 0; a < 64; a++) {
            int i = 0;
            for (i = 0; i < 7; i++) {
                if (win_masks2[a][i] == 0) break;
            }
            ret[a] = i;
        }
        return ret;
    }();

    static constexpr std::array<double, 4> score_vals = {2, 8, 30, 512*512};
    
    static constexpr std::array<std::array<double, 4>, 4> score_delta_A = [] {
        std::array<std::array<double, 4>, 4> delta = {};
        for (int a = 0; a < 4; ++a) { // Own pieces
            for (int b = 0; b < 4; ++b) { // Opponent pieces
                if (b == 0) { // Opponent has no pieces on the line
                    delta[a][b] = score_vals[a] - (a > 0 ? score_vals[a - 1] : 0);
                } else if (a == 0) { // We have no pieces, but opponent does
                    delta[a][b] = score_vals[b - 1];
                } else { // Line is already contested
                    delta[a][b] = 0;
                }
            }
        }
        return delta;
    }();

    // Pre-calculates the score delta for Player B. It's the inverse of Player A's table.
    static constexpr std::array<std::array<double, 4>, 4> score_delta_B = [] {
        std::array<std::array<double, 4>, 4> delta = {};
        for (int a = 0; a < 4; ++a) { // Player A's pieces
            for (int b = 0; b < 4; ++b) { // Player B's pieces
                delta[a][b] = -score_delta_A[b][a];
            }
        }
        return delta;
    }();

    void insertionSortDescending(connect3dMove* moves, int count) {
        if (count < 2) {
            return; // Already sorted if 0 or 1 elements
        }

        for (int i = 1; i < count; ++i) {
            connect3dMove key = moves[i];
            int j = i - 1;

            // Move elements of moves[0..i-1], that are less than key's heuristic,
            // to one position ahead of their current position.
            // This is modified for a descending sort.
            while (j >= 0 && moves[j].modHeuristic < key.modHeuristic) {
                moves[j + 1] = moves[j];
                j = j - 1;
            }
            moves[j + 1] = key;
        }
    }
    void inline insertionSortAscending(connect3dMove* moves, int count) {
        if (count < 2) {
            return; // Already sorted if 0 or 1 elements
        }

        for (int i = 1; i < count; ++i) {
            connect3dMove key = moves[i];
            int j = i - 1;

            // Move elements of moves[0..i-1], that are less than key's heuristic,
            // to one position ahead of their current position.
            // This is modified for a ascending sort.
            while (j >= 0 && moves[j].modHeuristic > key.modHeuristic) {
                moves[j + 1] = moves[j];
                j = j - 1;
            }
            moves[j + 1] = key;
        }
    }
    struct Symmetries {
        // maps[0] is identity, 1-3 are rotations, 4 is flip, 5-7 are flip+rotations
        static constexpr std::array<std::array<int, 64>, 8> maps = [] {
            std::array<std::array<int, 64>, 8> generatedMaps = {};
            for (int d = 0; d < 4; ++d) {
                for (int r = 0; r < 4; ++r) {
                    for (int c = 0; c < 4; ++c) {
                        int idx = d * 16 + r * 4 + c;
                        // 0: Identity
                        generatedMaps[0][idx] = idx;
                        // 1: Rotate 90 degrees
                        generatedMaps[1][idx] = d * 16 + c * 4 + (3 - r);
                        // 2: Rotate 180 degrees
                        generatedMaps[2][idx] = d * 16 + (3 - r) * 4 + (3 - c);
                        // 3: Rotate 270 degrees
                        generatedMaps[3][idx] = d * 16 + (3 - c) * 4 + r;
                        // 4: Flip horizontally
                        generatedMaps[4][idx] = d * 16 + r * 4 + (3 - c);
                        // 5: Flip + Rotate 90
                        generatedMaps[5][idx] = d * 16 + c * 4 + r;
                        // 6: Flip + Rotate 180
                        generatedMaps[6][idx] = d * 16 + (3 - r) * 4 + c;
                        // 7: Flip + Rotate 270
                        generatedMaps[7][idx] = d * 16 + (3 - c) * 4 + (3 - r);
                    }
                }
            }
            return generatedMaps;
        }();
    };

public:
    connect3dBoard() = default;

    std::array<connect3dMove, 16> findMoves(player play, connect3dMove prevBest) override {

        std::array<connect3dMove, 16> moves;

        // all used spaces
        uint64_t boards = boardA | boardB;

        // all empty spaces
        uint64_t empty = ~boards;

        // all spaces exactly 1 above a piece, plus the bottom 
        uint64_t oneAbove = (boards >> 16) | 0xFFFF000000000000ULL;

        // take only empty spaces of above
        uint64_t movebits = oneAbove & empty;
        //std::cout << "movebits: " << movebits << '\n';
        
        int inx = 0;
        while (movebits != 0) {
            // This trick isolates the least significant set bit
            uint64_t move_bit = movebits & -movebits;

            // Add the move to the list
            moves[inx] = (connect3dMove(move_bit, play));

            // Clear that bit from the mask to find the next one in the next iteration
            movebits &= ~move_bit;

            moves[inx].modHeuristic = scoreMove(moves[inx]);
            moves[inx].isHeuristicSet = true;

            inx++;

        };

        bool usePrev = prevBest.isValid();

        if (play == player::A) {
            // Player A is the maximizer, sort descending (highest score first)
            //insertionSortDescending(moves.data(), inx);
            std::sort(moves.begin(), moves.begin() + inx, [usePrev, prevBest](const connect3dMove& a, const connect3dMove& b) {
                if (usePrev) {
                    if (a.move == prevBest.move) return true;  // 'a' is the best move, it MUST come first.
                    if (b.move == prevBest.move) return false; // 'b' is the best move, 'a' CANNOT come first.
                }
                return (a.modHeuristic > b.modHeuristic);
            });
        } else { // player::B
            // Player B is the minimizer, sort ascending (lowest score first)
            //insertionSortAscending(moves.data(), inx);
            
            std::sort(moves.begin(), moves.begin() + inx, [usePrev, prevBest](const connect3dMove& a, const connect3dMove& b) {
                if (usePrev) {
                    if (a.move == prevBest.move) return true;  // 'a' is the best move, it MUST come first.
                    if (b.move == prevBest.move) return false; // 'b' is the best move, 'a' CANNOT come first.
                }
                return (a.modHeuristic < b.modHeuristic);
            });
        }


        while (inx < 16) {
            moves[inx++] = connect3dMove();
        }
        

        return moves;
        
    }

    void makeMove(connect3dMove m) override {
        #if heuristicState
        if (!m.isHeuristicSet) {
            m.modHeuristic = scoreMove(m);
            m.isHeuristicSet = true;
        }
        heuristicS += m.modHeuristic;
        #endif

        int player_idx = (m.p == player::A) ? 0 : 1;
        int cell_idx = m.level() * 16 + m.row() * 4 + m.col();
        // Update Zobrist hash for the piece

        const auto& keys = ZobristKeys::getInstance();
        for (int i = 0; i < 8; i++) {
            // Find where the piece at cell_idx lands in this symmetry
            int mapped_idx = Symmetries::maps[i][cell_idx];
            
            // Update the corresponding hash for this symmetry
            z_hashes[i] ^= keys.pieceKeys[player_idx][mapped_idx];
            z_hashes[i] ^= keys.turnKey;
        }

        //z_hash ^= ZobristKeys::getInstance().pieceKeys[player_idx][cell_idx];
        // It's good practice to also hash whose turn it is
        //z_hash ^= ZobristKeys::getInstance().turnKey;

        if (m.p == player::A) boardA |= m.move;
        else if (m.p == player::B) boardB |= m.move;
        else std::cerr<<"ERR MAKE MOVE";
    }

    void undoMove(connect3dMove m) override {
        if (m.p == player::A) boardA ^= m.move;
        else if (m.p == player::B) boardB ^= m.move;
        else std::cerr<<"ERR UNDO MOVE";

        #if heuristicState
        if (!m.isHeuristicSet) {
            m.modHeuristic = scoreMove(m);
            m.isHeuristicSet = true;
        }
        heuristicS -= m.modHeuristic;
        #endif

        int player_idx = (m.p == player::A) ? 0 : 1;
        int cell_idx = m.level() * 16 + m.row() * 4 + m.col();

        // Update Zobrist hash (same operations as makeMove)
        const auto& keys = ZobristKeys::getInstance();
        for (int i = 0; i < 8; ++i) {
            int mapped_idx = Symmetries::maps[i][cell_idx];
            z_hashes[i] ^= keys.turnKey;
            z_hashes[i] ^= keys.pieceKeys[player_idx][mapped_idx];
        }
        //z_hash ^= ZobristKeys::getInstance().turnKey;
        //z_hash ^= ZobristKeys::getInstance().pieceKeys[player_idx][cell_idx];
    }

    player checkWin(connect3dMove* m) override {
        if (m != nullptr) {

            if (!(m->isHeuristicSet)) {
                m->modHeuristic = scoreMove(*m);
                m->isHeuristicSet = true;
            }
            // if the move would win, it's heuristic would be big enough
            if (m->isHeuristicSet) {
                return m->modHeuristic > 512*8 ? player::A : m->modHeuristic < -512*8 ? player::B : player::NONE;
            }
            int cellIndex = m->level()*16+m->row()*4+m->col();
            const auto& mask = win_masks2[cellIndex];
            if (m->p == player::A) {
                for (int i = 0; i < 7; i++) {
                    if (mask[i] == 0) break;
                    if ((mask[i]&boardA)==mask[i]) {
                        return player::A;
                    }
                }
            } 
            if (m->p == player::B) {
                for (int i = 0; i < 7; i++) {
                    if (mask[i] == 0) break;
                    if ((mask[i]&boardB)==mask[i]) {
                        return player::B;
                    }
                }
            }
        } else {
            for(int i = 0; i < 76; i++) {
                if ((boardA&win_masks[i]) == win_masks[i]) {
                    return player::A;
                }
            }
            for(int i = 0; i < 76; i++) {
                if ((boardB&win_masks[i]) == win_masks[i]) {
                    return player::B;
                }
            }    
        }
        
        
        return player::NONE;
    }

    // score a speculative move
    double inline scoreMove(const connect3dMove& m) const {
        //static double constexpr scorevals[] = {1,8,16,512};

        double score = 0;
        int cellIndex = m.level()*16+m.row()*4+m.col();
        const auto& mask = win_masks2[cellIndex];

        const auto& table_to_use = (m.p == player::A) ? score_delta_A : score_delta_B;
        //const auto& table_to_use = score_delta_A;

        short s = win_masks2_size[cellIndex];


        for (int i = 0; i < s; i++) {
            //if (mask[i] == 0) break;

            int a = std::popcount((mask[i]&boardA));
            int b = std::popcount((mask[i]&boardB));
            score += table_to_use[a][b];
        }
            
        return score;
    }

    double heuristic() override {
        #if heuristicState
        return heuristicS / (512*8);
        #endif
        return 0;
    }
    std::string toString() override {
        std::stringstream ss;
        ss << "3D Connect Four Board (view from top):\n\n";
        uint64_t occupied = boardA | boardB;

        for (int d = 3; d >= 0; d--) {
            ss << "Layer " << d + 1 << (d == 3 ? " (Top)" : "") << (d == 0 ? " (Bottom)" : "") << "\n";
            ss << "  ---------------\n";
            for (int r = 0; r < 4; r++) {
                ss << r << " | ";
                for (int c = 0; c < 4; c++) {
                    uint64_t bit = pos(r, c, d);
                    if ((boardA & bit) != 0) {
                        ss << "A ";
                    } else if ((boardB & bit) != 0) {
                        ss << "B ";
                    } else {
                        ss << "- ";
                    }
                }
                ss << "|\n";
            }
            ss << "  ---------------\n";
            ss << "    0 1 2 3\n\n";
        }
        return ss.str();
    }
};