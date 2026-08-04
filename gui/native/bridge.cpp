#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "3d-connect4-board.hpp"
#include "ai.hpp"
#include "heuristic_bot.hpp"
#include "minimax_ai_b1_v1.hpp"
#include "minimax_ai_b1_v2.hpp"
#include "minimax_ai_b2_v1.hpp"
#include "minimax_ai_b2_v2.hpp"
#include "minimax_ai_b3_v1.hpp"
#include "minimax_ai_b3_v2.hpp"
#include "minimax_ai_b4_v1.hpp"
#include "minimax_ai_b5_v1.hpp"
#include "minimax_ai_b5_v2.hpp"

namespace {

volatile std::sig_atomic_t stopRequested = 0;
void onSignal(int) { stopRequested = 1; }

std::string quote(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
                else out << c;
        }
    }
    out << '"';
    return out.str();
}

std::vector<int> splitInts(const std::string& text) {
    std::vector<int> values;
    if (text.empty()) return values;
    std::stringstream input(text);
    std::string item;
    while (std::getline(input, item, ',')) {
        if (item.empty()) throw std::runtime_error("empty value in integer list");
        size_t used = 0;
        int value = std::stoi(item, &used);
        if (used != item.size()) throw std::runtime_error("invalid integer list");
        values.push_back(value);
    }
    return values;
}

std::vector<std::string> splitStrings(const std::string& text) {
    std::vector<std::string> values;
    if (text.empty()) return values;
    std::stringstream input(text);
    std::string item;
    while (std::getline(input, item, ',')) if (!item.empty()) values.push_back(item);
    return values;
}

std::map<std::string, int> splitDepthOverrides(const std::string& text) {
    std::map<std::string, int> values;
    for (const auto& item : splitStrings(text)) {
        size_t separator = item.find('=');
        if (separator == std::string::npos || separator == 0 || separator + 1 == item.size()) throw std::runtime_error("invalid depth override");
        size_t used = 0;
        int depth = std::stoi(item.substr(separator + 1), &used);
        if (used != item.size() - separator - 1 || depth < 1 || depth > 12) throw std::runtime_error("depth must be between 1 and 12");
        values[item.substr(0, separator)] = depth;
    }
    return values;
}

std::map<std::string, std::string> parseOptions(int argc, char** argv, int start) {
    std::map<std::string, std::string> result;
    for (int i = start; i < argc; ++i) {
        std::string key = argv[i];
        if (!key.starts_with("--")) throw std::runtime_error("expected --option, got " + key);
        key = key.substr(2);
        if (i + 1 < argc && !std::string(argv[i + 1]).starts_with("--")) result[key] = argv[++i];
        else result[key] = "true";
    }
    return result;
}

int getInt(const std::map<std::string, std::string>& options, const std::string& key, int fallback) {
    auto it = options.find(key);
    if (it == options.end()) return fallback;
    size_t used = 0;
    int value = std::stoi(it->second, &used);
    if (used != it->second.size()) throw std::runtime_error("invalid integer for --" + key);
    return value;
}

std::string getString(const std::map<std::string, std::string>& options, const std::string& key,
                      const std::string& fallback = "") {
    auto it = options.find(key);
    return it == options.end() ? fallback : it->second;
}

class SeededRandomAI final : public AI_base {
    std::mt19937_64 generator;
public:
    explicit SeededRandomAI(uint64_t seed) : generator(seed) {}
    evalReturn getNextMove(connect3dBoard board) override {
        auto moves = board.findMoves();
        if (moves.empty()) return {0, connect3dMove()};
        std::uniform_int_distribution<size_t> pick(0, moves.size() - 1);
        return {0, moves[pick(generator)], 1, 0};
    }
};

struct AiDefinition {
    const char* id;
    const char* name;
    const char* family;
    int depth;
    std::function<std::unique_ptr<AI_base>(uint64_t, int)> factory;
};

