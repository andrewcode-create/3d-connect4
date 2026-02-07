#include "minimax.hpp" 
#include <vector>
#include <sstream>

struct TicTacToeMove {
    int row;
    int col;
    TicTacToeMove(int r, int c, char p) {
        row = r;
        col = c;
        player = p;
    }
    TicTacToeMove() = default;
    char player;
};


class TicTacToeBoard : public board_t<TicTacToeMove> {
private:
    char grid[3][3] = {' ',' ',' ',' ',' ',' ',' ',' ',' '};
    

public:
    TicTacToeBoard() = default;

    std::vector<TicTacToeMove> findMoves() override {
        // get current player by counting number of each player on board
        char player;
        int countA = 0;
        int countB = 0;
        for (auto& pr : grid) {
            for (auto& p : pr) {
                if (p == 'A') countA++;
                if (p == 'B') countB++;
            }
        }
        if (countA>countB) player = 'B';
        else player = 'A';

        // find all possible moves
        std::vector<TicTacToeMove> moves = std::vector<TicTacToeMove>();
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                if (grid[r][c] == ' ') {
                    moves.push_back(TicTacToeMove(r, c, player));
                }
            }
        }
        return moves;
    }

    void makeMove(TicTacToeMove m) override {
        grid[m.row][m.col] = m.player;
    }

    void undoMove(TicTacToeMove m) override {
        grid[m.row][m.col] = ' ';
    }
    player checkWin() override {
        // check rows
        for (int r = 0; r < 3; r++) {
            if (grid[r][0] != ' ' && grid[r][0] == grid[r][1] && grid[r][1] == grid[r][2]) {
                return (grid[r][0] == 'A' ? player::A : player::B);
            }
        }
        // check cols
        for (int c = 0; c < 3; c++) {
            if (grid[0][c] != ' ' && grid[0][c] == grid[1][c] && grid[1][c] == grid[2][c]) {
                return (grid[0][c] == 'A' ? player::A : player::B);
            }
        }
        // check diagonals
        if (grid[0][0] != ' ' && grid[0][0] == grid[1][1] && grid[1][1] == grid[2][2]) {
            return (grid[0][0] == 'A' ? player::A : player::B);
        }
        if (grid[0][2] != ' ' && grid[0][2] == grid[1][1] && grid[1][1] == grid[2][0]) {
            return (grid[0][2] == 'A' ? player::A : player::B);
        }
        return player::NONE;
    }
    double heuristic() override {
        // no heuristic, so return 0 for draw
        return 0;
    }
    std::string toString() override {
        return 
        std::string(1, grid[0][0]) + " | " + std::string(1, grid[0][1]) + " | " + std::string(1, grid[0][2]) + "\n" + 
        std::string(1, grid[1][0]) + " | " + std::string(1, grid[1][1]) + " | " + std::string(1, grid[1][2]) + "\n" + 
        std::string(1, grid[2][0]) + " | " + std::string(1, grid[2][1]) + " | " + std::string(1, grid[2][2]) + "\n";
    }
};