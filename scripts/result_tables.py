"""Summary tables comparing SymSplit with RRSplit.

Reads results/<solver>/<DATASET>_<heuristic>_<timeout>.txt as written by run.sh
(see compose.yaml) and prints, per dataset and for all datasets combined:

Larger common subgraph, on hard instances (both solvers hit the timeout):
  No. Hard Cases                  hard instances both solvers have results for
  Non-Equal Rate (%)              hard instances where the two sizes differ
  Larger Common Subgraph Rate (%) share of the non-equal instances won by each solver

Speedup, on instances at least one solver completed and at least one ran for
--min-time seconds or more:
  Average / Maximum Speedup       RRSplit time / SymSplit time, over the instances
                                  where SymSplit completed faster

Symmetry pruning, over all SymSplit instances:
  Mean / Stdev                    branches pruned by symmetry as a % of branches
                                  pruned by the bound (instances with bound pruning)
  High sym pruning cases (%)      instances where symmetry pruned more than the bound

Usage:
  python scripts/result_tables.py --timeout 1800
  python scripts/result_tables.py --timeout 10 --datasets BI SI
"""
import argparse
import os
import statistics

# Columns shared by both solvers' output lines.
SIZE = 2
COMPLETION_TIME = 5
ABORTED = -1  # 1 when the solver hit the timeout
# SymSplit-only columns.
CUT_BRANCHES = 8
G0_PRUNED = 9
G1_PRUNED = 10

SOLVERS = ["rrsplit", "symsplit"]


def normalize(path):
    return path.strip().removeprefix("./")


def read_results(file_name):
    """Maps (pattern, target) to the tokens of its result line."""
    results = {}
    with open(file_name) as file:
        for line in file:
            tokens = [token.strip() for token in line.split(",")]
            if len(tokens) < COMPLETION_TIME + 2:
                continue  # blank or truncated line, e.g. a run still writing
            results[(normalize(tokens[0]), normalize(tokens[1]))] = tokens
    return results


def read_indicator(file_name):
    with open(file_name) as file:
        return [tuple(normalize(t) for t in line.split(",")) for line in file if line.strip()]


def load(dataset, args):
    """SymSplit results and (RRSplit, SymSplit) pairs in indicator order, or None if a results file is missing."""
    files = {s: os.path.join(args.results, s, f"{dataset}_{args.heuristic}_{args.timeout}.txt")
             for s in SOLVERS}
    missing = [f for f in files.values() if not os.path.exists(f)]
    if missing:
        print(f"{dataset}: skipped, missing {', '.join(missing)}")
        return None

    keys = read_indicator(os.path.join(args.indicators, f"{dataset}.txt"))
    rr, sym = (read_results(files[s]) for s in SOLVERS)
    return {
        "symsplit": [sym[k] for k in keys if k in sym],
        "pairs": [(rr[k], sym[k]) for k in keys if k in rr and k in sym],
    }


def aborted(r):
    return int(r[ABORTED]) == 1


# Larger common subgraph

def subgraph_counts(pairs):
    """(hard cases, SymSplit larger, RRSplit larger)."""
    hard = sym_larger = rr_larger = 0
    for r, s in pairs:
        if not (aborted(r) and aborted(s)):
            continue
        hard += 1
        if int(s[SIZE]) > int(r[SIZE]):
            sym_larger += 1
        elif int(s[SIZE]) < int(r[SIZE]):
            rr_larger += 1
    return hard, sym_larger, rr_larger


def percent(part, whole):
    return f"{part * 100 / whole:.2f}" if whole else "-"


def subgraph_row(name, pairs):
    hard, sym_larger, rr_larger = subgraph_counts(pairs)
    non_equal = sym_larger + rr_larger
    return [name, str(hard), percent(non_equal, hard),
            percent(sym_larger, non_equal), percent(rr_larger, non_equal)]


def print_subgraph_table(rows, total):
    w = [max(7, *(len(r[0]) for r in rows + [total])), 8, 9, 10, 10]
    group = w[3] + w[4] + 3

    def line(cells):
        return (f"| {cells[0]:<{w[0]}} | {cells[1]:>{w[1]}} | {cells[2]:>{w[2]}} | "
                f"{cells[3]:>{w[3]}}   {cells[4]:>{w[4]}} |")

    rule = "+" + "+".join("-" * (x + 2) for x in w[:3]) + "+" + "-" * (group + 2) + "+"
    print(rule)
    print(f"| {'':{w[0]}} | {'No. Hard':>{w[1]}} | {'Non-Equal':>{w[2]}} | {'Larger Common Subgraph':^{group}} |")
    print(f"| {'Dataset':{w[0]}} | {'Cases':>{w[1]}} | {'Rate (%)':>{w[2]}} | {'Rate (%)':^{group}} |")
    print(f"| {'':{w[0]}} | {'':{w[1]}} | {'':{w[2]}} | {'-' * group} |")
    print(line(["", "", "", "SymSplit", "RRSplit"]))
    print(rule)
    for r in rows:
        print(line(r))
    print(rule)
    print(line(total))
    print(rule)


