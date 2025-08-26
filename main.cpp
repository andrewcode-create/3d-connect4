//#include "tictactoe.hpp"
#include "3d-connect4-bitboard.hpp"
#include <iostream>

int main(int argc, char** argv) {

    connect3dBoard board;

    connect3dMove bestMove;// = connect3dMove(0,0,'Z');

    stat_t stats;

    if (argc != 2) {
        std::cout << "Usage: halfMovesToCheck\n";
        return 1;
    }

    int halfMovesToCheck = std::stoi(argv[1]);

    double score;

    for (int i = 0; i < 1; i++) {
        std::cout << "Move " << i+1 << '\n';
        stats.nodesExplored = 0;
        std::cout << "checking depth " << (int)(halfMovesToCheck+(i/5)) << "\n";
        score = minimax(board, (i % 2 == 0 ? player::A : player::B), 0, (int)(halfMovesToCheck+(i/5)), &bestMove, stats);
        //std::cout << "BLAHHHHH!";
        std::cout << "AI chooses move to (" << bestMove.move << ") with score " << score << "\n";
        std::cout << "This is row " << bestMove.row() << ", col " << bestMove.col() << ", level " << bestMove.level() << "\n";
        std::cout << "stats:" << stats.nodesExplored << "\n";
        
        board.makeMove(bestMove);
        std::cout << "board rep: " << board.boardA << "  " << board.boardB << "\n"; 
        std::cout << board.toString();

        if (board.checkWin(nullptr) != player::NONE) {
            std::cout << "Player " << (char)board.checkWin(nullptr) << " won! ---------------\n";
            break;
        }
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