const std::vector<AiDefinition>& aiDefinitions() {
    static const std::vector<AiDefinition> definitions = {
        {"random", "Random AI", "random", 0, [](uint64_t s, int) { return std::make_unique<SeededRandomAI>(s); }},
        {"heuristic", "Heuristic Bot", "heuristic", 1, [](uint64_t, int) { return std::make_unique<HeuristicBot>(); }},
        {"b1-v1", "Minimax AI b1 v1", "minimax", 4, [](uint64_t, int d) { return std::make_unique<MinimaxAI_b1_v1>(d); }},
        {"b1-v2", "Minimax AI b1 v2", "minimax", 7, [](uint64_t, int d) { return std::make_unique<MinimaxAI_b1_v2>(d); }},
        {"b2-v1", "Minimax AI b2 v1", "minimax", 6, [](uint64_t, int d) { return std::make_unique<MinimaxAI_b2_v1>(d); }},
        {"b2-v2", "Minimax AI b2 v2", "minimax", 6, [](uint64_t, int d) { return std::make_unique<MinimaxAI_b2_v2>(d); }},
        {"b3-v1", "Minimax AI b3 v1", "minimax", 9, [](uint64_t, int d) { return std::make_unique<MinimaxAI_b3_v1>(d); }},
        {"b3-v2", "Minimax AI b3 v2", "minimax", 7, [](uint64_t, int d) { return std::make_unique<MinimaxAI_b3_v2>(d); }},
        {"b4-v1", "Minimax AI b4 v1", "minimax", 9, [](uint64_t, int d) { return std::make_unique<MinimaxAI_b4_v1>(d); }},
        {"b5-v1", "Minimax AI b5 v1", "minimax", 9, [](uint64_t, int d) { return std::make_unique<MinimaxAI_b5_v1>(d); }},
        {"b5-v2", "Minimax AI b5 v2", "minimax", 7, [](uint64_t, int d) { return std::make_unique<MinimaxAI_b5_v2>(d); }}
    };
    return definitions;
}

const AiDefinition& findAi(const std::string& id) {
    auto& definitions = aiDefinitions();
    auto it = std::find_if(definitions.begin(), definitions.end(), [&](const auto& item) { return item.id == id; });
    if (it == definitions.end()) throw std::runtime_error("unknown AI id: " + id);
    return *it;
}

std::unique_ptr<AI_base> makeAi(const std::string& id, uint64_t seed, int depthOverride = 0) {
    const auto& definition = findAi(id);
    if (depthOverride < 0 || depthOverride > 12) throw std::runtime_error("depth must be between 1 and 12");
    if (depthOverride > 0 && std::string(definition.family) != "minimax") throw std::runtime_error("depth override is only supported for minimax AIs");
    return definition.factory(seed, depthOverride > 0 ? depthOverride : definition.depth);
}

std::vector<std::array<int, 4>> winningLines() {
    std::vector<std::array<int, 4>> lines;
    auto add = [&](int a, int b, int c, int d) { lines.push_back({a, b, c, d}); };
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) add(r * 4 + c, r * 4 + c + 16, r * 4 + c + 32, r * 4 + c + 48);
    for (int d = 0; d < 4; ++d) for (int r = 0; r < 4; ++r) { int i = d * 16 + r * 4; add(i, i + 1, i + 2, i + 3); }
    for (int d = 0; d < 4; ++d) for (int c = 0; c < 4; ++c) { int i = d * 16 + c; add(i, i + 4, i + 8, i + 12); }
    for (int d = 0; d < 4; ++d) { int i = d * 16; add(i, i + 5, i + 10, i + 15); add(i + 3, i + 6, i + 9, i + 12); }
    for (int r = 0; r < 4; ++r) { int i = r * 4; add(i, i + 17, i + 34, i + 51); add(i + 3, i + 18, i + 33, i + 48); }
    for (int c = 0; c < 4; ++c) { int i = c; add(i, i + 20, i + 40, i + 60); add(i + 12, i + 24, i + 36, i + 48); }
    add(0, 21, 42, 63); add(3, 22, 41, 60); add(12, 25, 38, 51); add(15, 26, 37, 48);
    return lines;
}

std::vector<int> winningCells(const connect3dBoard& board) {
    player winner = board.checkWin();
    if (winner == player::NONE) return {};
    for (const auto& line : winningLines()) {
        if (std::all_of(line.begin(), line.end(), [&](int cell) { return board.board[cell] == winner; }))
            return {line.begin(), line.end()};
    }
    return {};
}

