#pragma once
#include "ai.hpp"
#include <string>
#include <vector>
#include <optional>
#include <limits>
#include <cstdint>
#include <memory>

// Enable alpha beta pruning. Disabling can be helpful to test speedups in move gen/checking
#define ALPHABETAPRUNING true

// Enable basic performance statistics
#define statisticsEnabled true

namespace mm5 {

//Flag for transposition table. Exact is the exact best move, LOWER_BOUND is beta, UPPER_BOUND is alpha.
enum TTFlag { EMPTY = 0, EXACT, LOWER_BOUND, UPPER_BOUND };


// this is the type for transposition table entries
struct TTEntry {
    int16_t score;
    uint8_t depth; // the depth searched to
    uint8_t bestmove; // the best move found in this position
    TTFlag flag = TTFlag::EMPTY; // type of score
    std::array<uint8_t, 10> positionCompressed; // compressed position

    static bool positionEquals(const std::array<uint8_t, 10>& a, const std::array<uint8_t, 10>& b) {
        for (int i = 0; i < 10; ++i) {
            if (a[i] != b[i]) return false;
        }
        return true;
    }
    //uint64_t z_hash = 0; // the hash of the position
    TTEntry() : score(0), depth(0), bestmove(0), flag(TTFlag::EMPTY) { positionCompressed.fill(0); }

    TTEntry(int16_t s, uint8_t d, uint8_t bm, TTFlag f, std::array<uint8_t, 10> p) 
        : score(s), depth(d), bestmove(bm), flag(f), positionCompressed(p) {}
};


// class to contain moves, with method to get next best move by heuristic.


// Abstract base class for a game board.
// Any game using this minimax library must inherit from this class
// and implement all pure virtual functions.
// the MoveType must implement the following:
// virtual bool isValid() = 0;
template<typename MoveType>
struct board_t {
    virtual ~board_t() = default;

    // returns an an array of all possible moves for the player's turn, ordered such that the most promising moves are first. 
    //virtual std::array<MoveType, MaxBranch> findMoves(player play) = 0;
    // it is highly recomended to put bestMove first if it is valid
    //virtual std::array<MoveType, MaxBranch> findMoves(player play, MoveType bestMove) = 0;
    // applies the move to the board
    virtual void makeMove(MoveType m) = 0;
    // undos the move
    virtual void undoMove(MoveType m) = 0;
    // checks whether a player won given last move was m. Returns the player or player::NONE
    virtual player checkWin(MoveType* m) const = 0;
    // assigns a heuristic score to the board, where positive is player A and negative is player B. 
    // Values should be scaled between -1 and 1.
    virtual int16_t heuristic() = 0;

    virtual uint64_t hash() const = 0;

