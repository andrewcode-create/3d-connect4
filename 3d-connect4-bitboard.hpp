#include "minimax.hpp" 
#include <vector>
#include <sstream>
#include <array> 
#include <iostream>
#include <bit>


struct connect3dMove {
    inline int bit_index(uint64_t m) const { return 63 - __builtin_ctzll(m); }
    uint64_t move = 0;
    player p = player::NONE;
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
        
        uint64_t m = move;
        uint64_t mask = 0xFFFFULL << (16*3);
        for (int i = 0; i < 4; i++) {
            if ((m&mask) != 0) {
                return i;
            }
            mask >>= 16;
        }
        return -1;
        
    }
    inline short row() const {
        int idx = bit_index(move);
        return (idx%16)/4;
        
        uint64_t m = move;
        uint64_t mask = 0xF000F000F000F000ULL;
        for (int i = 0; i < 4; i++) {
            if ((m&mask) != 0) {
                return i;
            }
            mask >>= 4;
        }
        return -1;
        
    }
    inline short col() const {
        int idx = bit_index(move);
        return idx%4;
        
        uint64_t m = move;
        uint64_t mask = 0x8888888888888888ULL;
        for (int i = 0; i < 4; i++) {
            if ((m&mask) != 0) {
                return i;
            }
            mask >>= 1;
        }
        return -1;
        
    }
    connect3dMove() = default;
};


class connect3dBoard : public board_t<connect3dMove, 16> {
public:
    // goes like this: r0c0d0 r0c1d0 r0c2d0 r0c3d0 r1c0d0 r1c1d0 ... r3c3d0 r0c0d1 r0c1d1 ... r3c3d3
    // bottom is d0, top is d3
    uint64_t boardA = 0;
    uint64_t boardB = 0;

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

    static constexpr std::array<std::array<uint64_t, 13>, 64> win_masks2 = [] {
        auto pos = [](short r, short c, short d) constexpr {
            return 0x8000000000000000ULL >> (d * 16 + r * 4 + c);
        };
        std::array<std::array<uint64_t, 13>, 64> masks = {};
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                for (int d = 0; d < 4; d++) {
                    int count = 0;
                    for (int i = 0; i < 76; i++) {
                        if ((win_masks[i] & pos(r,c,d)) !=0) {
                            masks[d*16+r*4+c][count++] = win_masks[i];
                        } 
                    }
                    while (count<13) {
                        masks[d*16+r*4+c][count++] = 0ULL;
                    }
                }
            }
        }
        return masks;
    }();

    

    

public:
    connect3dBoard() = default;

    std::array<connect3dMove, 16> findMoves(player play) override {

        std::array<connect3dMove, 16> moves;
        //moves.reserve(16);
        
        
        
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
            moves[inx++] = (connect3dMove(move_bit, play));

            // Clear that bit from the mask to find the next one in the next iteration
            movebits &= ~move_bit;
        }
        while (inx < 16) {
            moves[inx++] = connect3dMove();
        }
        /*
        for (uint8_t i = 0; i < 64; i++) {
            if((movebits>>i & 0b1) == 1) moves.push_back(connect3dMove(movebits & (0b1ULL<<i), play));
            
        }*/
            /*
        uint64_t boards = boardA | boardB;
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                int d;
                for (d = 0; d<4; d++) {
                    if ((boards&pos(r,c,d))==0ULL) {
                        moves.push_back(connect3dMove(pos(r,c,d), play));
                        break;
                    }
                }
            }
        }*/

        return moves;
    }

    void makeMove(connect3dMove m) override {
        if (m.p == player::A) boardA |= m.move;
        else if (m.p == player::B) boardB |= m.move;
        else std::cerr<<"ERR MAKE MOVE";
    }

    void undoMove(connect3dMove m) override {
        if (m.p == player::A) boardA ^= m.move;
        else if (m.p == player::B) boardB ^= m.move;
        else std::cerr<<"ERR UNDO MOVE";
    }

    player checkWin(const connect3dMove* m) override {
        if (m != nullptr) {
            int cellIndex = m->level()*16+m->row()*4+m->col();
            const auto& mask = win_masks2[cellIndex];
            if (m->p == player::A) {
                for (int i = 0; i < 13; i++) {
                    if (mask[i] == 0) break;
                    if ((mask[i]&boardA)==mask[i]) {
                        return player::A;
                    }
                }
            } 
            if (m->p == player::B) {
                for (int i = 0; i < 13; i++) {
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

    double heuristic() override {
        // no heuristic, so return 0 for draw
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