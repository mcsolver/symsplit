#include "solver.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <limits.h>
#include <mutex>
#include <numeric>
#include <set>
#include <thread>
#include <vector>

using std::vector;
using std::cout;
using std::endl;

// ── Internal solver context ──────────────────────────────────────────────────
// Holds all mutable state for a single solve() call, replacing the globals
// that were previously scattered across symsplit.cpp.

struct SolverContext {
    const SolverParams& params;
    SolverStats stats;
    std::atomic<bool> abort_due_to_timeout{false};
    vector<int> index_right;
    vector<int> g0_eqn_classes;
    vector<int> g1_eqn_classes;
    std::chrono::time_point<std::chrono::steady_clock> start_time;

    explicit SolverContext(const SolverParams& p) : params(p) {}
};

// ── Public helper functions (also tested directly) ───────────────────────────

int calc_bound(const vector<Bidomain>& domains) {
    int bound = 0;
    for (const Bidomain& bd : domains)
        bound += std::min(bd.left_len, bd.right_len);
    return bound;
}

int find_min_value(const vector<int>& arr, int start_idx, int len) {
    int min_v = INT_MAX;
    for (int i = 0; i < len; i++)
        if (arr[start_idx + i] < min_v)
            min_v = arr[start_idx + i];
    return min_v;
}

int sum(const vector<int>& vec) {
    return std::accumulate(std::begin(vec), std::end(vec), 0);
}

bool have_identical_neighborhoods(const Graph& graph, ui u, ui v) {
    if (graph.degree[u] != graph.degree[v])
        return false;
    if (graph.label[u] != graph.label[v])
        return false;

    const auto& u_neighbors = graph.adjmat[u];
    const auto& v_neighbors = graph.adjmat[v];

    if (u_neighbors[v] != v_neighbors[u])
        return false;

    for (ui i = 0; i < static_cast<ui>(graph.n); ++i) {
        if (i != v && i != u && u_neighbors[i] != v_neighbors[i])
            return false;
    }
    return true;
}

int find_vertices_with_common_neighbors(const Graph& graph, vector<int>& eqn_classes) {
    const ui graph_size = graph.n;
    eqn_classes.assign(graph_size, -1);

    int label = 0;
    int n_syms = 0;

    for (ui i = 0; i < graph_size; ++i) {
        if (eqn_classes[i] != -1) continue;
        int n_syms_in_class = 0;

        for (ui j = i + 1; j < graph_size; ++j) {
            if (eqn_classes[j] != -1) continue;
            if (graph.degree[i] != graph.degree[j] || graph.label[i] != graph.label[j]) continue;
            if (have_identical_neighborhoods(graph, i, j)) {
                eqn_classes[j] = label;
                n_syms++;
                n_syms_in_class++;
            }
        }

        if (n_syms_in_class > 0) {
            eqn_classes[i] = label;
            ++label;
        }
    }
    return n_syms;
}

// ── Internal helpers (static) ────────────────────────────────────────────────

static vector<int> calculate_degrees(const Graph& g) {
    vector<int> degree(g.n, 0);
    for (int v = 0; v < g.n; v++) {
        for (int w = 0; w < g.n; w++) {
            unsigned int mask = 0xFFFFu;
            if (g.adjmat[v][w] & mask)  degree[v]++;
            if (g.adjmat[v][w] & ~mask) degree[v]++;
        }
    }
    return degree;
}

static int select_bidomain(const vector<Bidomain>& domains, const vector<int>& left,
                            int current_matching_size, const SolverParams& params) {
    int min_size        = INT_MAX;
    int min_tie_breaker = INT_MAX;
    int best            = -1;
    for (unsigned int i = 0; i < domains.size(); i++) {
        const Bidomain& bd = domains[i];
        if (params.connected && current_matching_size > 0 && !bd.is_adjacent) continue;
        int len = params.heuristic == min_max
                ? std::max(bd.left_len, bd.right_len)
                : bd.left_len * bd.right_len;
        if (len < min_size) {
            min_size        = len;
            min_tie_breaker = find_min_value(left, bd.l, bd.left_len);
            best            = i;
        } else if (len == min_size) {
            int tie_breaker = find_min_value(left, bd.l, bd.left_len);
            if (tie_breaker < min_tie_breaker) {
                min_tie_breaker = tie_breaker;
                best            = i;
            }
        }
    }
    return best;
}