# Speedup

def speedups(pairs, min_time):
    """RRSplit time / SymSplit time on instances where SymSplit completed faster."""
    result = []
    for r, s in pairs:
        if aborted(r) and aborted(s):
            continue
        rr_time, sym_time = float(r[COMPLETION_TIME]), float(s[COMPLETION_TIME])
        if rr_time < min_time and sym_time < min_time:
            continue
        if sym_time < rr_time:
            result.append(rr_time / sym_time)
    return result


def times(x):
    """Three significant figures, switching to 8.06e3 style from 1000 up."""
    return f"x {x:.2e}".replace("e+0", "e") if x >= 1000 else f"x {x:.3g}"


def speedup_row(name, pairs, min_time):
    values = speedups(pairs, min_time)
    if not values:
        return [name, "-", "-"]
    return [name, times(statistics.mean(values)), times(max(values))]


# Symmetry pruning

def pruning_ratios(symsplit):
    """(symmetry/bound pruning % per instance with bound pruning, high symmetry pruning count)."""
    ratios = []
    high = 0
    for s in symsplit:
        bound = int(s[CUT_BRANCHES])
        symmetry = int(s[G0_PRUNED]) + int(s[G1_PRUNED])
        if bound != 0:
            ratios.append(symmetry * 100 / bound)
        if symmetry > bound:
            high += 1
    return ratios, high


def pruning_row(name, symsplit):
    ratios, high = pruning_ratios(symsplit)
    if not ratios:
        return [name, "-", "-", "-"]
    stdev = statistics.stdev(ratios) if len(ratios) > 1 else 0.0
    return [name, f"{statistics.mean(ratios):.2f}", f"{stdev:.2f}", percent(high, len(ratios))]


def print_table(header, rows, total):
    widths = [max(len(str(r[i])) for r in [header, total] + rows) for i in range(len(header))]

    def line(cells):
        return "| " + " | ".join(c.ljust(w) if i == 0 else c.rjust(w)
                                 for i, (c, w) in enumerate(zip(cells, widths))) + " |"

    rule = "+" + "+".join("-" * (w + 2) for w in widths) + "+"
    print(rule)
    print(line(header))
    print(rule)
    for r in rows:
        print(line(r))
    print(rule)
    print(line(total))
    print(rule)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--timeout", required=True, help="TIMEOUT the experiments ran with")
    parser.add_argument("--heuristic", default="min_max")
    parser.add_argument("--datasets", nargs="+", default=["BI", "CV", "LV", "SI"])
    parser.add_argument("--results", default="results")
    parser.add_argument("--indicators", default="indicators")
    parser.add_argument("--min-time", type=float, default=10,
                        help="speedup ignores instances both solvers completed in less time (s)")
    args = parser.parse_args()

    loaded = {}
    for dataset in args.datasets:
        results = load(dataset, args)
        if results is not None:
            loaded[dataset] = results
    if not loaded:
        return
    all_pairs = [p for r in loaded.values() for p in r["pairs"]]
    all_symsplit = [s for r in loaded.values() for s in r["symsplit"]]

    print("\nLarger common subgraph on hard instances")
    print_subgraph_table([subgraph_row(d, r["pairs"]) for d, r in loaded.items()],
                         subgraph_row("All", all_pairs))

    print(f"\nSpeedup of SymSplit over RRSplit (instances taking >= {args.min_time:g} s)")
    print_table(["Dataset", "Average Speedup", "Maximum Speedup"],
                [speedup_row(d, r["pairs"], args.min_time) for d, r in loaded.items()],
                speedup_row("All", all_pairs, args.min_time))

    print("\nSymmetry pruning as % of bound pruning (SymSplit)")
    print_table(["Dataset", "Mean", "Stdev", "High sym pruning cases (%)"],
                [pruning_row(d, r["symsplit"]) for d, r in loaded.items()],
                pruning_row("All", all_symsplit))


if __name__ == "__main__":
    main()