    virtual std::array<uint8_t, 10> compressPosition() const = 0;
};

// statistics type
struct stat_t {
    uint64_t nodesExplored = 0; // total leaf nodes
    uint64_t hashCollisions = 0; // transposition table collisions
};


template<typename BoardType>
int16_t minimax(BoardType& board, player player, int halfMoveNum, int maxHalfMoveNum, 
    typename BoardType::MoveType* bestMoveRet, stat_t& stats, std::vector<TTEntry>& tt) {

    return minimax<BoardType>(board, player, halfMoveNum, maxHalfMoveNum, bestMoveRet, stats, std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max(), (typename BoardType::MoveType*)nullptr, tt);
}


// Runs the minimax algorithm on a given board
// Returns the heuristic score of the best move.
// if bestMoveRet is not nullptr, populates it with the best move found.
template<typename BoardType>
int16_t minimax(BoardType& board, player player, int halfMoveNum, int maxHalfMoveNum, 
    typename BoardType::MoveType* bestMoveRet, stat_t& stats, int16_t alpha, int16_t beta, typename BoardType::MoveType* lastMove, std::vector<TTEntry>& tt) {

    using MoveType = typename BoardType::MoveType;

    // Transposition Table Lookup
    uint64_t hash = 0;
    std::array<uint8_t, 10> boardPos;

    if (!tt.empty()) {
        hash = board.hash();
        boardPos = board.compressPosition();
        size_t idx = hash % tt.size();
        const TTEntry& entry = tt[idx];
        
        if (entry.flag != TTFlag::EMPTY && TTEntry::positionEquals(entry.positionCompressed, boardPos)) {
            if (entry.depth >= (maxHalfMoveNum - halfMoveNum)) {
                if (entry.flag == EXACT) {
                    if (bestMoveRet) *bestMoveRet = MoveType(entry.bestmove);
                    return entry.score;
                } else if (entry.flag == LOWER_BOUND) {
                    alpha = std::max(alpha, entry.score);
                } else if (entry.flag == UPPER_BOUND) {
                    beta = std::min(beta, entry.score);
                }
                if (alpha >= beta) {
                    if (bestMoveRet) *bestMoveRet = MoveType(entry.bestmove);
                    return entry.score;
                }
            }
        }
    }

    MoveType bestmove = MoveType();

    // check if someone won
    auto pwin = board.checkWin(lastMove);

    // if someone won, return a score of intMax minus 1 per move away it is, plus 1
    // this incentivizes earlier wins
    if (pwin != player::NONE) {
        return (pwin == player::A ? std::numeric_limits<int16_t>::max() - halfMoveNum - 1: std::numeric_limits<int16_t>::min() + halfMoveNum + 1);
    }

    // depth limit check
    if (halfMoveNum >= maxHalfMoveNum) {
        return board.heuristic();
    }

    // Keep a copy of the original alpha to determine the node type later
    int16_t original_alpha = alpha;

    auto moves = board.createMoveFactory(player);

    int16_t bestscore;

    bool isDraw = true;

    if (player == player::A) {
        // set worst score so that first move is always taken
        bestscore = std::numeric_limits<int16_t>::min();

        // go through moves
        for(MoveType m = moves.getNextBestMove(); m.isValid(); m = moves.getNextBestMove()) {
            #if statisticsEnabled
            stats.nodesExplored++;
            #endif
            isDraw = false;
            // check the move
            board.makeMove(m);
            int16_t newscore = minimax<BoardType>(board, player::B, halfMoveNum+1, maxHalfMoveNum, nullptr, stats, alpha, beta, &m, tt);
            board.undoMove(m);

            if (bestscore < newscore) {
                bestscore = newscore;
                bestmove = m;
            }

            // Optimization: If we found a winning move, stop.
            if (newscore > std::numeric_limits<int16_t>::max() - 66){
                alpha = bestscore;
                break;
            }

            #if ALPHABETAPRUNING
            // update alpha
            alpha = std::max(alpha, bestscore);
            if (alpha >= beta) {
                break; // Prune the remaining branches
            }
            #endif
        }
    } else {
        bestscore = std::numeric_limits<int16_t>::max();

        // go through moves
        for(MoveType m = moves.getNextBestMove(); m.isValid(); m = moves.getNextBestMove()) {
            #if statisticsEnabled
            stats.nodesExplored++;
            #endif
            isDraw = false;
            // check the move
            board.makeMove(m);
            int16_t newscore = minimax<BoardType>(board, player::A, halfMoveNum+1, maxHalfMoveNum, nullptr, stats, alpha, beta, &m, tt);
            board.undoMove(m);

            if (bestscore > newscore) {
                bestscore = newscore;
                bestmove = m;
            }

            // Optimization: If we found a winning move, stop.
            if (newscore < std::numeric_limits<int16_t>::min() + 66) {
                beta = bestscore;
                break;
            }

            #if ALPHABETAPRUNING
            // update beta
            beta = std::min(beta, bestscore);
            if (beta <= alpha) {
                break; // Prune the remaining branches
            }
            #endif
        }
    }
    
    // out the best move if created, then return the score
    if (bestMoveRet != nullptr) *bestMoveRet = bestmove;

    // Store in Transposition Table
    if (!tt.empty() && !isDraw) {
        TTFlag flag = EXACT;
        if (bestscore <= original_alpha) {
            flag = UPPER_BOUND;
        } else if (bestscore >= beta) {
            flag = LOWER_BOUND;
        }

        size_t idx = hash % tt.size();
        
        if (tt[idx].flag != TTFlag::EMPTY && !TTEntry::positionEquals(tt[idx].positionCompressed, boardPos)) {
            #if statisticsEnabled
            stats.hashCollisions++;
            #endif
        }

        // Replace if empty or if new search is deeper or same depth
        if (tt[idx].flag == TTFlag::EMPTY || (maxHalfMoveNum - halfMoveNum) >= tt[idx].depth) {
            tt[idx] = {bestscore, (uint8_t)(maxHalfMoveNum - halfMoveNum), bestmove.deflate(), flag, boardPos};
        }
    }

    if (isDraw) return 0;
    return bestscore;

}
}
