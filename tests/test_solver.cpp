#include <catch2/catch_test_macros.hpp>

#include "../solver.h"
#include "../graph.h"

using std::vector;

// ── calc_bound ───────────────────────────────────────────────────────────────

TEST_CASE("calc_bound sums min(left_len, right_len) across domains", "[calc_bound]") {
    // Each Bidomain: l, r, left_len, right_len, is_adjacent
    vector<Bidomain> domains = {
        {0, 0, 3, 5, false},   // min = 3
        {3, 5, 2, 2, true},    // min = 2
        {5, 7, 4, 1, false},   // min = 1
    };
    REQUIRE(calc_bound(domains) == 6);
}

TEST_CASE("calc_bound returns 0 for empty domains", "[calc_bound]") {
    REQUIRE(calc_bound({}) == 0);
}

TEST_CASE("calc_bound with single domain", "[calc_bound]") {
    vector<Bidomain> domains = {{0, 0, 7, 3, false}};
    REQUIRE(calc_bound(domains) == 3);
}

// ── find_min_value ───────────────────────────────────────────────────────────

TEST_CASE("find_min_value finds minimum in subrange", "[find_min_value]") {
    vector<int> arr = {10, 3, 7, 1, 5};
    REQUIRE(find_min_value(arr, 0, 5) == 1);
    REQUIRE(find_min_value(arr, 0, 3) == 3);  // subrange [10,3,7]
    REQUIRE(find_min_value(arr, 2, 2) == 1);  // subrange [7,1]
}

TEST_CASE("find_min_value with single element", "[find_min_value]") {
    vector<int> arr = {42};
    REQUIRE(find_min_value(arr, 0, 1) == 42);
}

// ── sum ──────────────────────────────────────────────────────────────────────

TEST_CASE("sum accumulates vector", "[sum]") {
    REQUIRE(sum({1, 2, 3, 4}) == 10);
    REQUIRE(sum({}) == 0);
    REQUIRE(sum({-1, 1}) == 0);
}

// ── have_identical_neighborhoods ─────────────────────────────────────────────
//
// Build graphs manually to avoid file I/O in unit tests.
// A triangle (0-1-2): all vertices have degree 2 and identical neighborhoods.
// A path  (0-1-2):   endpoints differ from middle vertex.

static Graph make_triangle() {
    // 3 vertices, edges 0-1, 1-2, 0-2
    Graph g(3);
    g.adjmat[0][1] = g.adjmat[1][0] = 1;
    g.adjmat[1][2] = g.adjmat[2][1] = 1;
    g.adjmat[0][2] = g.adjmat[2][0] = 1;
    set_adjlist(g);
    return g;
}

static Graph make_path3() {
    // 3 vertices, edges 0-1, 1-2
    Graph g(3);
    g.adjmat[0][1] = g.adjmat[1][0] = 1;
    g.adjmat[1][2] = g.adjmat[2][1] = 1;
    set_adjlist(g);
    return g;
}

TEST_CASE("have_identical_neighborhoods: triangle vertices 0 and 2 are symmetric", "[symmetry]") {
    Graph g = make_triangle();
    // In a triangle every pair of vertices has the same neighborhood structure
    REQUIRE(have_identical_neighborhoods(g, 0, 1) == true);
    REQUIRE(have_identical_neighborhoods(g, 0, 2) == true);
    REQUIRE(have_identical_neighborhoods(g, 1, 2) == true);
}

TEST_CASE("have_identical_neighborhoods: path endpoints vs middle differ", "[symmetry]") {
    Graph g = make_path3();
    // 0 and 2 are endpoints (degree 1, symmetric); 1 is the middle (degree 2)
    REQUIRE(have_identical_neighborhoods(g, 0, 2) == true);
    REQUIRE(have_identical_neighborhoods(g, 0, 1) == false);
    REQUIRE(have_identical_neighborhoods(g, 1, 2) == false);
}

// ── find_vertices_with_common_neighbors ──────────────────────────────────────

TEST_CASE("find_vertices_with_common_neighbors: triangle has 3 symmetric vertices", "[symmetry]") {
    Graph g = make_triangle();
    vector<int> classes;
    int n_syms = find_vertices_with_common_neighbors(g, classes);
    REQUIRE(n_syms == 2);           // 2 additional vertices share the class of vertex 0
    REQUIRE(classes.size() == 3);
    // All three should be in the same equivalence class
    REQUIRE(classes[0] == classes[1]);
    REQUIRE(classes[1] == classes[2]);
}

TEST_CASE("find_vertices_with_common_neighbors: path has 2 symmetric endpoints", "[symmetry]") {
    Graph g = make_path3();
    vector<int> classes;
    int n_syms = find_vertices_with_common_neighbors(g, classes);
    REQUIRE(n_syms == 1);           // vertices 0 and 2 are symmetric
    REQUIRE(classes[0] == classes[2]);
    REQUIRE(classes[1] == -1);      // middle vertex has no symmetric partner
}

TEST_CASE("find_vertices_with_common_neighbors: no symmetry in asymmetric graph", "[symmetry]") {
    // Star: center=0, leaves 1,2,3 but leaf 3 has an extra self-label
    Graph g(4);
    g.adjmat[0][1] = g.adjmat[1][0] = 1;
    g.adjmat[0][2] = g.adjmat[2][0] = 1;
    g.adjmat[0][3] = g.adjmat[3][0] = 1;
    g.label[3] = 99;   // different label breaks symmetry with leaves 1 and 2
    set_adjlist(g);

    vector<int> classes;
    int n_syms = find_vertices_with_common_neighbors(g, classes);
    // Leaves 1 and 2 are still symmetric; leaf 3 is not
    REQUIRE(n_syms == 1);
    REQUIRE(classes[1] == classes[2]);
    REQUIRE(classes[3] == -1);
}

// ── solve(): end-to-end with real graph files ─────────────────────────────────

TEST_CASE("solve: DIMACS pair gives MCS of size 10", "[solve][dimacs]") {
    char p[] = "data/tests/dimacs/pattern";
    char t[] = "data/tests/dimacs/target";
    Graph g0 = readGraph(p, 'D', false, false, false);
    Graph g1 = readGraph(t, 'D', false, false, false);

    SolverParams params;
    params.timeout = 30;
    SolverResult result = solve(g0, g1, params);

    REQUIRE(result.solution.size() == 10);
    REQUIRE(result.stats.aborted == false);
}

TEST_CASE("solve: LAD pair gives MCS of size 7", "[solve][lad]") {
    char p[] = "data/tests/general/pattern";
    char t[] = "data/tests/general/target";
    Graph g0 = readGraph(p, 'L', false, false, false);
    Graph g1 = readGraph(t, 'L', false, false, false);

    SolverParams params;
    params.timeout = 30;
    SolverResult result = solve(g0, g1, params);

    REQUIRE(result.solution.size() == 7);
    REQUIRE(result.stats.aborted == false);
}
