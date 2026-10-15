#include "graph.h"

#include <cstdio>
#include <cstdlib>

#include <iostream>
#include <string>

static void fail(std::string msg) {
    std::cerr << msg << std::endl;
    exit(1);
}

Graph::Graph(unsigned int n) {
    this->n = n;
    label  = std::vector<unsigned int>(n, 0u);
    adjmat = std::vector<std::vector<unsigned int>>(n, std::vector<unsigned int>(n, 0u));
}

Graph induced_subgraph(Graph& g, std::vector<int> vv) {
    Graph subg(vv.size());
    for (int i = 0; i < subg.n; i++)
        for (int j = 0; j < subg.n; j++)
            subg.adjmat[i][j] = g.adjmat[vv[i]][vv[j]];

    for (int i = 0; i < subg.n; i++)
        subg.label[i] = g.label[vv[i]];
    return subg;
}

static void add_edge(Graph& g, int v, int w, bool directed = false, unsigned int val = 1) {
    if (v != w) {
        if (directed) {
            g.adjmat[v][w] |= val;
            g.adjmat[w][v] |= (val << 16);
        } else {
            g.adjmat[v][w] = val;
            g.adjmat[w][v] = val;
        }
    } else {
        // Set the MSB of the vertex label to indicate a self-loop.
        g.label[v] |= (1u << (BITS_PER_UNSIGNED_INT - 1));
    }
}

static Graph read_dimacs_graph(char* filename, bool directed, bool vertex_labelled) {
    Graph g(0);

    FILE* f = fopen(filename, "r");
    if (f == NULL)
        fail("Cannot open file");

    char*  line  = NULL;
    size_t nchar = 0;

    int nvertices  = 0;
    int medges     = 0;
    int edges_read = 0;
    int v, w, label;

    while (getline(&line, &nchar, f) != -1) {
        if (nchar > 0) {
            switch (line[0]) {
            case 'p':
                if (sscanf(line, "p edge %d %d", &nvertices, &medges) != 2)
                    fail("Error reading a line beginning with p.\n");
                g = Graph(nvertices);
                break;
            case 'e':
                if (sscanf(line, "e %d %d", &v, &w) != 2)
                    fail("Error reading a line beginning with e.\n");
                add_edge(g, v - 1, w - 1, directed);
                edges_read++;
                break;
            case 'n':
                if (sscanf(line, "n %d %d", &v, &label) != 2)
                    fail("Error reading a line beginning with n.\n");
                if (vertex_labelled)
                    g.label[v - 1] |= label;
                break;
            }
        }
    }

    if (medges > 0 && edges_read != medges)
        fail("Unexpected number of edges.");

    fclose(f);
    return g;
}

static Graph read_lad_graph(char* filename, bool directed) {
    Graph g(0);

    FILE* f = fopen(filename, "r");
    if (f == NULL)
        fail("Cannot open file");

    int nvertices = 0;
    if (fscanf(f, "%d", &nvertices) != 1)
        fail("Number of vertices not read correctly.\n");
    g = Graph(nvertices);

    for (int i = 0; i < nvertices; i++) {
        int edge_count;
        if (fscanf(f, "%d", &edge_count) != 1)
            fail("Number of edges not read correctly.\n");
        for (int j = 0; j < edge_count; j++) {
            int w;
            if (fscanf(f, "%d", &w) != 1)
                fail("An edge was not read correctly.\n");
            add_edge(g, i, w, directed);
        }
    }

    fclose(f);
    return g;
}

static int read_word(FILE* fp) {
    unsigned char a[2];
    if (fread(a, 1, 2, fp) != 2)
        fail("Error reading file.\n");
    return (int)a[0] | (((int)a[1]) << 8);
}

static Graph read_binary_graph(char* filename, bool directed, bool edge_labelled, bool vertex_labelled) {
    Graph g(0);

    FILE* f = fopen(filename, "rb");
    if (f == NULL)
        fail("Cannot open file");

    int nvertices = read_word(f);
    g = Graph(nvertices);

    // Labelling scheme: see
    // https://github.com/ciaranm/cp2016-max-common-connected-subgraph-paper/blob/master/code/solve_max_common_subgraph.cc
    int m  = g.n * 33 / 100;
    int p  = 1;
    int k1 = 0;
    int k2 = 0;
    while (p < m && k1 < 16) {
        p *= 2;
        k1 = k2;
        k2++;
    }

    for (int i = 0; i < nvertices; i++) {
        int label = (read_word(f) >> (16 - k1));
        if (vertex_labelled)
            g.label[i] |= label;
    }

    for (int i = 0; i < nvertices; i++) {
        int len = read_word(f);
        for (int j = 0; j < len; j++) {
            int target = read_word(f);
            int label  = (read_word(f) >> (16 - k1)) + 1;
            add_edge(g, i, target, directed, edge_labelled ? label : 1);
        }
    }

    fclose(f);
    return g;
}

Graph read_graph(char* filename, char format, bool directed, bool edge_labelled, bool vertex_labelled) {
    if (format == 'D') return read_dimacs_graph(filename, directed, vertex_labelled);
    if (format == 'L') return read_lad_graph(filename, directed);
    if (format == 'B') return read_binary_graph(filename, directed, edge_labelled, vertex_labelled);
    fail("Unknown graph format\n");
    return Graph(0);
}

void set_adjlist(Graph& g) {
    g.degree  = new unsigned int[g.n];
    g.adjlist = new unsigned int*[g.n];

    for (int i = 0; i < g.n; i++) {
        unsigned int count = 0;
        for (int j = 0; j < g.n; j++)
            if (g.adjmat[i][j] == 1) count++;

        g.adjlist[i] = new unsigned int[count];
        count = 0;

        for (int j = 0; j < g.n; j++) {
            if (g.adjmat[i][j] == 1) {
                g.adjlist[i][count] = j;
                count++;
            }
        }
        g.degree[i] = count;
    }
}
