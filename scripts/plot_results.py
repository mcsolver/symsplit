"""Scatter, cumulative (cactus) and size difference plots comparing RRSplit and SymSplit results.

Reads results/<solver>/<DATASET>_<heuristic>_<timeout>.txt as written by run.sh
(see compose.yaml) and writes, per dataset and for all datasets combined:
  scatter_<metric>_<name>.pdf   RRSplit vs SymSplit time per instance (log-log)
  cactus_<name>.pdf             instances solved within t seconds
  size_delta_<name>.pdf         SymSplit size - RRSplit size on hard instances where they differ
  *.txt                         the same points as pgfplots coordinates

Usage:
  python scripts/plot_results.py --timeout 10
  python scripts/plot_results.py --timeout 1800 --datasets BI SI --out plots/1800
"""
import argparse
import os
from bisect import bisect_right
from collections import Counter

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator

# Columns shared by both solvers' output lines.
SIZE = 2
TIME_TO_OPTIMUM = 4
COMPLETION_TIME = 5
ABORTED = -1  # 1 when the solver hit the timeout

METRICS = {"optimum": TIME_TO_OPTIMUM, "completion": COMPLETION_TIME}
METRIC_LABELS = {"optimum": "time to optimum (s)", "completion": "completion time (s)"}
SOLVERS = {"rrsplit": "RRSplit", "symsplit": "SymSplit"}


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


def paired_instances(dataset, args):
    """Results of both solvers for instances both have finished, in indicator order."""
    files = {s: os.path.join(args.results, s, f"{dataset}_{args.heuristic}_{args.timeout}.txt")
             for s in SOLVERS}
    missing = [f for f in files.values() if not os.path.exists(f)]
    if missing:
        print(f"{dataset}: skipped, missing {', '.join(missing)}")
        return []

    rr, sym = (read_results(files[s]) for s in SOLVERS)
    pairs = []
    for key in read_indicator(os.path.join(args.indicators, f"{dataset}.txt")):
        if key in rr and key in sym:
            pairs.append((rr[key], sym[key]))
    print(f"{dataset}: {len(pairs)} instances in both "
          f"(RRSplit {len(rr)}, SymSplit {len(sym)})")
    return pairs


def solved(r):
    return int(r[ABORTED]) == 0


def check_sizes(name, pairs):
    """Both solvers must agree on the optimum when both completed."""
    mismatches = [(r, s) for r, s in pairs if solved(r) and solved(s) and r[SIZE] != s[SIZE]]
    for r, s in mismatches[:10]:
        print(f"  size mismatch: {r[0]}, {r[1]}: RRSplit {r[SIZE]}, SymSplit {s[SIZE]}")
    if mismatches:
        print(f"  {name}: {len(mismatches)} instances with different solution sizes")


def time_of(r, column, timeout):
    """Timed-out runs count as taking the full timeout."""
    return float(r[column]) if solved(r) else float(timeout)


def write_coordinates(path, points):
    with open(path, "w") as f:
        f.write(" ".join(f"({x:.4g}, {y:.4g})" for x, y in points) + "\n")


def plot_scatter(name, pairs, metric, args):
    column = METRICS[metric]
    # Instances neither solver finished carry no information.
    points = [(time_of(r, column, args.timeout), time_of(s, column, args.timeout))
              for r, s in pairs if solved(r) or solved(s)]
    if not points:
        return
    floor = 1e-4  # log axes cannot show 0 s
    points = [(max(x, floor), max(y, floor)) for x, y in points]

    faster = sum(1 for x, y in points if round(y, 1) < round(x, 1))
    slower = sum(1 for x, y in points if round(y, 1) > round(x, 1))
    equal = len(points) - faster - slower
    print(f"  {name} {metric}: SymSplit faster {100 * faster / len(points):.2f}%, "
          f"equal {100 * equal / len(points):.2f}%, slower {100 * slower / len(points):.2f}% "
          f"of {len(points)} (rounded to 0.1 s)")

    fig, ax = plt.subplots(figsize=(5, 5))
    ax.scatter([x for x, _ in points], [y for _, y in points], s=10, alpha=0.5)
    lo, hi = floor, float(args.timeout) * 1.5
    ax.plot([lo, hi], [lo, hi], "r--", linewidth=1, label="x = y")
    ax.set(xscale="log", yscale="log", xlim=(lo, hi), ylim=(lo, hi),
           xlabel=f"RRSplit {METRIC_LABELS[metric]}",
           ylabel=f"SymSplit {METRIC_LABELS[metric]}",
           title=f"{name}: {METRIC_LABELS[metric]}")
    ax.legend()
    ax.grid(True, which="major", alpha=0.3)
    fig.tight_layout()
    base = os.path.join(args.out, f"scatter_{metric}_{name}")
    fig.savefig(base + ".pdf")
    fig.savefig(base + ".png", dpi=150)
    plt.close(fig)
    write_coordinates(base + ".txt", points)


