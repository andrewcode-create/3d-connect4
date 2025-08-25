#pragma once
#include <string>
#include <vector>
#include <optional>
#include <limits>
#include <cstdint>

#define ALPHABETAPRUNING true


enum player {
    A='A', B='B', NONE=0
};


// Abstract base class for a game board.
// Any game using this minimax library must inherit from this class
// and implement all pure virtual functions.
// the MoveType must implement the following:
// virtual bool isValid() = 0;
template<typename MoveType, int MaxBranch>
struct board_t {
    virtual ~board_t() = default;

    // returns an an array of all possible moves for the player's turn, ordered such that the most promising moves are first. 
    virtual std::array<MoveType, MaxBranch> findMoves(player play) = 0;
    // applies the move to the board
    virtual void makeMove(MoveType m) = 0;
    // undos the move
    virtual void undoMove(MoveType m) = 0;
    // checks whether a player won. Returns the player.
    virtual player checkWin(const MoveType* m) = 0;
    // assigns a heuristic score to the board, where positive is player 1 and negative is player 2. 
    // Values should be scaled between -1 and 1.
    virtual double heuristic() = 0;

    virtual std::string toString() {
        return "NO GAME STR REPRESENTATION";
    }

};

struct stat_t {
    uint64_t nodesExplored = 0;
};

template<typename MoveType, int MaxBranch>
double minimax(board_t<MoveType, MaxBranch>& board, player player, int halfMoveNum, int maxHalfMoveNum, MoveType* bestMoveRet, stat_t& stats) {
    return minimax<MoveType>(board, player, halfMoveNum, maxHalfMoveNum, bestMoveRet, stats, -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), (MoveType*)nullptr);
}


// Runs the minimax algorithm on a given board
// Returns the heuristic score of the best move.
// if bestMoveRet is not nullptr, populates it with the best move found.
template<typename MoveType, int MaxBranch>
double minimax(board_t<MoveType, MaxBranch>& board, player player, int halfMoveNum, int maxHalfMoveNum, MoveType* bestMoveRet, stat_t& stats, double alpha, double beta, MoveType* lastMove) {

    auto pwin = board.checkWin(lastMove);

    if (pwin != player::NONE) {
        return (pwin == player::A ? 1 : -1) * 1000 * (1-halfMoveNum*0.001);
    }

    if (halfMoveNum >= maxHalfMoveNum) {
        // depth limit
        return board.heuristic();
    }



    std::array<MoveType, MaxBranch> moves = board.findMoves(player);

    // check for draw by no moves left
    if (!moves[0].isValid()) {
        return 0;
    }

    MoveType bestmove = moves[0];
    double bestscore;

    if (player == player::A) {
        // set worst score so that first move is always taken
        bestscore = -std::numeric_limits<double>::infinity();

        // go through moves
        for(int i = 0; i < moves.size() && moves[i].isValid(); i++) {
            stats.nodesExplored++;
            // check the move
            board.makeMove(moves[i]);
            double newscore = minimax<MoveType>(board, player::B, halfMoveNum+1, maxHalfMoveNum, nullptr, stats, alpha, beta, &moves[i]);
            board.undoMove(moves[i]);
            // update score and move if needed
            if (bestscore < newscore) {
                bestscore = newscore;
                bestmove = moves[i];
            }

            #if ALPHABETAPRUNING
            // update alpha
            alpha = std::max(alpha, bestscore);
            if (alpha >= beta) {
                break; // Prune the remaining branches
            }
            #endif
        }
    }
    if (player == player::B) {
        // set worst score so that first move is always taken
        bestscore = std::numeric_limits<double>::infinity();

        // go through moves
        for(int i = 0; i < moves.size() && moves[i].isValid(); i++) {
            stats.nodesExplored++;
            // check the move
            board.makeMove(moves[i]);
            double newscore = minimax<MoveType>(board, player::A, halfMoveNum+1, maxHalfMoveNum, nullptr, stats, alpha, beta, &moves[i]);
            board.undoMove(moves[i]);
            // update score and move if needed
            if (bestscore > newscore) {
                bestscore = newscore;
                bestmove = moves[i];
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

    if (bestMoveRet != nullptr) *bestMoveRet = bestmove;
    return bestscore;

}

