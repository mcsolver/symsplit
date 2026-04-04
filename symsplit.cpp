#include "graph.h"
#include "solver.h"

#include <iostream>
#include <string>

#include <argp.h>
#include <stdlib.h>
#include <string.h>

using std::cout;
using std::endl;

static void fail(std::string msg) {
    std::cerr << msg << std::endl;
    exit(1);
}

static char doc[] = "Find a maximum common subgraph of two graphs\vHEURISTIC can be min_max or min_product";
static char args_doc[] = "HEURISTIC FILENAME1 FILENAME2";
static struct argp_option options[] = {
    {"quiet",               'q', 0,         0, "Quiet output"},
    {"verbose",             'v', 0,         0, "Verbose output"},
    {"dimacs",              'd', 0,         0, "Read DIMACS format"},
    {"lad",                 'l', 0,         0, "Read LAD format"},
    {"connected",           'c', 0,         0, "Solve max common CONNECTED subgraph problem"},
    {"directed",            'i', 0,         0, "Use directed graphs"},
    {"labelled",            'a', 0,         0, "Use edge and vertex labels"},
    {"vertex-labelled-only",'x', 0,         0, "Use vertex labels, but not edge labels"},
    {"big-first",           'b', 0,         0, "First try to find an induced subgraph isomorphism, then decrement the target size"},
    {"timeout",             't', "timeout", 0, "Specify a timeout (seconds)"},
    { 0 }
};

static struct {
    bool quiet;
    bool verbose;
    bool dimacs;
    bool lad;
    bool connected;
    bool directed;
    bool edge_labelled;
    bool vertex_labelled;
    bool big_first;
    Heuristic heuristic;
    char *filename1;
    char *filename2;
    int timeout;
    int arg_num;
} arguments;

static void set_default_arguments() {
    arguments.quiet          = false;
    arguments.verbose        = false;
    arguments.dimacs         = false;
    arguments.lad            = false;
    arguments.connected      = false;
    arguments.directed       = false;
    arguments.edge_labelled  = false;
    arguments.vertex_labelled = false;
    arguments.big_first      = false;
    arguments.filename1      = NULL;
    arguments.filename2      = NULL;
    arguments.timeout        = 0;
    arguments.arg_num        = 0;
}

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    switch (key) {
        case 'd':
            if (arguments.lad)
                fail("The -d and -l options cannot be used together.\n");
            arguments.dimacs = true;
            break;
        case 'l':
            if (arguments.dimacs)
                fail("The -d and -l options cannot be used together.\n");
            arguments.lad = true;
            break;
        case 'q': arguments.quiet   = true; break;
        case 'v': arguments.verbose = true; break;
        case 'c':
            if (arguments.directed)
                fail("The connected and directed options can't be used together.");
            arguments.connected = true;
            break;
        case 'i':
            if (arguments.connected)
                fail("The connected and directed options can't be used together.");
            arguments.directed = true;
            break;
        case 'a':
            if (arguments.vertex_labelled)
                fail("The -a and -x options can't be used together.");
            arguments.edge_labelled  = true;
            arguments.vertex_labelled = true;
            break;
        case 'x':
            if (arguments.edge_labelled)
                fail("The -a and -x options can't be used together.");
            arguments.vertex_labelled = true;
            break;
        case 'b': arguments.big_first = true; break;
        case 't': arguments.timeout = std::stoi(arg); break;
        case ARGP_KEY_ARG:
            if (arguments.arg_num == 0) {
                if (std::string(arg) == "min_max")
                    arguments.heuristic = min_max;
                else if (std::string(arg) == "min_product")
                    arguments.heuristic = min_product;
                else
                    fail("Unknown heuristic (try min_max or min_product)");
            } else if (arguments.arg_num == 1) {
                arguments.filename1 = arg;
            } else if (arguments.arg_num == 2) {
                arguments.filename2 = arg;
            } else {
                argp_usage(state);
            }
            arguments.arg_num++;
            break;
        case ARGP_KEY_END:
            if (arguments.arg_num == 0)
                argp_usage(state);
            break;
        default: return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char** argv) {
    set_default_arguments();
    argp_parse(&argp, argc, argv, 0, 0, 0);

    char format = arguments.dimacs ? 'D' : arguments.lad ? 'L' : 'B';
    Graph g0 = readGraph(arguments.filename1, format, arguments.directed,
                         arguments.edge_labelled, arguments.vertex_labelled);
    Graph g1 = readGraph(arguments.filename2, format, arguments.directed,
                         arguments.edge_labelled, arguments.vertex_labelled);

    SolverParams params;
    params.connected  = arguments.connected;
    params.big_first  = arguments.big_first;
    params.quiet      = arguments.quiet;
    params.heuristic  = arguments.heuristic;
    params.timeout    = arguments.timeout;

    SolverResult result = solve(g0, g1, params);

    cout << result.solution.size() << ", " << 1 << ", "
         << result.stats.time_to_best.count() << ", "
         << result.stats.total_time.count() << ", "
         << result.stats.nodes << ", "
         << result.stats.calls_for_optimal << ", "
         << result.stats.cut_branches << ", "
         << result.stats.g0_pruned << ", "
         << result.stats.g1_pruned << ", "
         << result.stats.aborted << endl;

    for (size_t i = 0; i < result.solution.size(); i++) {
        if (i > 0) cout << ", ";
        cout << result.solution[i].v << " " << result.solution[i].w;
    }
    cout << endl;

    return 0;
}