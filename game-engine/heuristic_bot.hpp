#pragma once

#include "ai.hpp"
#include "minimax_ai_b2_v1.hpp"

class HeuristicBot : public AI_base {
public:
    evalReturn getNextMove(connect3dBoard board) override {
        // Use the fast board from b2_v1 which implements the move ordering logic
        b2_v1::connect3dBoardFast fastBoard(board);
        
        // Create the move factory. This factory handles generating moves and 
        // sorting them based on the heuristic delta (move ordering) when getNextBestMove is called.
        auto factory = fastBoard.createMoveFactory(board.getPlayerTurn());
        
        // getNextBestMove calculates heuristics and sorts moves on the first call.
        // It returns the best move based on the heuristic delta.
        b2_v1::connect3dMoveFast bestMove = factory.getNextBestMove();
        
        // Return the best move found. We count this as 1 node explored.
        return {bestMove.deltaHeuristic, bestMove, 1};
    }
};
