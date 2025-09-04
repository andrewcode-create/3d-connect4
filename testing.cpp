#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdlib> // For rand()
#include <ctime>   // For time()

// Assuming connect3dMove struct looks something like this
struct connect3dMove {
    int modHeuristic;
    // other move data...
};

/**
 * @brief Sorts an array of up to 16 connect3dMove objects using Insertion Sort.
 *
 * This function is optimized for sorting a small number of elements (where N <= 16).
 * Insertion sort is highly efficient for small datasets because it has low overhead
 * and performs well on data that might already be partially sorted.
 * It iterates through the elements, and for each element, it finds the correct
 * position in the already-sorted part of the array and inserts it there.
 *
 * For small N, this approach can outperform the more complex std::sort, which is
 * designed for larger datasets.
 *
 * @param moves A pointer to the beginning of the array/vector of moves.
 * @param count The actual number of elements in the array to sort.
 */
void insertionSort(connect3dMove* moves, int count) {
    if (count < 2) {
        return; // Already sorted if 0 or 1 elements
    }

    for (int i = 1; i < count; ++i) {
        connect3dMove key = moves[i];
        int j = i - 1;

        // Move elements of moves[0..i-1], that are less than key's heuristic,
        // to one position ahead of their current position.
        // This is modified for a descending sort.
        while (j >= 0 && moves[j].modHeuristic < key.modHeuristic) {
            moves[j + 1] = moves[j];
            j = j - 1;
        }
        moves[j + 1] = key;
    }
}


// Example usage and benchmark
int main() {
    srand(time(0)); // Seed random number generator

    // The actual number of moves to sort (can be anything up to 16)
    const int inx = 12;

    // Create a vector with a capacity of 16, but we'll only use 'inx' elements
    std::array<connect3dMove, 16> moves(16);
    for (int i = 0; i < inx; ++i) {
        moves[i] = {rand() % 100}; // Random heuristic values
    }

    std::array<connect3dMove, 16> moves_copy = moves;

    // --- Time std::sort ---
    auto start_std = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 64*64*64*64; i++) {
        std::sort(moves.begin(), moves.begin() + inx, [](const connect3dMove& a, const connect3dMove& b) {
            return a.modHeuristic > b.modHeuristic;
        });
    }
    auto end_std = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::nano> elapsed_std = end_std - start_std;


    // --- Time insertionSort ---
    auto start_fast = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 64*64*64*64; i++) {
        insertionSort(moves_copy.data(), inx);
    }
    auto end_fast = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::nano> elapsed_fast = end_fast - start_fast;


    // --- Print results ---
    std::cout << "Sorting " << inx << " elements:\n";
    std::cout << "std::sort took:     " << elapsed_std.count() << " ns\n";
    std::cout << "insertionSort took: " << elapsed_fast.count() << " ns\n\n";

    std::cout << "Sorted array using insertionSort:\n";
    for (int i = 0; i < inx; ++i) {
        std::cout << moves_copy[i].modHeuristic << " ";
    }
    std::cout << std::endl;

    return 0;
}
