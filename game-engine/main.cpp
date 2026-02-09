#include <iostream>
#include <memory>
#include <limits>
#include <vector>
#include <functional>
#include <string>
#include <chrono>
#include <future>
#include <thread>

#include "3d-connect4-board.hpp"
#include "random_ai.hpp"
#include "human_play.hpp"
#include "minimax_ai_b1_v1.hpp"
#include "minimax_ai_b1_v2.hpp"
#include "minimax_ai_b2_v1.hpp"
#include "minimax_ai_b2_v2.hpp"
#include "minimax_ai_b3_v1.hpp"
#include "minimax_ai_b3_v2.hpp"
#include "minimax_ai_b4_v1.hpp"
#include "minimax_ai_b5_v1.hpp"
#include "minimax_ai_b5_v2.hpp"
#include "heuristic_bot.hpp"

struct PlayerOption {
    std::string name;
    std::function<std::unique_ptr<AI_base>()> factory;
};

const std::vector<PlayerOption> playerOptions = {
    {"Human", []() { return std::make_unique<HumanPlayer>(); }},
    {"Random AI", []() { return std::make_unique<RandomAI>(); }},
    {"Minimax AI b1 v1", []() { return std::make_unique<MinimaxAI_b1_v1>(); }},
    {"Minimax AI b1 v2", []() { return std::make_unique<MinimaxAI_b1_v2>(); }},
    {"Minimax AI b2 v1", []() { return std::make_unique<MinimaxAI_b2_v1>(); }},
    {"Minimax AI b2 v2", []() { return std::make_unique<MinimaxAI_b2_v2>(); }},
    {"Minimax AI b3 v1", []() { return std::make_unique<MinimaxAI_b3_v1>(); }},
    {"Minimax AI b3 v2", []() { return std::make_unique<MinimaxAI_b3_v2>(); }},
    {"Minimax AI b4 v1", []() { return std::make_unique<MinimaxAI_b4_v1>(); }},
    {"Minimax AI b5 v1", []() { return std::make_unique<MinimaxAI_b5_v1>(); }},
    {"Minimax AI b5 v2", []() { return std::make_unique<MinimaxAI_b5_v2>(); }},
    {"Heuristic Bot", []() { return std::make_unique<HeuristicBot>(); }}
};

