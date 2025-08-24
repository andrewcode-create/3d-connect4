#include "tictactoe.hpp"
#include <iostream>

int main() {
    TicTacToeBoard board;
    player current_player = player::A;


    TicTacToeMove bestMove = TicTacToeMove(2,2,'Z');
    stat_t stats;

    board.makeMove(TicTacToeMove(0,0,'A'));

    double score = minimax(board, current_player, 0, 20, &bestMove, stats);

    std::cout << "AI chooses move to (" << bestMove.row << ", " << bestMove.col << ") with score " << score << "\n";
    std::cout << "stats:" << stats.nodesExplored << "\n";
    board.makeMove(bestMove);
    std::cout << board.toString();



    return 0;
}