#pragma once

#include <climits>

#include <vector>

using ui = unsigned int;
constexpr int BITS_PER_UNSIGNED_INT = CHAR_BIT * sizeof(unsigned int);

struct Graph {
    int n;
    std::vector<std::vector<unsigned int>> adjmat;
    std::vector<unsigned int> label;
    unsigned int  *degree  = nullptr;
    unsigned int **adjlist = nullptr;
    Graph(unsigned int n);
};

Graph induced_subgraph(Graph& g, std::vector<int> vv);

Graph read_graph(char* filename, char format, bool directed, bool edge_labelled, bool vertex_labelled);

void set_adjlist(Graph& g);