struct BuiltPosition { connect3dBoard board; std::vector<int> moves; };

BuiltPosition buildPosition(const std::vector<int>& moves) {
    BuiltPosition result;
    for (size_t ply = 0; ply < moves.size(); ++ply) {
        if (result.board.checkWin() != player::NONE || result.board.findMoves().empty())
            throw std::runtime_error("move history continues after the game ended at ply " + std::to_string(ply));
        int move = moves[ply];
        if (move < 0 || move >= 16) throw std::runtime_error("move must be between 0 and 15");
        auto legal = result.board.findMoves();
        if (std::none_of(legal.begin(), legal.end(), [&](const auto& item) { return item.movenum == move; }))
            throw std::runtime_error("column " + std::to_string(move) + " is full at ply " + std::to_string(ply));
        result.board.makeMove(connect3dMove(move));
        result.moves.push_back(move);
    }
    return result;
}

void writeIntArray(std::ostream& out, const std::vector<int>& values) {
    out << '[';
    for (size_t i = 0; i < values.size(); ++i) { if (i) out << ','; out << values[i]; }
    out << ']';
}

void writePosition(std::ostream& out, const BuiltPosition& position) {
    const auto& board = position.board;
    auto boardForMoves = board;
    auto legal = boardForMoves.findMoves();
    player winner = board.checkWin();
    out << "{\"schemaVersion\":1,\"moves\":";
    writeIntArray(out, position.moves);
    out << ",\"cells\":[";
    for (int i = 0; i < 64; ++i) {
        if (i) out << ',';
        if (board.board[i] == player::NONE) out << "null";
        else out << quote(std::string(1, char(board.board[i])));
    }
    out << "],\"turn\":" << quote(std::string(1, char(board.playerTurn))) << ",\"legalMoves\":[";
    for (size_t i = 0; i < legal.size(); ++i) { if (i) out << ','; out << int(legal[i].movenum); }
    out << "],\"winner\":";
    if (winner == player::NONE) out << "null"; else out << quote(std::string(1, char(winner)));
    out << ",\"draw\":" << ((winner == player::NONE && legal.empty()) ? "true" : "false") << ",\"winningCells\":";
    writeIntArray(out, winningCells(board));
    out << '}';
}

struct SearchData {
    int move = -1;
    std::optional<double> score;
    double elapsedMs = 0;
    uint64_t nodes = 0;
    uint64_t collisions = 0;
    std::optional<int> reply;
    std::string outcome;
};

SearchData search(AI_base& ai, const connect3dBoard& board) {
    auto started = std::chrono::steady_clock::now();
    auto result = ai.getNextMove(board);
    auto ended = std::chrono::steady_clock::now();
    return {result.move.isValid() ? int(result.move.movenum) : -1, result.score,
            std::chrono::duration<double, std::milli>(ended - started).count(),
            result.nodesExplored, result.hashCollisions, std::nullopt, ""};
}

void writeSearch(std::ostream& out, const SearchData& searchData) {
    out << "{\"move\":" << searchData.move << ",\"score\":";
    if (searchData.score) out << std::setprecision(12) << *searchData.score; else out << "null";
    out << ",\"elapsedMs\":" << std::setprecision(8) << searchData.elapsedMs
        << ",\"nodes\":" << searchData.nodes << ",\"hashCollisions\":" << searchData.collisions;
    if (searchData.reply) out << ",\"reply\":" << *searchData.reply;
    if (!searchData.outcome.empty()) out << ",\"outcome\":" << quote(searchData.outcome);
    out << '}';
}

std::mutex emitMutex;
void emitLine(const std::function<void(std::ostream&)>& writer) {
    std::lock_guard lock(emitMutex);
    writer(std::cout);
    std::cout << '\n' << std::flush;
}

int inspectCommand(const std::map<std::string, std::string>& options) {
    auto position = buildPosition(splitInts(getString(options, "moves")));
    emitLine([&](std::ostream& out) { out << "{\"type\":\"complete\",\"result\":"; writePosition(out, position); out << '}'; });
    return 0;
}