def cumulative(times, timeout):
    """Step points (t, instances solved within t), extended to the timeout."""
    times = sorted(times)
    points = [(t, bisect_right(times, t)) for t in sorted(set(times))]
    # Completion times can overshoot the timeout slightly; never step backwards.
    points.append((max([float(timeout)] + times), len(times)))
    return points


def plot_cactus(name, pairs, args):
    fig, ax = plt.subplots(figsize=(8, 5))
    for i, solver in enumerate(SOLVERS):
        results = [pair[i] for pair in pairs if solved(pair[i])]
        for metric, style in (("optimum", "--"), ("completion", "-")):
            points = cumulative([float(r[METRICS[metric]]) for r in results], args.timeout)
            label = f"{SOLVERS[solver]} ({'find optimum' if metric == 'optimum' else 'completion'})"
            ax.step([x for x, _ in points], [y for _, y in points], style, where="post",
                    color=f"C{i}", label=label)
            write_coordinates(os.path.join(args.out, f"cactus_{name}_{solver}_{metric}.txt"), points)
        print(f"  {name}: {SOLVERS[solver]} solved {len(results)} of {len(pairs)}")

    ax.set(xlabel="Time (s)", ylabel="Number of instances solved",
           title=f"{name}: instances solved within time (timeout {args.timeout} s)")
    if args.log_x:
        ax.set_xscale("log")
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    base = os.path.join(args.out, f"cactus_{name}")
    fig.savefig(base + ".pdf")
    fig.savefig(base + ".png", dpi=150)
    plt.close(fig)


def plot_size_delta(name, pairs, args):
    """Frequency of SymSplit size - RRSplit size on hard instances (both timed out) where they differ."""
    deltas = [int(s[SIZE]) - int(r[SIZE]) for r, s in pairs
              if not solved(r) and not solved(s) and r[SIZE] != s[SIZE]]
    if not deltas:
        return
    counts = sorted(Counter(deltas).items())
    larger = sum(1 for d in deltas if d > 0)
    print(f"  {name}: SymSplit larger on {larger} of {len(deltas)} hard instances with different sizes "
          f"(delta mean {sum(deltas) / len(deltas):.2f}, min {min(deltas)}, max {max(deltas)})")

    fig, ax = plt.subplots(figsize=(8, 5))
    for color, label, keep in (("C0", "SymSplit larger", lambda d: d > 0),
                               ("C1", "RRSplit larger", lambda d: d < 0)):
        bars = [(d, c) for d, c in counts if keep(d)]
        if bars:
            ax.bar([d for d, _ in bars], [c for _, c in bars], color=color, edgecolor="black", label=label)
    ax.axvline(0, color="grey", linewidth=1)
    ax.set(xlabel="SymSplit size - RRSplit size (vertices)", ylabel="Number of instances",
           title=f"{name}: common subgraph size difference on hard instances")
    ax.xaxis.set_major_locator(MaxNLocator(integer=True))
    ax.legend()
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    base = os.path.join(args.out, f"size_delta_{name}")
    fig.savefig(base + ".pdf")
    fig.savefig(base + ".png", dpi=150)
    plt.close(fig)
    write_coordinates(base + ".txt", counts)


def plot_all(name, pairs, args):
    if not pairs:
        return
    check_sizes(name, pairs)
    for metric in METRICS:
        plot_scatter(name, pairs, metric, args)
    plot_cactus(name, pairs, args)
    plot_size_delta(name, pairs, args)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--timeout", required=True, help="TIMEOUT the experiments ran with")
    parser.add_argument("--heuristic", default="min_max")
    parser.add_argument("--datasets", nargs="+", default=["BI", "CV", "LV", "SI"])
    parser.add_argument("--results", default="results")
    parser.add_argument("--indicators", default="indicators")
    parser.add_argument("--out", default="plots")
    parser.add_argument("--log-x", action="store_true", help="log time axis on cactus plots")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)

    combined = []
    for dataset in args.datasets:
        pairs = paired_instances(dataset, args)
        plot_all(dataset, pairs, args)
        combined += pairs
    if len(args.datasets) > 1:
        print("All datasets:")
        plot_all("ALL", combined, args)
    print(f"Plots written to {args.out}/")


if __name__ == "__main__":
    main()