int getPlayerChoice(const std::string& playerName) {
    std::cout << "Select " << playerName << ":" << std::endl;
    for (size_t i = 0; i < playerOptions.size(); ++i) {
        std::cout << (i + 1) << ". " << playerOptions[i].name << std::endl;
    }

    int choice;
    while (true) {
        std::cout << "Enter choice (1-" << playerOptions.size() << "): ";
        if (std::cin >> choice && choice >= 1 && choice <= static_cast<int>(playerOptions.size())) {
            return choice - 1;
        }
        std::cout << "Invalid input. Please try again." << std::endl;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

int main() {
    std::cout << "3D Connect 4 Game Engine" << std::endl;
    std::cout << "========================" << std::endl;

    int playerAIdx = getPlayerChoice("Player A");
    int playerBIdx = getPlayerChoice("Player B");

    bool isHumanA = (dynamic_cast<HumanPlayer*>(playerOptions[playerAIdx].factory().get()) != nullptr);
    bool isHumanB = (dynamic_cast<HumanPlayer*>(playerOptions[playerBIdx].factory().get()) != nullptr);
    int numGames = 1;

    if (!isHumanA && !isHumanB) {
        while (true) {
            std::cout << "Enter number of games to play (1 for interactive, >1 for silent simulation): ";
            if (std::cin >> numGames && numGames >= 1) {
                break;
            }
            std::cout << "Invalid input. Please enter an integer >= 1." << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }

    if (numGames > 1) {
        int winsA = 0, winsB = 0, draws = 0;
        double totalTimeA = 0, totalTimeB = 0;
        uint64_t totalNodesA = 0, totalNodesB = 0;
        uint64_t totalCollisionsA = 0, totalCollisionsB = 0;
        std::cout << "Simulating " << numGames << " games..." << std::endl;

        struct SimResult {
            int winsA = 0; int winsB = 0; int draws = 0;
            double timeA = 0; double timeB = 0;
            uint64_t nodesA = 0; uint64_t nodesB = 0;
            uint64_t collisionsA = 0; uint64_t collisionsB = 0;
        };

        auto runGames = [&](int count, bool swap) -> SimResult {
            SimResult res;
            int p1Idx = swap ? playerBIdx : playerAIdx;
            int p2Idx = swap ? playerAIdx : playerBIdx;

            for (int i = 0; i < count; ++i) {
                connect3dBoard board;
                auto playerA = playerOptions[p1Idx].factory();
                auto playerB = playerOptions[p2Idx].factory();

                while (true) {
                    player winner = board.checkWin();
                    if (winner != player::NONE) {
                        if (winner == player::A) {
                            if (!swap) res.winsA++; else res.winsB++;
                        } else {
                            if (!swap) res.winsB++; else res.winsA++;
                        }
                        break;
                    }
                    if (board.findMoves().empty()) {
                        res.draws++;
                        break;
                    }

                    AI_base* currentPlayer = (board.getPlayerTurn() == player::A) ? playerA.get() : playerB.get();
                    try {
                        auto start = std::chrono::high_resolution_clock::now();
                        auto ret = currentPlayer->getNextMove(board);
                        auto end = std::chrono::high_resolution_clock::now();
                        std::chrono::duration<double, std::milli> elapsed = end - start;
                        if (board.getPlayerTurn() == player::A) {
                            if (!swap) {
                                res.timeA += elapsed.count();
                                res.nodesA += ret.nodesExplored;
                                res.collisionsA += ret.hashCollisions;
                            } else {
                                res.timeB += elapsed.count();
                                res.nodesB += ret.nodesExplored;
                                res.collisionsB += ret.hashCollisions;
                            }
                        } else {
                            if (!swap) {
                                res.timeB += elapsed.count();
                                res.nodesB += ret.nodesExplored;
                                res.collisionsB += ret.hashCollisions;
                            } else {
                                res.timeA += elapsed.count();
                                res.nodesA += ret.nodesExplored;
                                res.collisionsA += ret.hashCollisions;
                            }
                        }
                        board.makeMove(ret.move);
                    } catch (...) {
                        // std::cerr << "Error: Invalid move in simulation." << std::endl;
                        break;
                    }
                }
            }
            return res;
        };

        unsigned int nThreads = std::thread::hardware_concurrency();
        if (nThreads == 0) nThreads = 4;
        std::vector<std::future<SimResult>> futures;

        if (!isHumanA && !isHumanB) {
            int gamesSwap = numGames / 2;
            int gamesNormal = numGames - gamesSwap;

            int gpThread = gamesNormal / nThreads;
            int rem = gamesNormal % nThreads;
            for (unsigned int t = 0; t < nThreads; ++t) {
                int count = gpThread + (t < rem ? 1 : 0);
                if (count > 0) futures.push_back(std::async(std::launch::async, runGames, count, false));
            }

            gpThread = gamesSwap / nThreads;
            rem = gamesSwap % nThreads;
            for (unsigned int t = 0; t < nThreads; ++t) {
                int count = gpThread + (t < rem ? 1 : 0);
                if (count > 0) futures.push_back(std::async(std::launch::async, runGames, count, true));
            }
        } else {
            int gamesPerThread = numGames / nThreads;
            int remainder = numGames % nThreads;

            for (unsigned int t = 0; t < nThreads; ++t) {
                int count = gamesPerThread + (t < remainder ? 1 : 0);
                if (count > 0) futures.push_back(std::async(std::launch::async, runGames, count, false));
            }
        }

        for (auto& f : futures) {
            SimResult res = f.get();
            winsA += res.winsA;
            winsB += res.winsB;
            draws += res.draws;
            totalTimeA += res.timeA;
            totalTimeB += res.timeB;
            totalNodesA += res.nodesA;
            totalNodesB += res.nodesB;
            totalCollisionsA += res.collisionsA;
            totalCollisionsB += res.collisionsB;
        }

        std::cout << "Results after " << numGames << " games:" << std::endl;
        std::cout << "Player A Wins: " << winsA << " (" << (100.0 * winsA / numGames) << "%)" << std::endl;
        std::cout << "Player B Wins: " << winsB << " (" << (100.0 * winsB / numGames) << "%)" << std::endl;
        std::cout << "Draws:         " << draws << " (" << (100.0 * draws / numGames) << "%)" << std::endl;
        std::cout << "Total Time A:  " << totalTimeA << " ms" << std::endl;
        std::cout << "Total Nodes A: " << totalNodesA << std::endl;
        std::cout << "Total Collisions A: " << totalCollisionsA << std::endl;
        std::cout << "Total Time B:  " << totalTimeB << " ms" << std::endl;
        std::cout << "Total Nodes B: " << totalNodesB << std::endl;
        std::cout << "Total Collisions B: " << totalCollisionsB << std::endl;
        return 0;
    }

    connect3dBoard board;
    auto playerA = playerOptions[playerAIdx].factory();
    auto playerB = playerOptions[playerBIdx].factory();
    double totalTimeA = 0;
    double totalTimeB = 0;
    uint64_t totalNodesA = 0;
    uint64_t totalNodesB = 0;
    uint64_t totalCollisionsA = 0;
    uint64_t totalCollisionsB = 0;
    
    std::cout << "\nStarting Game...\n" << std::endl;

    while (true) {
        // Check for win
        player winner = board.checkWin();
        if (winner != player::NONE) {
            std::cout << connect3dBoard::toString(board) << std::endl;
            std::cout << "Game Over! Player " << (char)winner << " wins!" << std::endl;
            break;
        }

        // Check for draw
        if (board.findMoves().empty()) {
            std::cout << connect3dBoard::toString(board) << std::endl;
            std::cout << "Game Over! It's a draw." << std::endl;
            break;
        }

        player currentTurn = board.getPlayerTurn();
        AI_base* currentPlayerPtr = (currentTurn == player::A) ? playerA.get() : playerB.get();
        
        // If not human, print board state so we can see what's happening
        if (!dynamic_cast<HumanPlayer*>(currentPlayerPtr)) {
            std::cout << connect3dBoard::toString(board) << std::endl;
            std::cout << "Player " << (char)currentTurn << " is thinking..." << std::endl;
        }

        AI_base::evalReturn ret;
        auto start = std::chrono::high_resolution_clock::now();

        if (currentTurn == player::A) {
            ret = playerA->getNextMove(board);
        } else {
            ret = playerB->getNextMove(board);
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;

        if (currentTurn == player::A) {
            totalTimeA += elapsed.count();
            totalNodesA += ret.nodesExplored;
            totalCollisionsA += ret.hashCollisions;
        } else {
            totalTimeB += elapsed.count();
            totalNodesB += ret.nodesExplored;
            totalCollisionsB += ret.hashCollisions;
        }

        std::cout << "Player " << (char)currentTurn << " plays move " << (int)ret.move.movenum 
                  << " (" << elapsed.count() << " ms, " << ret.nodesExplored << " nodes, " 
                  << ret.hashCollisions << " collisions, eval: " << ret.score << ")" << std::endl;
        
        try {
            board.makeMove(ret.move);
        } catch (...) {
            std::cerr << "Error: Invalid move detected." << std::endl;
            break;
        }
    }

    std::cout << "Total Time A: " << totalTimeA << " ms" << std::endl;
    std::cout << "Total Nodes A: " << totalNodesA << std::endl;
    std::cout << "Total Collisions A: " << totalCollisionsA << std::endl;
    std::cout << "Total Time B: " << totalTimeB << " ms" << std::endl;
    std::cout << "Total Nodes B: " << totalNodesB << std::endl;
    std::cout << "Total Collisions B: " << totalCollisionsB << std::endl;

    return 0;
}