int aisCommand() {
    emitLine([](std::ostream& out) {
        out << "{\"type\":\"complete\",\"result\":[";
        auto& defs = aiDefinitions();
        for (size_t i = 0; i < defs.size(); ++i) {
            if (i) out << ',';
            out << "{\"id\":" << quote(defs[i].id) << ",\"name\":" << quote(defs[i].name)
                << ",\"family\":" << quote(defs[i].family) << ",\"depth\":";
            if (defs[i].depth) out << defs[i].depth; else out << "null";
            out << '}';
        }
        out << "]}";
    });
    return 0;
}

int analyzeCommand(const std::map<std::string, std::string>& options) {
    auto position = buildPosition(splitInts(getString(options, "moves")));
    if (position.board.checkWin() != player::NONE || position.board.findMoves().empty())
        throw std::runtime_error("cannot analyze a terminal position");
    const std::string aiId = getString(options, "ai");
    const auto& definition = findAi(aiId);
    uint64_t seed = uint64_t(getInt(options, "seed", 1));
    int depth = getInt(options, "depth", 0);
    bool candidates = getString(options, "candidates", "false") == "true";
    auto ai = makeAi(aiId, seed, depth);
    SearchData best = search(*ai, position.board);
    std::vector<SearchData> reports;
    if (candidates) {
        auto evaluator = makeAi(aiId, seed ^ 0x9e3779b97f4a7c15ULL, depth);
        for (auto move : position.board.findMoves()) {
            connect3dBoard copy = position.board;
            copy.makeMove(move);
            SearchData report;
            report.move = move.movenum;
            player winner = copy.checkWin();
            if (winner != player::NONE) {
                report.outcome = winner == player::A ? "A_WIN" : "B_WIN";
            } else if (copy.findMoves().empty()) {
                report.outcome = "DRAW";
            } else if (std::string(definition.family) == "random") {
                auto reply = search(*evaluator, copy);
                report.reply = reply.move;
                report.elapsedMs = reply.elapsedMs;
                report.nodes = reply.nodes;
            } else if (std::string(definition.family) == "heuristic") {
                auto started = std::chrono::steady_clock::now();
                b2_v1::connect3dBoardFast fast(copy);
                report.score = fast.heuristic();
                auto reply = evaluator->getNextMove(copy);
                auto ended = std::chrono::steady_clock::now();
                report.reply = reply.move.isValid() ? std::optional<int>(reply.move.movenum) : std::nullopt;
                report.elapsedMs = std::chrono::duration<double, std::milli>(ended - started).count();
                report.nodes = 1;
            } else {
                auto reply = search(*evaluator, copy);
                report.score = reply.score;
                report.reply = reply.move;
                report.elapsedMs = reply.elapsedMs;
                report.nodes = reply.nodes;
                report.collisions = reply.collisions;
            }
            reports.push_back(report);
            emitLine([&](std::ostream& out) { out << "{\"type\":\"candidate\",\"report\":"; writeSearch(out, report); out << '}'; });
            if (stopRequested) break;
        }
    }
    emitLine([&](std::ostream& out) {
        out << "{\"type\":\"complete\",\"result\":{\"position\":";
        writePosition(out, position);
        out << ",\"best\":"; writeSearch(out, best);
        out << ",\"candidates\":[";
        for (size_t i = 0; i < reports.size(); ++i) { if (i) out << ','; writeSearch(out, reports[i]); }
        out << "]}}";
    });
    return 0;
}

struct MoveData : SearchData { int ply = 0; player who = player::NONE; int cell = -1; };
struct Totals { double ms = 0; uint64_t nodes = 0; uint64_t collisions = 0; };
struct GameData {
    std::string id;
    int repeat = 0;
    std::string pairing;
    std::string playerA;
    std::string playerB;
    int playerADepth = 0;
    int playerBDepth = 0;
    player winner = player::NONE;
    bool draw = false;
    std::vector<MoveData> moves;
    Totals totalA;
    Totals totalB;
};

void writeMove(std::ostream& out, const MoveData& move) {
    out << "{\"ply\":" << move.ply << ",\"player\":" << quote(std::string(1, char(move.who)))
        << ",\"cell\":" << move.cell << ',';
    std::ostringstream rest; writeSearch(rest, move);
    std::string fields = rest.str();
    out << fields.substr(1);
}

