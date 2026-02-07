#pragma once

#include "ai.hpp"

#include <random>
#include <vector>

class RandomAI : public AI_base {
public:
    evalReturn getNextMove(connect3dBoard board) override {
        // Get all possible moves for the current player
        std::vector<connect3dMove> moves = board.findMoves();

        evalReturn ret;
        ret.score = 0;

        // If no moves are available, return a default move
        if (moves.empty()) {
            ret.move = connect3dMove(0);
            return ret;
        }

        // Setup random number generation
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, moves.size() - 1);

        // Return a random move
        ret.move = moves[distrib(gen)];
        return ret;
    }
};
