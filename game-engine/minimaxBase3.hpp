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



namespace mm3 {

//Flag for transposition table. Exact is the exact best move, LOWER_BOUND is beta, UPPER_BOUND is alpha.
enum TTFlag { EXACT, LOWER_BOUND, UPPER_BOUND };

// this is the type for transposition table entries
struct TTEntry {
    double score;
    uint8_t depth; // the depth searched to
    uint8_t bestmove; // the best move found in this position
    TTFlag flag; // type of score
    uint64_t z_hash = 0; // the hash of the position
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
    virtual double heuristic() = 0;

    virtual uint64_t hash() const = 0;
};

// statistics type
struct stat_t {
    uint64_t nodesExplored = 0; // total leaf nodes
    uint64_t hashCollisions = 0; // transposition table collisions
};


template<typename BoardType>
double minimax(BoardType& board, player player, int halfMoveNum, int maxHalfMoveNum, 
    typename BoardType::MoveType* bestMoveRet, stat_t& stats, std::vector<TTEntry>& tt) {

    return minimax<BoardType>(board, player, halfMoveNum, maxHalfMoveNum, bestMoveRet, stats, -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), (typename BoardType::MoveType*)nullptr, tt);
}


// Runs the minimax algorithm on a given board
// Returns the heuristic score of the best move.
// if bestMoveRet is not nullptr, populates it with the best move found.
template<typename BoardType>
double minimax(BoardType& board, player player, int halfMoveNum, int maxHalfMoveNum, 
    typename BoardType::MoveType* bestMoveRet, stat_t& stats, double alpha, double beta, typename BoardType::MoveType* lastMove, std::vector<TTEntry>& tt) {

    using MoveType = typename BoardType::MoveType;

    // Transposition Table Lookup
    uint64_t hash = 0;

    if (!tt.empty()) {
        hash = board.hash();
        size_t idx = hash % tt.size();
        const TTEntry& entry = tt[idx];
        
        if (entry.z_hash == hash) {
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

    // if someone won, return a score of 1000 minus 1 per move away it is
    // this incentivizes earlier wins
    if (pwin != player::NONE) {
        return (pwin == player::A ? 1 : -1) * 1000 * (1-halfMoveNum*0.001);
    }

    // depth limit check
    if (halfMoveNum >= maxHalfMoveNum) {
        return board.heuristic();
    }

    // Keep a copy of the original alpha to determine the node type later
    double original_alpha = alpha;

    auto moves = board.createMoveFactory(player);

    double bestscore;

    bool isDraw = true;

    if (player == player::A) {
        // set worst score so that first move is always taken
        bestscore = -std::numeric_limits<double>::infinity();

        // go through moves
        for(MoveType m = moves.getNextBestMove(); m.isValid(); m = moves.getNextBestMove()) {
            #if statisticsEnabled
            stats.nodesExplored++;
            #endif
            isDraw = false;
            // check the move
            board.makeMove(m);
            double newscore = minimax<BoardType>(board, player::B, halfMoveNum+1, maxHalfMoveNum, nullptr, stats, alpha, beta, &m, tt);
            board.undoMove(m);

            if (bestscore < newscore) {
                bestscore = newscore;
                bestmove = m;
            }

            // Optimization: If we found a winning move, stop.
            if (newscore > 900.0) {
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
        bestscore = std::numeric_limits<double>::infinity();

        // go through moves
        for(MoveType m = moves.getNextBestMove(); m.isValid(); m = moves.getNextBestMove()) {
            #if statisticsEnabled
            stats.nodesExplored++;
            #endif
            isDraw = false;
            // check the move
            board.makeMove(m);
            double newscore = minimax<BoardType>(board, player::A, halfMoveNum+1, maxHalfMoveNum, nullptr, stats, alpha, beta, &m, tt);
            board.undoMove(m);

            if (bestscore > newscore) {
                bestscore = newscore;
                bestmove = m;
            }

            // Optimization: If we found a winning move, stop.
            if (newscore < -900.0) {
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
        
        if (tt[idx].z_hash != 0 && tt[idx].z_hash != hash) {
            #if statisticsEnabled
            stats.hashCollisions++;
            #endif
        }

        // Replace if empty or if new search is deeper or same depth
        if (tt[idx].z_hash == 0 || (maxHalfMoveNum - halfMoveNum) >= tt[idx].depth) {
            tt[idx] = {bestscore, (uint8_t)(maxHalfMoveNum - halfMoveNum), bestmove.deflate(), flag, hash};
        }
    }

    if (isDraw) return 0;
    return bestscore;

}
}