void writeTotals(std::ostream& out, const Totals& totals) {
    out << "{\"elapsedMs\":" << totals.ms << ",\"nodes\":" << totals.nodes
        << ",\"hashCollisions\":" << totals.collisions << '}';
}

void writeGame(std::ostream& out, const GameData& game) {
    out << "{\"id\":" << quote(game.id) << ",\"repeat\":" << game.repeat
        << ",\"pairing\":" << quote(game.pairing) << ",\"playerA\":" << quote(game.playerA)
        << ",\"playerB\":" << quote(game.playerB)
        << ",\"playerADepth\":" << game.playerADepth << ",\"playerBDepth\":" << game.playerBDepth << ",\"winner\":";
    if (game.winner == player::NONE) out << "null"; else out << quote(std::string(1, char(game.winner)));
    out << ",\"draw\":" << (game.draw ? "true" : "false") << ",\"moves\":[";
    for (size_t i = 0; i < game.moves.size(); ++i) { if (i) out << ','; writeMove(out, game.moves[i]); }
    out << "],\"totals\":{\"A\":"; writeTotals(out, game.totalA); out << ",\"B\":"; writeTotals(out, game.totalB); out << "}}";
}

GameData playGame(const std::string& id, const std::string& pairing, int repeat,
                  const std::string& playerA, const std::string& playerB, int depthA, int depthB, uint64_t seed, bool live) {
    GameData game;
    game.id = id;
    game.repeat = repeat;
    game.pairing = pairing;
    game.playerA = playerA;
    game.playerB = playerB;
    game.playerADepth = depthA > 0 ? depthA : findAi(playerA).depth;
    game.playerBDepth = depthB > 0 ? depthB : findAi(playerB).depth;
    connect3dBoard board;
    auto aiA = makeAi(playerA, seed ^ 0xa0761d6478bd642fULL, depthA);
    auto aiB = makeAi(playerB, seed ^ 0xe7037ed1a0b428dbULL, depthB);
    while (!stopRequested) {
        game.winner = board.checkWin();
        auto legal = board.findMoves();
        if (game.winner != player::NONE || legal.empty()) { game.draw = game.winner == player::NONE; break; }
        player who = board.getPlayerTurn();
        auto result = search(who == player::A ? *aiA : *aiB, board);
        if (result.move < 0 || std::none_of(legal.begin(), legal.end(), [&](auto m) { return m.movenum == result.move; }))
            throw std::runtime_error("AI returned an invalid move");
        int cell = result.move;
        while (cell < 64 && board.board[cell] != player::NONE) cell += 16;
        MoveData move;
        static_cast<SearchData&>(move) = result;
        move.ply = int(game.moves.size()) + 1;
        move.who = who;
        move.cell = cell;
        game.moves.push_back(move);
        Totals& totals = who == player::A ? game.totalA : game.totalB;
        totals.ms += result.elapsedMs; totals.nodes += result.nodes; totals.collisions += result.collisions;
        board.makeMove(connect3dMove(result.move));
        if (live) emitLine([&](std::ostream& out) { out << "{\"type\":\"move\",\"gameId\":" << quote(id) << ",\"move\":"; writeMove(out, move); out << '}'; });
    }
    game.winner = board.checkWin();
    game.draw = game.winner == player::NONE && board.findMoves().empty();
    return game;
}

struct Task { size_t index; int repeat; std::string pairing; std::string a; std::string b; int depthA; int depthB; uint64_t seed; };
struct ColorStandingData {
    int played = 0, wins = 0, draws = 0, losses = 0, points = 0;
    Totals totals;
};

struct StandingData {
    int played = 0, wins = 0, draws = 0, losses = 0, points = 0;
    Totals totals;
    ColorStandingData asA;
    ColorStandingData asB;
    std::vector<double> repeatWinRates;
};

struct MatchupData {
    int played = 0, secondWins = 0, draws = 0, secondLosses = 0;
};

