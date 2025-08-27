#pragma once
#include <string>
#include <vector>
#include <optional>
#include <limits>
#include <cstdint>

// Enable alpha beta pruning. Disabling can be helpful to test speedups in move gen/checking
#define ALPHABETAPRUNING true

// Enable the transposition table. 
#define transpositionTableEnabled true


enum player {
    A='A', B='B', NONE=0
};

// Flag for transposition table. Exact is the exact best move, LOWER_BOUND is beta, UPPER_BOUND is alpha.
enum TTFlag { EXACT, LOWER_BOUND, UPPER_BOUND };

template<typename MoveType>
struct TTEntry {
    double score;
    uint8_t depth;
    uint8_t bestmove;
    uint8_t padding1;
    uint8_t padding2;
    TTFlag flag;
    uint64_t z_hash = 0;

    TTEntry(double newscore, uint8_t newdepth, uint8_t newbestmove, TTFlag newflag, uint64_t newz_hash) {
        score = newscore;
        depth = newdepth;
        bestmove = newbestmove;
        flag = newflag;
        z_hash = newz_hash;
    }

    TTEntry() {}
    //MoveType bestMove; // Optional: can be used for move ordering
};


// Abstract base class for a game board.
// Any game using this minimax library must inherit from this class
// and implement all pure virtual functions.
// the MoveType must implement the following:
// virtual bool isValid() = 0;
template<typename MoveType, int MaxBranch>
struct board_t {
    virtual ~board_t() = default;

    // returns an an array of all possible moves for the player's turn, ordered such that the most promising moves are first. 
    virtual std::array<MoveType, MaxBranch> findMoves(player play) = 0;
    // applies the move to the board
    virtual void makeMove(MoveType m) = 0;
    // undos the move
    virtual void undoMove(MoveType m) = 0;
    // checks whether a player won. Returns the player.
    virtual player checkWin(const MoveType* m) = 0;
    // assigns a heuristic score to the board, where positive is player 1 and negative is player 2. 
    // Values should be scaled between -1 and 1.
    virtual double heuristic() = 0;

    virtual uint64_t getHash() const = 0;

    virtual std::string toString() {
        return "NO GAME STR REPRESENTATION";
    }

};

struct stat_t {
    uint64_t nodesExplored = 0;
    uint64_t hashCollisions = 0;
};

template<typename MoveType, int MaxBranch>
double minimax(board_t<MoveType, MaxBranch>& board, player player, int halfMoveNum, int maxHalfMoveNum, 
    MoveType* bestMoveRet, stat_t& stats, std::vector<TTEntry<MoveType>> tt) {

    return minimax<MoveType>(board, player, halfMoveNum, maxHalfMoveNum, bestMoveRet, stats, -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), (MoveType*)nullptr, tt);
}


