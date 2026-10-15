"""Cumulative (cactus) plots comparing SymSplit with every baseline.

Reads results/<solver>/<DATASET>_<heuristic>_<timeout>.txt for each solver in
SOLVERS that has results (see compose.yaml) and writes, per dataset and for all
datasets combined:
  baselines_<metric>_<name>.pdf     instances solved within t seconds, one line per solver
  baselines_<metric>_<name>_<solver>.txt  the same points as pgfplots coordinates

Only instances every plotted solver has results for are counted, and instances
all of them completed in under --min-time seconds are left out as too easy.
A completed run reporting a smaller solution than another solver found is a
wrong answer: it is reported and not counted as solved.

Usage:
  python scripts/plot_baselines.py --timeout 10
  python scripts/plot_baselines.py --timeout 1800 --metric optimum --datasets BI
"""
import argparse
import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

from plot_results import (METRICS, METRIC_LABELS, SIZE, cumulative, read_indicator,
                          read_results, solved, write_coordinates)

# Service name in compose.yaml -> (label, colour, line style). A baseline and
# its +DSB variant share a colour; the DSB variant is dashed.
SOLVERS = {
    "mcsplit": ("McSplit", "C0", "-"),
    "mcsplit-dsb": ("McSplit+DSB", "C0", "--"),
    "mcsplit-rl": ("McSplit+RL", "C1", "-"),
    "mcsplit-rl-dsb": ("McSplit+RL+DSB", "C1", "--"),
    "mcsplit-ll": ("McSplit+LL", "C2", "-"),
    "mcsplit-ll-dsb": ("McSplit+LL+DSB", "C2", "--"),
    "mcsplit-dal": ("McSplit+DAL", "C4", "-"),
    "mcsplit-dal-dsb": ("McSplit+DAL+DSB", "C4", "--"),
    "rrsplit": ("RRSplit", "C5", "-"),
    "symsplit": ("SymSplit", "C3", "-"),
}


def load(dataset, args):
    """Maps each solver with results to its results, in indicator order, on the shared instances."""
    results = {}
    for solver in SOLVERS:
        path = os.path.join(args.results, solver, f"{dataset}_{args.heuristic}_{args.timeout}.txt")
        if os.path.exists(path):
            results[solver] = read_results(path)
    if not results:
        print(f"{dataset}: skipped, no results")
        return {}
    missing = [s for s in SOLVERS if s not in results]
    if missing:
        print(f"{dataset}: no results for {', '.join(missing)}")

    keys = [k for k in read_indicator(os.path.join(args.indicators, f"{dataset}.txt"))
            if all(k in r for r in results.values())]
    print(f"{dataset}: {len(keys)} instances shared by {len(results)} solvers")
    return {s: [r[k] for k in keys] for s, r in results.items()}


def solved_times(name, results, metric, min_time):
    """Maps each solver to the times of the instances it solved correctly."""
    column = METRICS[metric]
    solvers = list(results)
    times = {s: [] for s in solvers}
    skipped = wrong = 0
    for rows in zip(*results.values()):
        if all(float(r[METRICS["completion"]]) < min_time for r in rows):
            skipped += 1
            continue
        best = max(int(r[SIZE]) for r in rows)
        for s, r in zip(solvers, rows):
            if not solved(r):
                continue
            if int(r[SIZE]) < best:
                wrong += 1
                print(f"  wrong answer: {SOLVERS[s][0]} on {r[0]}, {r[1]}: {r[SIZE]} < {best}")
                continue
            times[s].append(float(r[column]))
    counted = len(next(iter(results.values()))) - skipped
    print(f"  {name}: {counted} instances counted, {skipped} left out as solved by all in < {min_time:g} s"
          + (f", {wrong} wrong answers" if wrong else ""))
    return times


def plot(name, results, args):
    if not results or not next(iter(results.values())):
        return
    times = solved_times(name, results, args.metric, args.min_time)
    fig, ax = plt.subplots(figsize=(9, 6))
    for solver, solver_times in times.items():
        label, color, style = SOLVERS[solver]
        points = cumulative(solver_times, args.timeout)
        ax.step([x for x, _ in points], [y for _, y in points], style, where="post", color=color,
                linewidth=2 if solver == "symsplit" else 1.2, label=f"{label} ({len(solver_times)})")
        write_coordinates(os.path.join(args.out, f"baselines_{args.metric}_{name}_{solver}.txt"), points)
        print(f"    {label}: {len(solver_times)} solved")

    ax.set(xlabel="Time (s)", ylabel="Number of instances solved",
           title=f"{name}: instances solved within {METRIC_LABELS[args.metric]} (timeout {args.timeout} s)")
    if args.log_x:
        ax.set_xscale("log")
    ax.legend(fontsize="small")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    base = os.path.join(args.out, f"baselines_{args.metric}_{name}")
    fig.savefig(base + ".pdf")
    fig.savefig(base + ".png", dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--timeout", required=True, help="TIMEOUT the experiments ran with")
    parser.add_argument("--heuristic", default="min_max")
    parser.add_argument("--datasets", nargs="+", default=["BI", "CV", "LV", "SI"])
    parser.add_argument("--metric", choices=METRICS, default="completion",
                        help="completion: time to finish the search; optimum: time to find the best solution")
    parser.add_argument("--min-time", type=float, default=10,
                        help="leave out instances all solvers completed in less time (s)")
    parser.add_argument("--results", default="results")
    parser.add_argument("--indicators", default="indicators")
    parser.add_argument("--out", default="plots")
    parser.add_argument("--log-x", action="store_true", help="log time axis")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)

    combined = None
    for dataset in args.datasets:
        results = load(dataset, args)
        plot(dataset, results, args)
        if not results:
            continue
        # Combine only solvers present for every dataset loaded so far.
        combined = ({s: combined[s] + results[s] for s in combined if s in results}
                    if combined is not None else results)
    if len(args.datasets) > 1 and combined:
        print("All datasets:")
        plot("ALL", combined, args)
    print(f"Plots written to {args.out}/")


if __name__ == "__main__":
    main()