void writeColorStanding(std::ostream& out, const ColorStandingData& color) {
    out << "{\"played\":" << color.played << ",\"wins\":" << color.wins
        << ",\"draws\":" << color.draws << ",\"losses\":" << color.losses
        << ",\"points\":" << color.points << ",\"elapsedMs\":" << color.totals.ms
        << ",\"nodes\":" << color.totals.nodes << ",\"hashCollisions\":" << color.totals.collisions << '}';
}

std::vector<size_t> chooseSamples(const std::vector<GameData>& games, int limit, uint64_t seed) {
    std::vector<size_t> picked;
    std::unordered_set<size_t> seen;
    auto takeOutcome = [&](player winner, bool draw) {
        for (size_t i = 0; i < games.size(); ++i) if (!seen.contains(i) && games[i].winner == winner && games[i].draw == draw) {
            picked.push_back(i); seen.insert(i); return;
        }
    };
    takeOutcome(player::A, false); takeOutcome(player::B, false); takeOutcome(player::NONE, true);
    std::vector<size_t> remaining;
    for (size_t i = 0; i < games.size(); ++i) if (!seen.contains(i)) remaining.push_back(i);
    std::mt19937_64 random(seed);
    std::shuffle(remaining.begin(), remaining.end(), random);
    for (size_t i : remaining) if (int(picked.size()) < limit) picked.push_back(i);
    if (int(picked.size()) > limit) picked.resize(limit);
    return picked;
}

