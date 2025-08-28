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

// Enable basic performance statistics
#define statisticsEnabled true

enum player {
    A='A', B='B', NONE=0
};

// Flag for transposition table. Exact is the exact best move, LOWER_BOUND is beta, UPPER_BOUND is alpha.
enum TTFlag { EXACT, LOWER_BOUND, UPPER_BOUND };

// this is the type for transposition table entries
template<typename MoveType>
struct TTEntry {
    double score;
    uint8_t depth; // the depth searched to
    uint8_t bestmove; // the best move found in this position
    uint8_t ply; // number of pieces on the board
    uint8_t extraSpace; // unused, here because of 64-bit alignment 
    TTFlag flag; // type of score
    uint64_t z_hash = 0; // the hash of the position

    TTEntry(double newscore, uint8_t newdepth, uint8_t newbestmove, TTFlag newflag, uint64_t newz_hash, uint8_t newply) {
        score = newscore;
        depth = newdepth;
        bestmove = newbestmove;
        flag = newflag;
        z_hash = newz_hash;
        ply = newply;
    }

    TTEntry() {}
};


// Abstract base class for a game board.
// Any game using this minimax library must inherit from this class
// and implement all pure virtual functions.
// the MoveType must implement the following:
// virtual bool isValid() = 0;
// the MaxBranch is the maximum branching factor for the game
template<typename MoveType, int MaxBranch>
struct board_t {
    virtual ~board_t() = default;

    // returns an an array of all possible moves for the player's turn, ordered such that the most promising moves are first. 
    //virtual std::array<MoveType, MaxBranch> findMoves(player play) = 0;
    // it is highly recomended to put bestMove first if it is valid
    virtual std::array<MoveType, MaxBranch> findMoves(player play, MoveType bestMove) = 0;
    // applies the move to the board
    virtual void makeMove(MoveType m) = 0;
    // undos the move
    virtual void undoMove(MoveType m) = 0;
    // checks whether a player won. Returns the player or player::NONE
    virtual player checkWin(MoveType* m) = 0;
    // assigns a heuristic score to the board, where positive is player A and negative is player B. 
    // Values should be scaled between -1 and 1.
    virtual double heuristic() = 0;

    
    #if transpositionTableEnabled
    // get a hash of the position for transposition table
    virtual uint64_t getHash() const = 0;
    #endif

    // for printing the board in human readable format
    virtual std::string toString() {
        return "NO GAME STR REPRESENTATION";
    }

};

// statistics type
struct stat_t {
    uint64_t nodesExplored = 0; // total leaf nodes
    uint64_t hashCollisions = 0; // transposition table collisions
};

// overloaded function. This should be called from main.
#if transpositionTableEnabled
template<typename MoveType, int MaxBranch>
double minimax(board_t<MoveType, MaxBranch>& board, player player, int halfMoveNum, int maxHalfMoveNum, 
    MoveType* bestMoveRet, stat_t& stats, std::vector<TTEntry<MoveType>>& tt, bool preload=false) {

    return minimax<MoveType>(board, player, halfMoveNum, maxHalfMoveNum, bestMoveRet, stats, -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), (MoveType*)nullptr, tt, preload);
}
#else
template<typename MoveType, int MaxBranch>
double minimax(board_t<MoveType, MaxBranch>& board, player player, int halfMoveNum, int maxHalfMoveNum, 
    MoveType* bestMoveRet, stat_t& stats) {

    return minimax<MoveType>(board, player, halfMoveNum, maxHalfMoveNum, bestMoveRet, stats, -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), (MoveType*)nullptr);
}
#endif