static int partition(vector<int>& all_vv, int start, int len,
                     const vector<unsigned int>& adjrow) {
    int i = 0;
    for (int j = 0; j < len; j++) {
        if (adjrow[all_vv[start + j]]) {
            std::swap(all_vv[start + i], all_vv[start + j]);
            i++;
        }
    }
    return i;
}

static int partition_right(vector<int>& all_vv, int start, int len,
                            const vector<unsigned int>& adjrow, vector<int>& index_right) {
    int i = 0;
    for (int j = 0; j < len; j++) {
        if (adjrow[all_vv[start + j]]) {
            std::swap(index_right[all_vv[start + i]], index_right[all_vv[start + j]]);
            std::swap(all_vv[start + i], all_vv[start + j]);
            i++;
        }
    }
    return i;
}

static int partition_sparse(vector<int>& all_vv, int start, int len,
                             int degree, const ui* adjlist, vector<int>& index_right) {
    int pos; int j = 0;
    for (int i = 0; i < degree; ++i) {
        pos = index_right[adjlist[i]];
        if (pos >= start && pos < start + len) {
            std::swap(index_right[all_vv[start + j]], index_right[all_vv[pos]]);
            std::swap(all_vv[start + j], all_vv[pos]);
            j++;
        }
    }
    return j;
}

static vector<Bidomain> filter_domains(const vector<Bidomain>& d, vector<int>& left,
                                        vector<int>& right, const Graph& g0, const Graph& g1,
                                        int v, int w, bool& best_match,
                                        vector<int>& index_right) {
    vector<Bidomain> new_d;
    new_d.reserve(d.size());
    unsigned int ccount = 0;

    for (const Bidomain& old_bd : d) {
        int l = old_bd.l;
        int r = old_bd.r;

        int left_len = partition(left, l, old_bd.left_len, g0.adjmat[v]);
        int right_len;
        if (old_bd.right_len > (int)g1.degree[w])
            right_len = partition_sparse(right, r, old_bd.right_len, g1.degree[w], g1.adjlist[w], index_right);
        else
            right_len = partition_right(right, r, old_bd.right_len, g1.adjmat[w], index_right);

        int left_len_noedge  = old_bd.left_len - left_len;
        int right_len_noedge = old_bd.right_len - right_len;

        if ((left_len == 0 && right_len == 0) ||
            (left_len_noedge == 0 && right_len_noedge == 0) ||
            old_bd.left_len == 0)
            ccount++;

        if (left_len_noedge && right_len_noedge)
            new_d.push_back({l + left_len, r + right_len, left_len_noedge, right_len_noedge, old_bd.is_adjacent});
        if (left_len && right_len)
            new_d.push_back({l, r, left_len, right_len, true});
    }

    best_match = (ccount == d.size());
    return new_d;
}

static int index_of_next_smallest(const vector<int>& arr, int start_idx, int len, int w) {
    int idx     = -1;
    int smallest = INT_MAX;
    for (int i = 0; i < len; i++) {
        if (arr[start_idx + i] > w && arr[start_idx + i] < smallest) {
            smallest = arr[start_idx + i];
            idx      = i;
        }
    }
    return idx;
}

static void remove_vtx_from_left_domain(vector<int>& left, Bidomain& bd, int v) {
    int i = 0;
    while (left[bd.l + i] != v) i++;
    std::swap(left[bd.l + i], left[bd.l + bd.left_len - 1]);
    bd.left_len--;
}

static void remove_bidomain(vector<Bidomain>& domains, int idx) {
    domains[idx] = domains[domains.size() - 1];
    domains.pop_back();
}

static bool break_g1_sym(const vector<int>& arr, int start_idx, int len, int w,
                          const vector<int>& g1_eqn_classes) {
    int w_sym_identity = g1_eqn_classes[w];
    for (int i = 0; i < len; i++) {
        if (g1_eqn_classes[arr[start_idx + i]] == w_sym_identity && arr[start_idx + i] < w)
            return true;
    }
    return false;
}