int runCommand(const std::map<std::string, std::string>& options) {
    const std::string kind = getString(options, "kind", "match");
    const int gamesPerPair = getInt(options, "games", kind == "match" ? 1 : 10);
    const int repeats = getInt(options, "repeats", 1);
    const int sampleLimit = getInt(options, "samples", 5);
    const int requestedThreads = getInt(options, "threads", 2);
    const uint64_t baseSeed = uint64_t(getInt(options, "seed", 1));
    const bool includeSelfPlay = getInt(options, "self-play", 0) != 0;
    const auto depthOverrides = splitDepthOverrides(getString(options, "depths"));
    if (gamesPerPair < 1 || repeats < 1 || sampleLimit < 0 || requestedThreads < 1) throw std::runtime_error("numeric run options must be positive");
    if (kind != "match" && gamesPerPair % 2 != 0) throw std::runtime_error("batch and tournament game counts must be even");

    std::vector<Task> tasks;
    std::vector<std::string> entrants;
    if (kind == "tournament") {
        entrants = splitStrings(getString(options, "ais"));
        if (entrants.empty() || (!includeSelfPlay && entrants.size() < 2)) throw std::runtime_error(includeSelfPlay ? "self-play tournament needs at least one AI" : "tournament needs at least two AIs");
        for (const auto& id : entrants) findAi(id);
        for (int repeat = 0; repeat < repeats; ++repeat) for (size_t i = 0; i < entrants.size(); ++i) for (size_t j = includeSelfPlay ? i : i + 1; j < entrants.size(); ++j) {
            std::string pairing = entrants[i] + "__" + entrants[j] + "__r" + std::to_string(repeat);
            for (int game = 0; game < gamesPerPair; ++game) {
                bool swap = game >= gamesPerPair / 2;
                tasks.push_back({tasks.size(), repeat, pairing, swap ? entrants[j] : entrants[i], swap ? entrants[i] : entrants[j],
                    swap ? depthOverrides.contains(entrants[j]) ? depthOverrides.at(entrants[j]) : 0 : depthOverrides.contains(entrants[i]) ? depthOverrides.at(entrants[i]) : 0,
                    swap ? depthOverrides.contains(entrants[i]) ? depthOverrides.at(entrants[i]) : 0 : depthOverrides.contains(entrants[j]) ? depthOverrides.at(entrants[j]) : 0,
                    baseSeed + tasks.size() * 0x9e3779b9ULL + uint64_t(repeat)});
            }
        }
    } else {
        std::string a = getString(options, "a"), b = getString(options, "b");
        int depthA = getInt(options, "depth-a", 0), depthB = getInt(options, "depth-b", 0);
        findAi(a); findAi(b); entrants = {a, b};
        int count = kind == "match" ? 1 : gamesPerPair;
        for (int game = 0; game < count; ++game) {
            bool swap = kind != "match" && game >= count / 2;
            tasks.push_back({tasks.size(), 0, a + "__" + b + "__r0", swap ? b : a, swap ? a : b,
                swap ? depthB : depthA, swap ? depthA : depthB, baseSeed + tasks.size() * 0x9e3779b9ULL});
        }
    }

    emitLine([&](std::ostream& out) { out << "{\"type\":\"started\",\"totalGames\":" << tasks.size() << '}'; });
    std::vector<GameData> results(tasks.size());
    std::atomic<size_t> next{0};
    std::atomic<int> completed{0};
    std::exception_ptr workerError;
    std::mutex errorMutex;
    int threadCount = std::min<int>(requestedThreads, tasks.size());
    std::vector<std::thread> workers;
    for (int thread = 0; thread < threadCount; ++thread) workers.emplace_back([&] {
        while (!stopRequested) {
            size_t index = next.fetch_add(1);
            if (index >= tasks.size()) break;
            try {
                const auto& task = tasks[index];
                results[index] = playGame("game-" + std::to_string(index + 1), task.pairing, task.repeat,
                                          task.a, task.b, task.depthA, task.depthB, task.seed, kind == "match");
                int done = ++completed;
                emitLine([&](std::ostream& out) { out << "{\"type\":\"progress\",\"completed\":" << done << ",\"total\":" << tasks.size() << '}'; });
            } catch (...) {
                std::lock_guard lock(errorMutex);
                if (!workerError) workerError = std::current_exception();
                stopRequested = 1;
            }
        }
    });
    for (auto& worker : workers) worker.join();
    if (workerError) std::rethrow_exception(workerError);
    results.resize(completed.load());

    std::map<std::string, StandingData> standings;
    std::map<std::string, std::map<std::string, MatchupData>> matchups;
    std::map<std::string, std::map<std::string, int>> headToHead;
    std::map<int, std::map<std::string, std::pair<int, int>>> repeatWins;
    for (const auto& id : entrants) standings[id];
    for (const auto& game : results) {
        auto& sa = standings[game.playerA]; auto& sb = standings[game.playerB];
        auto& matchup = matchups[game.playerB][game.playerA];
        matchup.played++;
        sa.played++; sb.played++; sa.asA.played++; sb.asB.played++;
        sa.totals.ms += game.totalA.ms; sa.totals.nodes += game.totalA.nodes; sa.totals.collisions += game.totalA.collisions;
        sb.totals.ms += game.totalB.ms; sb.totals.nodes += game.totalB.nodes; sb.totals.collisions += game.totalB.collisions;
        sa.asA.totals.ms += game.totalA.ms; sa.asA.totals.nodes += game.totalA.nodes; sa.asA.totals.collisions += game.totalA.collisions;
        sb.asB.totals.ms += game.totalB.ms; sb.asB.totals.nodes += game.totalB.nodes; sb.asB.totals.collisions += game.totalB.collisions;
        repeatWins[game.repeat][game.playerA].second++; repeatWins[game.repeat][game.playerB].second++;
        if (game.draw) {
            matchup.draws++;
            sa.draws++; sb.draws++; sa.points++; sb.points++; sa.asA.draws++; sb.asB.draws++; sa.asA.points++; sb.asB.points++;
            headToHead[game.playerA][game.playerB]++; headToHead[game.playerB][game.playerA]++;
        } else if (game.winner == player::A) {
            matchup.secondLosses++;
            sa.wins++; sb.losses++; sa.points += 3; sa.asA.wins++; sb.asB.losses++; sa.asA.points += 3;
            headToHead[game.playerA][game.playerB] += 3; repeatWins[game.repeat][game.playerA].first++;
        } else {
            matchup.secondWins++;
            sb.wins++; sa.losses++; sb.points += 3; sb.asB.wins++; sa.asA.losses++; sb.asB.points += 3;
            headToHead[game.playerB][game.playerA] += 3; repeatWins[game.repeat][game.playerB].first++;
        }
    }
    for (auto& [id, standing] : standings) for (auto& [repeat, values] : repeatWins) {
        auto [wins, played] = values[id];
        if (played) standing.repeatWinRates.push_back(double(wins) / played);
    }
    std::vector<std::string> ranked;
    for (auto& [id, _] : standings) ranked.push_back(id);
    std::sort(ranked.begin(), ranked.end(), [&](const auto& left, const auto& right) {
        auto& a = standings[left]; auto& b = standings[right];
        if (a.points != b.points) return a.points > b.points;
        if (headToHead[left][right] != headToHead[right][left]) return headToHead[left][right] > headToHead[right][left];
        int diffA = a.wins - a.losses, diffB = b.wins - b.losses;
        return diffA != diffB ? diffA > diffB : left < right;
    });

    std::map<std::string, std::vector<GameData>> groups;
    if (kind == "match") groups["match"] = results;
    else for (auto& game : results) groups[game.pairing].push_back(game);
    std::vector<GameData> samples;
    for (auto& [key, group] : groups) for (size_t index : chooseSamples(group, kind == "match" ? 1 : sampleLimit,
                                                                        baseSeed ^ std::hash<std::string>{}(key))) samples.push_back(group[index]);

    emitLine([&](std::ostream& out) {
        out << "{\"type\":\"complete\",\"result\":{\"kind\":" << quote(kind)
            << ",\"completedGames\":" << completed << ",\"totalGames\":" << tasks.size() << ",\"standings\":[";
        for (size_t i = 0; i < ranked.size(); ++i) {
            if (i) out << ',';
            auto& id = ranked[i]; auto& s = standings[id];
            double mean = 0, deviation = 0;
            if (!s.repeatWinRates.empty()) {
                for (double value : s.repeatWinRates) mean += value;
                mean /= s.repeatWinRates.size();
                for (double value : s.repeatWinRates) deviation += (value - mean) * (value - mean);
                deviation = std::sqrt(deviation / s.repeatWinRates.size());
            }
            out << "{\"aiId\":" << quote(id) << ",\"played\":" << s.played << ",\"wins\":" << s.wins
                << ",\"draws\":" << s.draws << ",\"losses\":" << s.losses << ",\"points\":" << s.points
                << ",\"elapsedMs\":" << s.totals.ms << ",\"nodes\":" << s.totals.nodes
                << ",\"hashCollisions\":" << s.totals.collisions << ",\"meanWinRate\":" << mean
                << ",\"stdDevWinRate\":" << deviation << ",\"byColor\":{\"A\":";
            writeColorStanding(out, s.asA);
            out << ",\"B\":";
            writeColorStanding(out, s.asB);
            out << "}}";
        }
        out << "],\"matchups\":[";
        bool firstMatchup = true;
        for (const auto& [playerB, opponents] : matchups) for (const auto& [playerA, matchup] : opponents) {
            if (!firstMatchup) out << ',';
            firstMatchup = false;
            out << "{\"playerA\":" << quote(playerA) << ",\"playerB\":" << quote(playerB)
                << ",\"played\":" << matchup.played << ",\"secondWins\":" << matchup.secondWins
                << ",\"draws\":" << matchup.draws << ",\"secondLosses\":" << matchup.secondLosses << '}';
        }
        out << "],\"sampledGames\":[";
        for (size_t i = 0; i < samples.size(); ++i) { if (i) out << ','; writeGame(out, samples[i]); }
        out << "]}}";
    });
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::signal(SIGTERM, onSignal);
    std::signal(SIGINT, onSignal);
    try {
        if (argc < 2) throw std::runtime_error("usage: connect4-gui-engine <ais|inspect|analyze|run>");
        std::string command = argv[1];
        auto options = parseOptions(argc, argv, 2);
        if (command == "ais") return aisCommand();
        if (command == "inspect") return inspectCommand(options);
        if (command == "analyze") return analyzeCommand(options);
        if (command == "run") return runCommand(options);
        throw std::runtime_error("unknown command: " + command);
    } catch (const std::exception& error) {
        emitLine([&](std::ostream& out) { out << "{\"type\":\"error\",\"message\":" << quote(error.what()) << '}'; });
        return 1;
    } catch (...) {
        emitLine([](std::ostream& out) { out << "{\"type\":\"error\",\"message\":\"unknown native error\"}"; });
        return 1;
    }
}

