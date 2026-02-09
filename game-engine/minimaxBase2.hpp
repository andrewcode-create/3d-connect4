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

// Flag for transposition table. Exact is the exact best move, LOWER_BOUND is beta, UPPER_BOUND is alpha.
//enum TTFlag { EXACT, LOWER_BOUND, UPPER_BOUND };

namespace mm2 {

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

};

// statistics type
struct stat_t {
    uint64_t nodesExplored = 0; // total leaf nodes
    uint64_t hashCollisions = 0; // transposition table collisions
};


template<typename BoardType>
double minimax(BoardType& board, player player, int halfMoveNum, int maxHalfMoveNum, 
    typename BoardType::MoveType* bestMoveRet, stat_t& stats) {

    return minimax<BoardType>(board, player, halfMoveNum, maxHalfMoveNum, bestMoveRet, stats, -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), (typename BoardType::MoveType*)nullptr);
}


// Runs the minimax algorithm on a given board
// Returns the heuristic score of the best move.
// if bestMoveRet is not nullptr, populates it with the best move found.
template<typename BoardType>
double minimax(BoardType& board, player player, int halfMoveNum, int maxHalfMoveNum, 
    typename BoardType::MoveType* bestMoveRet, stat_t& stats, double alpha, double beta, typename BoardType::MoveType* lastMove) {

    using MoveType = typename BoardType::MoveType;


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
            double newscore = minimax<BoardType>(board, player::B, halfMoveNum+1, maxHalfMoveNum, nullptr, stats, alpha, beta, &m);
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
            double newscore = minimax<BoardType>(board, player::A, halfMoveNum+1, maxHalfMoveNum, nullptr, stats, alpha, beta, &m);
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
    if (isDraw) return 0;
    return bestscore;

}
}
