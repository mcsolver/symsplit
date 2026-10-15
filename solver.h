#pragma once

#include "graph.h"
#include <chrono>
#include <vector>

enum Heuristic { min_max, min_product };

struct VtxPair {
    int v, w;
    VtxPair(int v, int w) : v(v), w(w) {}
};

struct Bidomain {
    int l, r;
    int left_len, right_len;
    bool is_adjacent;
    Bidomain(int l, int r, int left_len, int right_len, bool is_adjacent)
        : l(l), r(r), left_len(left_len), right_len(right_len), is_adjacent(is_adjacent) {}
};

struct SolverParams {
    bool connected  = false;
    bool big_first  = false;
    bool quiet      = false;
    Heuristic heuristic = min_max;
    int timeout     = 0;
};

struct SolverStats {
    unsigned long long nodes             = 0;
    unsigned long long cut_branches      = 0;
    unsigned long long calls_for_optimal = 0;
    std::chrono::duration<double> time_to_best{};
    std::chrono::duration<double> total_time{};
    unsigned int g0_pruned  = 0;
    unsigned int g1_pruned  = 0;
    bool g0_has_syms = false;
    bool g1_has_syms = false;
    bool aborted     = false;
};

struct SolverResult {
    std::vector<VtxPair> solution;
    SolverStats stats;
};

// Testable helper functions
int calc_bound(const std::vector<Bidomain>& domains);
int find_min_value(const std::vector<int>& arr, int start_idx, int len);
int sum(const std::vector<int>& vec);
bool have_identical_neighborhoods(const Graph& graph, ui u, ui v);
int find_vertices_with_common_neighbors(const Graph& graph, std::vector<int>& eqn_classes);

// Main entry point
SolverResult solve(const Graph& g0, const Graph& g1, const SolverParams& params);