// Runs the minimax algorithm on a given board
// Returns the heuristic score of the best move.
// if bestMoveRet is not nullptr, populates it with the best move found.
template<typename MoveType, int MaxBranch>
double minimax(board_t<MoveType, MaxBranch>& board, player player, int halfMoveNum, int maxHalfMoveNum, 
    MoveType* bestMoveRet, stat_t& stats, double alpha, double beta, MoveType* lastMove
    #if transpositionTableEnabled
    , std::vector<TTEntry<MoveType>>& tt
    #endif 
    , bool preload) {

    
    
    #if transpositionTableEnabled
    // Transposition Table Lookup
    uint64_t hash = board.getHash(); // get a hash of the board
    size_t index = hash % tt.size(); // Calculate index into the vector
    const TTEntry<MoveType>& entry = tt[index];

    MoveType bestmove;
    

    // if the entry is valid or random collision
    if (entry.z_hash == hash) {
        bestmove = MoveType(entry.bestmove);
        // Check if the stored result is from a deep enough search
        if (entry.depth >= (maxHalfMoveNum - halfMoveNum)) {
            switch (entry.flag) {
                case EXACT:
                    // If we have an exact score, we can return it.
                    if (bestMoveRet != nullptr) {
                        *bestMoveRet = MoveType(entry.bestmove);
                    }
                    return entry.score;
                case LOWER_BOUND:
                    // This is a fail-high node, the score is at least this high.
                    // TODO: 
                    alpha = std::max(alpha, entry.score);
                    //bestmove = MoveType(entry.bestmove);
                    break;
                case UPPER_BOUND:
                    // This is a fail-low node, the score is at most this high.
                    beta = std::min(beta, entry.score);
                    //bestmove = MoveType(entry.bestmove);
                    break;
            }
            if (alpha >= beta) {
                return entry.score; // We can cause a cutoff.
            }
        }
    } else {
        bestmove = MoveType();
    }

    // end transposition table lookup
    #endif

    #if not transpositionTableEnabled
    MoveType bestmove = MoveType();
    #endif

    // check if someone won
    auto pwin = board.checkWin(lastMove);

    // if someone won, return a score of 1000 minus 1 per move away it is
    // this incentivizes earlier wins
    if (pwin != player::NONE) {
        return (pwin == player::A ? 1 : -1) * 1000 * (1-halfMoveNum*0.001);
    }

    // depth limit check
    if (halfMoveNum >= maxHalfMoveNum) {
        return board.heuristic();
    }


    // find the moves that can be made
    std::array<MoveType, MaxBranch> moves = board.findMoves(player, bestmove);

    // check for draw by no moves left
    if (!moves[0].isValid()) {
        return 0;
    }

    // Keep a copy of the original alpha and beta to determine the node type later
    double original_alpha = alpha;
    double origional_beta = beta;

    
    double bestscore;

    if (player == player::A) {
        // set worst score so that first move is always taken
        bestscore = -std::numeric_limits<double>::infinity();

        // go through moves
        for(int i = 0; i < moves.size() && moves[i].isValid(); i++) {
            #if statisticsEnabled
            stats.nodesExplored++;
            #endif
            // check the move
            board.makeMove(moves[i]);
            double newscore = minimax<MoveType>(board, player::B, halfMoveNum+1, maxHalfMoveNum, nullptr, stats, alpha, beta, &moves[i], tt, preload);
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
            #if statisticsEnabled
            stats.nodesExplored++;
            #endif

            // check the move
            board.makeMove(moves[i]);
            double newscore = minimax<MoveType>(board, player::A, halfMoveNum+1, maxHalfMoveNum, nullptr, stats, alpha, beta, &moves[i], tt, preload);
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

    // calculate current search depth
    int current_search_depth = maxHalfMoveNum - halfMoveNum;

    // create an entry for the table
    TTEntry<MoveType> new_entry(bestscore, current_search_depth, bestmove.deflate(), EXACT, hash, 0);

    // set the flag and type for the new table
    if (bestscore <= original_alpha) {
        // The result failed to beat the maximizer's guaranteed floor.
        // This is a fail-low, so the score is an UPPER_BOUND.
        // this only happens if this move is pruned
        new_entry.flag = UPPER_BOUND;
    } else if (bestscore >= origional_beta) {
        // The result failed to stay under the minimizer's guaranteed ceiling.
        // This is a fail-high, so the score is a LOWER_BOUND.
        // this only happens if this moved is pruned
        new_entry.flag = LOWER_BOUND;
    } else {
        // The score fell neatly within the original (alpha, beta) window.
        // this is an exact score under win and heuristics
        new_entry.flag = EXACT;
    }
    
    // if this is a hash collision
    if (tt[index].z_hash != 0 && tt[index].z_hash != hash) {

        #if statisticsEnabled
        stats.hashCollisions++;
        #endif
        /* Store the result in one of two cases:
         * 1. The search depth of the new result is higher or equal. 
         *      There are relativley few of these, but they prune a lot, so 
         *      we always want to store them.
         * 2. This is the second half of the table. This splits the table into two parts:
         *      the 'depth' part and the 'leaf' part. The leaf part is used much more
         *      frequently but pruns little. The depth part prunes a lot but is used not often.
         *  THIS WAS EXPERIMENTALLY DETERMINED TO BE FASTER!!!
        */
        if (entry.depth < new_entry.depth || (index < tt.size()/2 && !preload)) {
            tt[index] = new_entry; 
        }
    } else {
        // not a hash collision, so write to the table
        tt[index] = new_entry; 
    }
    
    #endif

    // out the best move if created, then return the score
    if (bestMoveRet != nullptr) *bestMoveRet = bestmove;
    return bestscore;

}

