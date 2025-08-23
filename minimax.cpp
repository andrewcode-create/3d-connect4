#define MaxBranch = 16

int main() {


}

struct move {

};

struct ret {
    move move;
    double score;
};

enum player {
    A=1, B=2, NONE=0
};



struct board {


    // returns an an array of all possible moves for the player's turn, ordered such that the most promising moves are first. 
    // Length is always MaxBranch
    move* findMoves() {

    }
    
//dd
    // applies the move to the board
    void makeMove(move m) {

    }

    // undos the move
    void undoMove(move m) {

    }

    // checks whether a player won. Returns the player.
    player checkWin() {

    }

    // assigns a heuristic score to the board, where positive is player 1 and negative is player 2
    double heuristic() {
        
    }



};


// runs minimax
ret minimax(board& board, player player, int halfMoveNum, int maxhalfMoveNum) {



}