// Runs the minimax algorithm on a given board
// Returns the heuristic score of the best move.
// if bestMoveRet is not nullptr, populates it with the best move found.
template<typename MoveType, int MaxBranch>
double minimax(board_t<MoveType, MaxBranch>& board, player player, int halfMoveNum, int maxHalfMoveNum, 
    MoveType* bestMoveRet, stat_t& stats, double alpha, double beta, MoveType* lastMove, 
    std::vector<TTEntry<MoveType>>& tt) {

    
    
    #if transpositionTableEnabled
    // Transposition Table Lookup
    uint64_t hash = board.getHash();
    size_t index = hash % tt.size(); // Calculate index into the vector
    const TTEntry<MoveType>& entry = tt[index];
    //auto it = tt.find(hash);
    

    if (entry.z_hash == hash) {
        //const TTEntry<MoveType>& entry = it->second;
        // Check if the stored result is from a deep enough search
        if (entry.depth >= (maxHalfMoveNum - halfMoveNum)) {
            switch (entry.flag) {
                case EXACT:
                    // If we have an exact score, we can return it.
                    // We can't return bestMove as it's not stored in this version.
                    return entry.score;
                case LOWER_BOUND:
                    // This is a fail-high node, the score is at least this high.
                    alpha = std::max(alpha, entry.score);
                    break;
                case UPPER_BOUND:
                    // This is a fail-low node, the score is at most this high.
                    beta = std::min(beta, entry.score);
                    break;
            }
            if (alpha >= beta) {
                return entry.score; // We can cause a cutoff.
            }
        }
    }

    // end transposition table lookup
    #endif

    auto pwin = board.checkWin(lastMove);

    if (pwin != player::NONE) {
        return (pwin == player::A ? 1 : -1) * 1000 * (1-halfMoveNum*0.001);
    }

    if (halfMoveNum >= maxHalfMoveNum) {
        // depth limit
        return board.heuristic();
    }



    std::array<MoveType, MaxBranch> moves = board.findMoves(player);

    // check for draw by no moves left
    if (!moves[0].isValid()) {
        return 0;
    }

    // Keep a copy of the original alpha to determine the node type later
    double original_alpha = alpha;

    MoveType bestmove = moves[0];
    double bestscore;

    if (player == player::A) {
        // set worst score so that first move is always taken
        bestscore = -std::numeric_limits<double>::infinity();

        // go through moves
        for(int i = 0; i < moves.size() && moves[i].isValid(); i++) {
            stats.nodesExplored++;
            // check the move
            board.makeMove(moves[i]);
            double newscore = minimax<MoveType>(board, player::B, halfMoveNum+1, maxHalfMoveNum, nullptr, stats, alpha, beta, &moves[i], tt);
            board.undoMove(moves[i]);
            // update score and move if needed
            if (bestscore < newscore) {
                bestscore = newscore;
                bestmove = moves[i];
            }

            #if ALPHABETAPRUNING
            // update alpha
            alpha = std::max(alpha, bestscore);
            if (alpha >= beta) {
                break; // Prune the remaining branches
            }
            #endif
        }
    }
    if (player == player::B) {
        // set worst score so that first move is always taken
        bestscore = std::numeric_limits<double>::infinity();

        // go through moves
        for(int i = 0; i < moves.size() && moves[i].isValid(); i++) {
            stats.nodesExplored++;
            // check the move
            board.makeMove(moves[i]);
            double newscore = minimax<MoveType>(board, player::A, halfMoveNum+1, maxHalfMoveNum, nullptr, stats, alpha, beta, &moves[i], tt);
            board.undoMove(moves[i]);
            // update score and move if needed
            if (bestscore > newscore) {
                bestscore = newscore;
                bestmove = moves[i];
            }

            #if ALPHABETAPRUNING
            // update beta
            beta = std::min(beta, bestscore);
            if (beta <= alpha) {
                break; // Prune the remaining branches
            }
            #endif
        }
    }

    #if transpositionTableEnabled


    // Use a depth-preferred replacement strategy. We only replace an entry
    // if the new result is from a deeper search, which is more valuable
    // or if 

    int current_search_depth = maxHalfMoveNum - halfMoveNum;

    TTEntry<MoveType> new_entry(bestscore, current_search_depth, halfMoveNum, EXACT, hash);
    if (bestscore <= original_alpha) {
        new_entry.flag = UPPER_BOUND;
    } else if (bestscore >= beta) {
        new_entry.flag = LOWER_BOUND;
    } else {
        new_entry.flag = EXACT;
    }
    

    if (tt[index].z_hash != 0 && tt[index].z_hash != hash) {

        stats.hashCollisions++;
        if (entry.depth <= new_entry.depth || index > tt.size()/2) {
            tt[index] = new_entry; 
        }
        // Overwrite the entry at the calculated index
    } else {
        tt[index] = new_entry; 
    }
    



    // Transposition Table Store
    /*
    TTEntry<MoveType> new_entry;
    new_entry.score = bestscore;
    new_entry.depth = maxHalfMoveNum - halfMoveNum;
    
    if (bestscore <= original_alpha) {
        new_entry.flag = UPPER_BOUND; // Fail-low, score is an upper bound.
    } else if (bestscore >= beta) {
        new_entry.flag = LOWER_BOUND; // Fail-high, score is a lower bound.
    } else {
        new_entry.flag = EXACT; // Score is exact.
    }
    tt[hash] = new_entry;*/
    // End of TT Store 
    #endif

    if (bestMoveRet != nullptr) *bestMoveRet = bestmove;
    return bestscore;

}