static void solve_with_sym(SolverContext& ctx,
                            const Graph& g0, const Graph& g1,
                            vector<VtxPair>& incumbent,
                            vector<VtxPair>& current,
                            vector<Bidomain>& domains,
                            vector<int>& left, vector<int>& right,
                            unsigned int matching_size_goal, unsigned int level) {
    if (ctx.abort_due_to_timeout) return;

    if (current.size() > incumbent.size()) {
        incumbent                    = current;
        ctx.stats.calls_for_optimal  = ctx.stats.nodes;
        ctx.stats.time_to_best       = std::chrono::steady_clock::now() - ctx.start_time;
    }
    ctx.stats.nodes++;

    unsigned int bound = current.size() + calc_bound(domains);
    if (bound <= incumbent.size() || bound < matching_size_goal) {
        ctx.stats.cut_branches++;
        return;
    }

    int bd_idx = select_bidomain(domains, left, current.size(), ctx.params);
    if (bd_idx == -1) return;

    Bidomain& bd = domains[bd_idx];
    int v = find_min_value(left, bd.l, bd.left_len);
    remove_vtx_from_left_domain(left, domains[bd_idx], v);

    int w = -1, idx = -1;
    bd.right_len--;

    if (ctx.g0_eqn_classes[v] != -1) {
        for (VtxPair& a : current)
            if (ctx.g0_eqn_classes[a.v] == ctx.g0_eqn_classes[v] && w < a.w)
                w = a.w;
    }

    bool best_match = false;
    for (int i = bd.right_len; i >= 0; --i) {
        idx = index_of_next_smallest(right, bd.r, bd.right_len + 1, w);
        if (idx == -1) break;
        w = right[bd.r + idx];
        if (ctx.g1_eqn_classes[w] != -1 &&
            break_g1_sym(right, bd.r, bd.right_len + 1, w, ctx.g1_eqn_classes)) {
            ctx.stats.g1_pruned++;
            continue;
        }

        std::swap(ctx.index_right[w], ctx.index_right[right[bd.r + bd.right_len]]);
        right[bd.r + idx]           = right[bd.r + bd.right_len];
        right[bd.r + bd.right_len]  = w;

        auto new_domains = filter_domains(domains, left, right, g0, g1, v, w, best_match, ctx.index_right);
        current.emplace_back(VtxPair(v, w));
        solve_with_sym(ctx, g0, g1, incumbent, current, new_domains, left, right, matching_size_goal, level + 1);
        current.pop_back();
        if (best_match || bound <= incumbent.size()) return;
    }

    bd.right_len++;
    if (ctx.g0_eqn_classes[v] != -1) {
        for (int i = 0; i < bd.left_len; ++i) {
            if (ctx.g0_eqn_classes[left[bd.l + i]] == ctx.g0_eqn_classes[v]) {
                std::swap(left[bd.l + i], left[bd.l + bd.left_len - 1]);
                --bd.left_len; --i;
            }
        }
    }
    if (bd.left_len == 0) remove_bidomain(domains, bd_idx);
    solve_with_sym(ctx, g0, g1, incumbent, current, domains, left, right, matching_size_goal, level + 1);
}

static vector<VtxPair> mcs_internal(SolverContext& ctx, const Graph& g0, const Graph& g1) {
    vector<int> left, right;
    auto domains = vector<Bidomain>{};

    std::set<unsigned int> left_labels, right_labels;
    for (unsigned int label : g0.label) left_labels.insert(label);
    for (unsigned int label : g1.label) right_labels.insert(label);
    std::set<unsigned int> labels;
    std::set_intersection(std::begin(left_labels), std::end(left_labels),
                          std::begin(right_labels), std::end(right_labels),
                          std::inserter(labels, std::begin(labels)));

    for (unsigned int label : labels) {
        int start_l = left.size();
        int start_r = right.size();
        for (int i = 0; i < g0.n; i++)
            if (g0.label[i] == label) left.push_back(i);
        for (int i = 0; i < g1.n; i++)
            if (g1.label[i] == label) right.push_back(i);
        domains.push_back({start_l, start_r, (int)(left.size() - start_l), (int)(right.size() - start_r), false});
    }

    vector<VtxPair> incumbent;

    int g0_n_syms = find_vertices_with_common_neighbors(g0, ctx.g0_eqn_classes);
    int g1_n_syms = find_vertices_with_common_neighbors(g1, ctx.g1_eqn_classes);
    if (g0_n_syms > 0) ctx.stats.g0_has_syms = true;
    if (g1_n_syms > 0) ctx.stats.g1_has_syms = true;

    if (ctx.params.big_first) {
        for (int k = 0; k < g0.n; k++) {
            unsigned int goal     = g0.n - k;
            auto left_copy        = left;
            auto right_copy       = right;
            auto domains_copy     = domains;
            vector<VtxPair> current;
            solve_with_sym(ctx, g0, g1, incumbent, current, domains_copy, left_copy, right_copy, goal, 1);
            if (incumbent.size() == goal || ctx.abort_due_to_timeout) break;
            if (!ctx.params.quiet) cout << "Upper bound: " << goal - 1 << endl;
        }
    } else {
        vector<VtxPair> current;
        solve_with_sym(ctx, g0, g1, incumbent, current, domains, left, right, 1, 1);
    }

    return incumbent;
}

