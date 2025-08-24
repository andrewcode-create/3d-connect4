#include "tictactoe.hpp"
#include <iostream>

int main() {
    TicTacToeBoard board;
    //player current_player = player::A;


    TicTacToeMove bestMove = TicTacToeMove(2,2,'Z');
    stat_t stats;

    double score;
    
    score = minimax(board, player::A, 0, 20, &bestMove, stats);
    std::cout << "AI chooses move to (" << bestMove.row << ", " << bestMove.col << ") with score " << score << "\n";
    std::cout << "stats:" << stats.nodesExplored << "\n";
    stats.nodesExplored = 0;
    board.makeMove(bestMove);
    std::cout << board.toString();
    score = minimax(board, player::B, 0, 20, &bestMove, stats);
    std::cout << "AI chooses move to (" << bestMove.row << ", " << bestMove.col << ") with score " << score << "\n";
    std::cout << "stats:" << stats.nodesExplored << "\n";
    stats.nodesExplored = 0;
    board.makeMove(bestMove);
    std::cout << board.toString();
    score = minimax(board, player::A, 0, 20, &bestMove, stats);
    std::cout << "AI chooses move to (" << bestMove.row << ", " << bestMove.col << ") with score " << score << "\n";
    std::cout << "stats:" << stats.nodesExplored << "\n";
    stats.nodesExplored = 0;
    board.makeMove(bestMove);
    std::cout << board.toString();
    score = minimax(board, player::B, 0, 20, &bestMove, stats);
    std::cout << "AI chooses move to (" << bestMove.row << ", " << bestMove.col << ") with score " << score << "\n";
    std::cout << "stats:" << stats.nodesExplored << "\n";
    stats.nodesExplored = 0;
    board.makeMove(bestMove);
    std::cout << board.toString();



    return 0;
}