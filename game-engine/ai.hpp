#pragma once

#include "3d-connect4-board.hpp"

// This is a base class for interacting with the game board.
// Human and AI classes win inherit from this.
class AI_base {
public:
    virtual ~AI_base() = default;

    struct evalReturn {
        double score = 0; // positive good for A, negative good for B. This is score after the move.
        connect3dMove move;
        uint64_t nodesExplored = 0;
        uint64_t hashCollisions = 0;
    };

    // takes in a copy of the game board
    // returns 
    virtual evalReturn getNextMove(connect3dBoard board) = 0;
};
