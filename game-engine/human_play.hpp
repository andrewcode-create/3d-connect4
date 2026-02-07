#pragma once

#include "ai.hpp"
#include "3d-connect4-board.hpp"
#include <iostream>
#include <limits>
#include <vector>

class HumanPlayer : public AI_base {
public:
    evalReturn getNextMove(connect3dBoard board) override {
        // Print the board state
        std::cout << connect3dBoard::toString(board) << std::endl;

        // Get valid moves
        std::vector<connect3dMove> moves = board.findMoves();

        // If no moves are possible
        if (moves.empty()) {
            return {0, connect3dMove(0)};
        }

        int r, c;
        while (true) {
            std::cout << "Enter move (row [0-3] col [0-3]): ";
            
            if (std::cin >> r >> c) {
                int moveIdx = r * 4 + c;
                
                // Check if the move is valid
                bool isValid = false;
                for (const auto& move : moves) {
                    if (move.movenum == moveIdx) {
                        isValid = true;
                        break;
                    }
                }

                if (isValid) {
                    evalReturn ret;
                    ret.score = 0;
                    ret.move = connect3dMove(moveIdx);
                    return ret;
                } else {
                    std::cout << "Invalid move (column full or out of bounds). Please try again." << std::endl;
                }
            } else {
                std::cout << "Invalid input. Please enter two integers." << std::endl;
                // Clear error flags and ignore rest of line
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }
        }
    }
};
