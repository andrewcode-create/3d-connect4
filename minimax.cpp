#define MaxBranch 16

int main() {

    return 1;
}

//must include an invalid state
struct move_t {

    bool isValid() {
        return true;
    }
};

struct ret_t {
    move_t move;
    double score;
};

enum player {
    A=1, B=2, NONE=0
};



struct board_t {


    // returns an an array of all possible moves for the player's turn, ordered such that the most promising moves are first. 
    // Length is always MaxBranch
    move_t* findMoves() {
        return nullptr;
    }
    
    // applies the move to the board
    void makeMove(move_t m) {

    }

    // undos the move
    void undoMove(move_t m) {

    }

    // checks whether a player won. Returns the player.
    player checkWin() {
        return player::NONE;
    }

    // assigns a heuristic score to the board, where positive is player 1 and negative is player 2
    double heuristic() {
        return 0;
    }



};


// runs minimax
ret_t minimax(board_t& board, player player, int halfMoveNum, int maxhalfMoveNum) {

    auto pwin = board.checkWin();

    if (pwin != player::NONE) {
        ret_t ret;
        ret.score = (pwin == player::A ? 1 : -1);
        return ret;
    }

    ret_t bestmove;

    move_t* moves = board.findMoves();

    for(int i = 0; i < MaxBranch; i++) {

        if (!moves[i].isValid()) {
            //move is not valid, so went thorugh all valid moves, break.
            break;
        }

        board.makeMove(moves[i]);
        if (player == player::A) {
            ret_t newmove = minimax(board, player::B, halfMoveNum+1, maxhalfMoveNum);
            if ( bestmove.move.isValid() || bestmove.score < newmove.score) {
                bestmove = newmove;
            }
        }
        if (player == player::B) {
            ret_t newmove = minimax(board, player::A, halfMoveNum+1, maxhalfMoveNum);
            if ( bestmove.move.isValid() || bestmove.score > newmove.score) {
                bestmove = newmove;
            }
        }
        return bestmove;
    }

}

