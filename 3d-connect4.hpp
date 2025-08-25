#include "minimax.hpp" 
#include <vector>
#include <sstream>

struct connect3dMove {
    int row;
    int col;
    char player;
    connect3dMove(int r, int c, char p) {
        row = r;
        col = c;
        player = p;
    }
    connect3dMove() = default;
};


class connect3dBoard : public board_t<connect3dMove> {
private:
    // row, col, depth (bottom is 0). Viewed from the top.
    char grid[4][4][4] = {'-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', };
    
    // returns 0 or null if not the same, otherwise returns the char they are
    inline char compare4(char a, char b, char c, char d) {
        if (a==b&&b==c&&c==d) return a;
        else return 0;
    }

public:
    connect3dBoard() = default;

    std::vector<connect3dMove> findMoves(player play) override {
        /*
        // get current player by counting number of each player on board
        char player;
        int countA = 0;
        int countB = 0;
        for (auto& r : grid) {
            for (auto& c : r) {
                for (auto& d : c){
                    if (d == 'A') countA++;
                    if (d == 'B') countB++;
                }
            }
        }
        if (countA>countB) player = 'B';
        else player = 'A';
        */
        char p = 'A';
        if (play == player::B) p = 'B';

        // find all possible moves
        std::vector<connect3dMove> moves = std::vector<connect3dMove>();
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (grid[r][c][3] == '-') {
                    moves.push_back(connect3dMove(r, c, p));
                }
            }
        }
        return moves;
    }

    void makeMove(connect3dMove m) override {
        auto& stack = grid[m.row][m.col];
        for (int d = 0; d < 4; d++) {
            if (stack[d] == '-') {
                stack[d] = m.player;
                break;
            }
        }
    }

    void undoMove(connect3dMove m) override {
        auto& stack = grid[m.row][m.col];
        for (int d = 3; d >= 0; d--) {
            if (stack[d] != '-') {
                stack[d] = '-';
                break;
            }
        }
    }
    player checkWin(const connect3dMove& m) override {
        // do the folowing for every depth level, starting with bottom.
        for (int d = 0; d < 4; d++) {
            // check rows for lines
            for (int r = 0; r < 4; r++) {
                char res = compare4(grid[r][0][d],grid[r][1][d],grid[r][2][d],grid[r][3][d]);
                if (res == 'A') return player::A;
                else if (res == 'B') return player::B; 
            }

            // check cols for lines
            for (int c = 0; c < 4; c++) {
                char res = compare4(grid[0][c][d],grid[1][c][d],grid[2][c][d],grid[3][c][d]);
                if (res == 'A') return player::A;
                else if (res == 'B') return player::B; 
            }

            // check diagonals for lines
            {
                char res = compare4(grid[0][0][d],grid[1][1][d],grid[2][2][d],grid[3][3][d]);
                if (res == 'A') return player::A;
                else if (res == 'B') return player::B; 
            }
            {
                char res = compare4(grid[0][3][d],grid[1][2][d],grid[2][1][d],grid[3][0][d]);
                if (res == 'A') return player::A;
                else if (res == 'B') return player::B; 
            }
        }
        

        // check rows for stairs
        for (int r = 0; r < 4; r++) {
            {
                char res = compare4(grid[r][0][0],grid[r][1][1],grid[r][2][2],grid[r][3][3]);
                if (res == 'A') return player::A;
                else if (res == 'B') return player::B; 
            }
            {
                char res = compare4(grid[r][0][3],grid[r][1][2],grid[r][2][1],grid[r][3][0]);
                if (res == 'A') return player::A;
                else if (res == 'B') return player::B; 
            }
        }

        // check cols for stairs
        for (int c = 0; c < 4; c++) {
            {
                char res = compare4(grid[0][c][0],grid[1][c][1],grid[2][c][2],grid[3][c][3]);
                if (res == 'A') return player::A;
                else if (res == 'B') return player::B; 
            }
            {
                char res = compare4(grid[0][c][3],grid[1][c][2],grid[2][c][1],grid[3][c][0]);
                if (res == 'A') return player::A;
                else if (res == 'B') return player::B; 
            }
        }

        // check diagonals for stairs
        {
            char res = compare4(grid[0][0][0],grid[1][1][1],grid[2][2][2],grid[3][3][3]);
            if (res == 'A') return player::A;
            else if (res == 'B') return player::B; 
        }
        {
            char res = compare4(grid[0][0][3],grid[1][1][2],grid[2][2][1],grid[3][3][0]);
            if (res == 'A') return player::A;
            else if (res == 'B') return player::B; 
        }
        {
            char res = compare4(grid[0][3][0],grid[1][2][1],grid[2][1][2],grid[3][0][3]);
            if (res == 'A') return player::A;
            else if (res == 'B') return player::B; 
        }
        {
            char res = compare4(grid[0][3][3],grid[1][2][2],grid[2][1][1],grid[3][0][0]);
            if (res == 'A') return player::A;
            else if (res == 'B') return player::B; 
        }

        // check for stacks
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                char res = compare4(grid[r][c][0],grid[r][c][1],grid[r][c][2],grid[r][c][3]);
                if (res == 'A') return player::A;
                else if (res == 'B') return player::B; 
            }
        }

        return player::NONE;
    }
    double heuristic() override {
        // no heuristic, so return 0 for draw
        return 0;
    }
    std::string toString() override {
        std::stringstream ss;
        ss << "3D Connect Four Board (view from top):\n\n";

        // Iterate through each layer, from top (d=3) to bottom (d=0)
        for (int d = 3; d >= 0; d--) {
            ss << "Layer " << d + 1;
            if (d == 3) ss << " (Top)";
            if (d == 0) ss << " (Bottom)";
            ss << "\n  ---------------\n";
            
            // Iterate through rows for the current layer
            for (int r = 0; r < 4; r++) {
                ss << r << " | ";
                // Iterate through columns for the current row
                for (int c = 0; c < 4; c++) {
                    ss << grid[r][c][d] << " ";
                }
                ss << "|\n";
            }
            ss << "  ---------------\n";
            ss << "    0 1 2 3\n\n"; // Column headers
        }
        return ss.str();
    }
};