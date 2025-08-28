//#include "tictactoe.hpp"
#include "3d-connect4-bitboard.hpp"
#include <iostream>

uint64_t countEntries(std::vector<TTEntry<connect3dMove>>& tt) {
    uint64_t ret = 0;
    for (auto& t : tt) {
        if (t.z_hash!=0) {
            ret++;
        }
    }
    return ret;
}

int main(int argc, char** argv) {

    connect3dBoard board;

    connect3dMove bestMove;// = connect3dMove(0,0,'Z');

    stat_t stats;

    if (argc != 2) {
        std::cout << "Usage: halfMovesToCheck\n";
        return 1;
    }

    int halfMovesToCheck = std::stoi(argv[1]);


    // Decide on a size in Megabytes.
    const size_t tt_size_mb = 24; // Use 128 MB for the table
    // Calculate how many entries can fit in that memory space.
    const size_t tt_entry_count = (tt_size_mb * 1024 * 1024) / sizeof(TTEntry<connect3dMove>);

    // Create the transposition table as a vector of that size.
    // All entries are zero-initialized by default.
    std::vector<TTEntry<connect3dMove>> tt(tt_entry_count);
    
    std::cout << "Transposition Table sized for " << tt_entry_count << " entries (" << tt_size_mb << " MB).\n";

    double score;

    for (int i = 0; i < 1 || false; i++) {
        stats.nodesExplored = 0;
        stats.hashCollisions = 0;
        std::cout << "Move " << i+1 << '\n';
        std::cout << "checking depth " << (int)(halfMovesToCheck) << "\n";
        std::cout.flush();
        //tt.assign(tt_entry_count, {}); 
        // attempt itterative deepening through transposition table, search for immidiate winning moves / good scores now
        score = minimax(board, (i % 2 == 0 ? player::A : player::B), 0, (int)(halfMovesToCheck-4), &bestMove, stats, tt, true);
        std::cout << "nodes:      " << stats.nodesExplored << "\n";
        std::cout << "collisions: " << stats.hashCollisions << "\n";
        std::cout << "percent collided: " << stats.hashCollisions/(double)stats.nodesExplored*100 << "\n";
        std::cout << "percent filled:   " << countEntries(tt)/(double)tt_entry_count*100 << "\n";
        stats.nodesExplored = 0;
        stats.hashCollisions = 0;
        // real
        score = minimax(board, (i % 2 == 0 ? player::A : player::B), 0, (int)(halfMovesToCheck), &bestMove, stats, tt, false);
        //std::cout << "BLAHHHHH!";
        std::cout << "AI chooses move to (" << bestMove.move << ") with score " << score << "\n";
        std::cout << "This is row " << bestMove.row() << ", col " << bestMove.col() << ", level " << bestMove.level() << "\n";
        std::cout << "nodes:      " << stats.nodesExplored << "\n";
        std::cout << "collisions: " << stats.hashCollisions << "\n";
        std::cout << "percent collided: " << stats.hashCollisions/(double)stats.nodesExplored*100 << "\n";
        std::cout << "percent filled:   " << countEntries(tt)/(double)tt_entry_count*100 << "\n";
        
        board.makeMove(bestMove);
        std::cout << "board rep: " << board.boardA << "  " << board.boardB << "\n"; 
        std::cout << board.toString();

        if (board.checkWin(nullptr) != player::NONE) {
            std::cout << "Player " << (char)board.checkWin(nullptr) << " won! ---------------\n";
            break;
        }
        //char age = 0;
        //std::cin >> age;
        //std::cout << age;
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