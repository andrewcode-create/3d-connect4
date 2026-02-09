#pragma once
#include "ai.hpp"
#include <string>
#include <vector>
#include <optional>
#include <limits>
#include <cstdint>

// Enable alpha beta pruning. Disabling can be helpful to test speedups in move gen/checking
#define ALPHABETAPRUNING true

// Enable the transposition table. 
#define transpositionTableEnabled false

// Enable basic performance statistics
#define statisticsEnabled true

// Flag for transposition table. Exact is the exact best move, LOWER_BOUND is beta, UPPER_BOUND is alpha.
enum TTFlag { EXACT, LOWER_BOUND, UPPER_BOUND };

namespace mm1 {
// this is the type for transposition table entries
template<typename MoveType>
struct TTEntry {
    double score;
    uint8_t depth; // the depth searched to
    uint8_t bestmove; // the best move found in this position
    //uint8_t ply; // number of pieces on the board
    uint16_t extraSpace = 0;
    TTFlag flag; // type of score
    uint64_t z_hash = 0; // the hash of the position

    TTEntry(double newscore, uint8_t newdepth, uint8_t newbestmove, TTFlag newflag, uint64_t newz_hash, uint8_t newply) {
        score = newscore;
        depth = newdepth;
        bestmove = newbestmove;
        flag = newflag;
        z_hash = newz_hash;
        //ply = newply;
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
    // checks whether a player won given last move was m. Returns the player or player::NONE
    virtual player checkWin(MoveType* m) const = 0;
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

    return minimax<MoveType>(board, player, halfMoveNum, maxHalfMoveNum, bestMoveRet, stats, -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), (MoveType*)nullptr, false);
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

    uint64_t hash = board.getHash();
    size_t primary_index = hash % tt.size();
    size_t secondary_index = (hash ^ 0x4BC) % tt.size();

    TTEntry<MoveType>* entry_ptr = nullptr;

    // 1. Check the primary slot
    if (tt[primary_index].z_hash == hash) {
        entry_ptr = &tt[primary_index];
    } 
    // 2. If primary misses, check the secondary slot
    else if (tt[secondary_index].z_hash == hash) {
        entry_ptr = &tt[secondary_index];
    }

    MoveType bestmove;

    if (entry_ptr != nullptr) {
    TTEntry<MoveType>& entry = *entry_ptr;
    bestmove = MoveType(entry.bestmove);
    
        // Check if the stored result is from a deep enough search
        if (entry.depth >= (maxHalfMoveNum - halfMoveNum)) {
            switch (entry.flag) {
                case EXACT:
                    if (bestMoveRet != nullptr) {
                        *bestMoveRet = MoveType(entry.bestmove);
                    }
                    return entry.score;
                case LOWER_BOUND:
                    alpha = std::max(alpha, entry.score);
                    break;
                case UPPER_BOUND:
                    beta = std::min(beta, entry.score);
                    break;
            }
            if (alpha >= beta) {
                return entry.score; // We can cause a cutoff.
            }
        } else {
            bestmove = MoveType();
        }
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
            double newscore = minimax<MoveType>(board, player::B, halfMoveNum+1, maxHalfMoveNum, nullptr, stats, alpha, beta, &moves[i]
            #if transpositionTableEnabled
            , tt
            #endif
            , preload);
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
            double newscore = minimax<MoveType>(board, player::A, halfMoveNum+1, maxHalfMoveNum, nullptr, stats, alpha, beta, &moves[i]
            #if transpositionTableEnabled
            , tt
            #endif
            , preload);
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

    // Determine the type of entry based on the score relative to the alpha-beta bounds.
    TTFlag flag_to_store = EXACT;
    if (bestscore <= original_alpha) {
        flag_to_store = UPPER_BOUND; // Fail-low, score is an upper bound.
    } else if (bestscore >= origional_beta) {
        flag_to_store = LOWER_BOUND; // Fail-high, score is a lower bound.
    }

    // Calculate the remaining search depth for this entry.
    int current_search_depth = maxHalfMoveNum - halfMoveNum;

    // Create the new entry for the table.
    TTEntry<MoveType> new_entry(bestscore, current_search_depth, bestmove.deflate(), flag_to_store, hash, halfMoveNum);

    // Calculate primary and secondary indices for the hash.
    //primary_index = hash % tt.size();
    //secondary_index = (hash ^ 0x4BC) % tt.size();

    TTEntry<MoveType>& primary_entry = tt[primary_index];
    TTEntry<MoveType>& secondary_entry = tt[secondary_index];

    // Scenario 1: The primary slot is empty or already stores an entry for the same position.
    // In this case, we use the primary slot.
    if (primary_entry.z_hash == 0 || primary_entry.z_hash == hash) {
        primary_entry = new_entry;
    }
    // Scenario 2: The secondary slot is empty or for the same position.
    // We use the secondary slot.
    else if (secondary_entry.z_hash == 0 || secondary_entry.z_hash == hash) {
        secondary_entry = new_entry;
    }
    // Scenario 3: Both slots are occupied by entries for different positions (a double collision).
    // We must decide which of the two existing entries is better to replace.
    // The "Always Replace" scheme replaces the entry from the shallower search.
    else {
        #if statisticsEnabled
        stats.hashCollisions++;
        #endif
        if (primary_entry.depth <= secondary_entry.depth) {
            // The primary entry is less valuable or equally valuable, so we replace it.
            primary_entry = new_entry;
        } else {
            // The secondary entry is less valuable, so we replace it.
            secondary_entry = new_entry;
        }
    }
    
    #endif

    // out the best move if created, then return the score
    if (bestMoveRet != nullptr) *bestMoveRet = bestmove;
    return bestscore;

}
}
