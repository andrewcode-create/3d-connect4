//#include "tictactoe.hpp"
#include "3d-connect4.hpp"
#include <iostream>

int main() {

    connect3dBoard board;

    connect3dMove bestMove = connect3dMove(0,0,'Z');

    stat_t stats;

    int halfMovesToCheck = 8;

    double score;

    for (int i = 0; i < 1; i++) {
        std::cout << "Move " << i+1 << '\n';
        stats.nodesExplored = 0;
        score = minimax(board, (i % 2 == 0 ? player::A : player::B), 0, halfMovesToCheck, &bestMove, stats);
        std::cout << "AI chooses move to (" << bestMove.row << ", " << bestMove.col << ") with score " << score << "\n";
        std::cout << "stats:" << stats.nodesExplored << "\n";
        board.makeMove(bestMove);
        std::cout << board.toString();
    }




    #if false
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
    #endif



    return 0;
}