// ── Public entry point ───────────────────────────────────────────────────────

SolverResult solve(const Graph& g0_in, const Graph& g1_in, const SolverParams& params) {
    // Ensure g0 is the smaller graph (copies, so originals stay intact)
    Graph g0 = (g0_in.n <= g1_in.n) ? g0_in : g1_in;
    Graph g1 = (g0_in.n <= g1_in.n) ? g1_in : g0_in;

    // Sort vertices by degree for better search ordering
    vector<int> g0_deg = calculate_degrees(g0);
    vector<int> g1_deg = calculate_degrees(g1);

    vector<int> vv0(g0.n);
    std::iota(std::begin(vv0), std::end(vv0), 0);
    bool g1_dense = sum(g1_deg) < g1.n * (g1.n - 1);
    std::stable_sort(std::begin(vv0), std::end(vv0), [&](int a, int b) {
        return g1_dense ? (g0_deg[a] > g0_deg[b]) : (g0_deg[a] < g0_deg[b]);
    });

    vector<int> vv1(g1.n);
    std::iota(std::begin(vv1), std::end(vv1), 0);
    bool g0_dense = sum(g0_deg) < g0.n * (g0.n - 1);
    std::stable_sort(std::begin(vv1), std::end(vv1), [&](int a, int b) {
        return g0_dense ? (g1_deg[a] > g1_deg[b]) : (g1_deg[a] < g1_deg[b]);
    });

    Graph g0_sorted = induced_subgraph(g0, vv0);
    Graph g1_sorted = induced_subgraph(g1, vv1);
    set_adjlist(g0_sorted);
    set_adjlist(g1_sorted);

    SolverContext ctx(params);
    ctx.start_time = std::chrono::steady_clock::now();
    for (int i = 0; i < g1_sorted.n; ++i) ctx.index_right.push_back(i);

    // Timeout thread
    std::thread timeout_thread;
    std::mutex timeout_mutex;
    std::condition_variable timeout_cv;

    if (params.timeout != 0) {
        timeout_thread = std::thread([&] {
            auto abort_time = std::chrono::steady_clock::now() + std::chrono::seconds(params.timeout);
            std::unique_lock<std::mutex> guard(timeout_mutex);
            while (!ctx.abort_due_to_timeout.load()) {
                if (std::cv_status::timeout == timeout_cv.wait_until(guard, abort_time)) {
                    ctx.stats.aborted = true;
                    break;
                }
            }
            ctx.abort_due_to_timeout.store(true);
        });
    }

    vector<VtxPair> solution = mcs_internal(ctx, g0_sorted, g1_sorted);

    // Remap sorted indices back to original graph indices
    for (auto& p : solution) {
        p.v = vv0[p.v];
        p.w = vv1[p.w];
    }

    auto total = std::chrono::steady_clock::now() - ctx.start_time;

    // Clean up timeout thread
    if (timeout_thread.joinable()) {
        {
            std::unique_lock<std::mutex> guard(timeout_mutex);
            ctx.abort_due_to_timeout.store(true);
            timeout_cv.notify_all();
        }
        timeout_thread.join();
    }

    ctx.stats.total_time = total;

    SolverResult result;
    result.solution = solution;
    result.stats    = ctx.stats;
    return result;
}
