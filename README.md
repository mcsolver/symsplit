# SymSplit
New symmetry aware algorithm for maximum common subgraph problem.

- [Getting the code](#getting-the-code)
- [Reproducing the experiments](#reproducing-the-experiments) — Docker, no local build needed
- [Plotting the results](#plotting-the-results) — Python
- [Building and running locally](#building-and-running-locally) — C++
- [Python API](docs/python-api.md) and [baselines](docs/baselines.md)

# Getting the code
The code, experiment setup and scripts are at <https://github.com/mcsolver/symsplit>. Clone it with
[git](https://git-scm.com/downloads) and run every command in this README from the repository root:

```shell
git clone https://github.com/mcsolver/symsplit.git
cd symsplit
```

Without git, download <https://github.com/mcsolver/symsplit/archive/refs/heads/main.zip>, unzip it and `cd`
into the `symsplit-main` folder.

The benchmark graphs are not in the repository. The Docker build downloads them (see below); to run without
Docker, download them as described in [Running](#running).

# Reproducing the experiments
The experiments compare SymSplit with the RRSplit baseline on four benchmark families:

| Dataset | Graphs | Instances |
|---|---|---|
| `BI` | biochemical reactions | 9,316 |
| `CV` | images-CVIU11 | 6,278 |
| `LV` | LV | 5,782 |
| `SI` | si | 1,170 |

Each instance is a `pattern, target` pair listed in `indicators/<DATASET>.txt`. The graphs are too large
for git and are downloaded from [mcs-data](https://github.com/BuddhiWathsala/mcs-data) during the build.

## Prerequisites
- [Docker](https://docs.docker.com/get-docker/) with Docker Compose v2 (`docker compose version`).
  Docker Desktop on macOS or Windows includes both.
- About 2 GB of free disk space and an internet connection for the first build.
- On Docker Desktop, give Docker enough CPUs (Settings → Resources) for the number of parallel runs you choose.

## How it works
[`compose.yaml`](compose.yaml) defines one service per solver, all built from the same [`Dockerfile`](Dockerfile).
Building an image:
1. clones the solver's repository at the commit set by `REPO_REF` and adds this project's `run.sh` and
   `indicators/`,
2. downloads `data.zip`, checks its SHA-256 and unpacks it into `data/`,
3. compiles the solver and runs a quick test.

Running a container writes `configs/run.env` from the settings below and runs [`run.sh`](run.sh), which solves
every instance of `BI`, `CV`, `LV` and `SI` and appends one line per instance to the results directory.

## Run
```shell
git clone https://github.com/mcsolver/symsplit.git
cd symsplit
docker compose build
docker compose run --rm symsplit     # results in results/symsplit/
docker compose run --rm rrsplit      # results in results/rrsplit/
```

Run the solvers one after the other. `docker compose up` starts them all at once, and they then compete for
CPU, which skews the timing comparison.

The other baselines run the same way, each writing to `results/<service>/`:

```shell
docker compose run --rm mcsplit      # results in results/mcsplit/
```

| Service | Repository | Branch |
|---|---|---|
| `mcsplit` | [BuddhiWathsala/mcsplit-v1](https://github.com/BuddhiWathsala/mcsplit-v1) | `main` |
| `mcsplit-dsb` | [BuddhiWathsala/mcsplit-dsb](https://github.com/BuddhiWathsala/mcsplit-dsb) | `main` |
| `mcsplit-rl` | [BuddhiWathsala/mcsplit-rl-pvt](https://github.com/BuddhiWathsala/mcsplit-rl-pvt) | `main` |
| `mcsplit-rl-dsb` | [BuddhiWathsala/mcsplit-rl-pvt](https://github.com/BuddhiWathsala/mcsplit-rl-pvt) | `new-bound-for-symsplit` |
| `mcsplit-ll` | [BuddhiWathsala/mcsplit-ll-pvt](https://github.com/BuddhiWathsala/mcsplit-ll-pvt) | `main-for-symsplit` |
| `mcsplit-ll-dsb` | [BuddhiWathsala/mcsplit-ll-pvt](https://github.com/BuddhiWathsala/mcsplit-ll-pvt) | `new-bound-for-symsplit` |
| `mcsplit-dal` | [BuddhiWathsala/mcsplit-dal](https://github.com/BuddhiWathsala/mcsplit-dal) | `main-for-symsplit` |
| `mcsplit-dal-dsb` | [BuddhiWathsala/mcsplit-dal](https://github.com/BuddhiWathsala/mcsplit-dal) | `new-bound-for-sym` |

Every image runs this project's [`run.sh`](run.sh) on this project's `indicators/`, whatever the solver's
repository contains, so all solvers are measured on the same instances in the same way.

## Settings
Set these as environment variables when running:

| Variable | Default | Meaning |
|---|---|---|
| `TIMEOUT` | `10` | Time limit per instance, in seconds |
| `N_PARALLEL` | `4` | Instances solved at the same time. Keep it at or below the number of physical CPU cores. |
| `MCSP_HEURISTIC` | `min_max` | Vertex selection heuristic: `min_max` or `min_product` |

```shell
TIMEOUT=1800 N_PARALLEL=8 docker compose run --rm symsplit
TIMEOUT=1800 N_PARALLEL=8 docker compose run --rm rrsplit
```

Use the same settings for every solver so that their results are comparable.

## Results
Each run writes `results/<solver>/<DATASET>_<MCSP_HEURISTIC>_<TIMEOUT>.txt`, for example
`results/symsplit/BI_min_max_10.txt`. Each line is one instance:

```
data/biochemicalReactions/001.txt, data/biochemicalReactions/003.txt, 10, 1, 7.5459e-05, 7.5959e-05, 11, 10, 1, 0, 0, 0
```

| Column | Meaning |
|---|---|
| 1, 2 | Pattern and target graph |
| 3 | Size of the maximum common subgraph found |
| 4 | Solution check (`1` = valid) |
| 5 | Time to find the best solution (s) |
| 6 | Total time (s) |
| 7 | Search nodes |
| Last | `1` if the run hit `TIMEOUT`, otherwise `0` |

    cout << result.solution.size() << ", " << 1 << ", "
         << result.stats.time_to_best.count() << ", "
         << result.stats.total_time.count() << ", "
         << result.stats.nodes << ", "
         << result.stats.calls_for_optimal << ", "
         << result.stats.cut_branches << ", "
         << result.stats.g0_pruned << ", "
         << result.stats.g1_pruned << ", "
         << result.stats.aborted << endl;

Columns between 7 and the last are solver-specific statistics. Lines are appended as instances finish, so
their order differs from the indicator files.

**Resuming:** `run.sh` skips as many instances as its results file already has lines, so rerunning the same
command after an interruption continues where it stopped. To start over, delete the results file.
Changing `TIMEOUT` or `MCSP_HEURISTIC` writes to a new file.

## Rebuilding
Docker reuses the cached clone as long as `REPO_REF` in `compose.yaml` does not change. Pin `REPO_REF` to a
commit hash so that results are tied to exact code, and change the hash to pick up new commits.
To force a complete rebuild:

```shell
docker compose build --no-cache --pull           # all images
docker compose build --no-cache --pull symsplit  # one image
```

## Without Compose
```shell
docker build -t mcs-symsplit --build-arg REPO_REF=<commit> .
docker run --rm -v "$PWD/results/symsplit:/app/results" \
  -e TIMEOUT=10 -e N_PARALLEL=4 -e MCSP_HEURISTIC=min_max mcs-symsplit
```

The other build arguments are `REPO_URL` (the solver repository), `DATA_URL` and `DATA_SHA256` (the data
archive) and `SMOKE_TEST` (the check run after compiling).

# Plotting the results
[`scripts/plot_results.py`](scripts/plot_results.py) compares the two solvers on the instances that both have
finished, so it also works while experiments are still running. It needs Python 3.9+ and matplotlib.

```shell
conda create -n mcs-plots python=3.12 matplotlib -y   # once
conda activate mcs-plots
python scripts/plot_results.py --timeout 10
```

Without conda: `python3 -m venv .venv && source .venv/bin/activate && pip install matplotlib`.

Run it from the repository root. `--timeout` must match the `TIMEOUT` of the experiments, since it selects the
result files. Other options:

| Option | Default | Meaning |
|---|---|---|
| `--datasets` | `BI CV LV SI` | Datasets to plot |
| `--heuristic` | `min_max` | Must match `MCSP_HEURISTIC` |
| `--results` | `results` | Directory holding `rrsplit/` and `symsplit/` |
| `--out` | `plots` | Output directory |
| `--log-x` | off | Log time axis on cumulative plots |

For each dataset, and for all datasets combined (`ALL`), it writes to `plots/`:
- `scatter_completion_*` and `scatter_optimum_*`: RRSplit time (x) against SymSplit time (y) per instance, on
  log scales. Points below the dashed `x = y` line are instances where SymSplit was faster. Timed-out runs are
  plotted at the timeout; instances neither solver finished are left out.
- `cactus_*`: number of instances solved within each time, for finding the optimum and for completing the search.
- `size_delta_*`: on hard instances (both solvers hit the timeout) where the sizes differ, how many instances
  have each SymSplit size − RRSplit size. Bars right of 0 are instances where SymSplit found the larger
  common subgraph.
- `.pdf` and `.png` versions of each plot, and `.txt` files with the points as pgfplots coordinates.

It also prints, per dataset, how often SymSplit was faster, equal or slower, how many instances each solver
solved, and any instance where both solvers finished but report different solution sizes.

## All baselines
[`scripts/plot_baselines.py`](scripts/plot_baselines.py) draws one cumulative plot with a line per solver, for
every solver in `results/` (SymSplit, RRSplit and the McSplit baselines; a `+DSB` variant is dashed in the
colour of its baseline). It takes the same options as `plot_results.py`, plus:

| Option | Default | Meaning |
|---|---|---|
| `--metric` | `completion` | `completion` (time to finish the search) or `optimum` (time to find the best solution) |
| `--min-time` | `10` | Leave out instances every solver completed in less time (s) |

```shell
python scripts/plot_baselines.py --timeout 1800
```

Only instances every solver has results for are counted, so the plots fill in as the experiments progress. A run
that completes with a smaller solution than another solver found is reported as a wrong answer and not counted.
It writes `baselines_<metric>_<DATASET>.pdf`/`.png` and `ALL`, plus one pgfplots `.txt` file per solver.

## Summary tables
[`scripts/result_tables.py`](scripts/result_tables.py) prints three tables, per dataset and for all combined. It
needs only Python 3.9+ and takes the same `--timeout`, `--heuristic`, `--datasets` and `--results` options.

```shell
python scripts/result_tables.py --timeout 1800
```

- **Larger common subgraph:** on hard instances (both solvers hit the timeout), the percentage where the two
  sizes differ and, among those, the percentage where SymSplit or RRSplit found the larger common subgraph.
- **Speedup:** average and maximum of RRSplit time / SymSplit time over instances where SymSplit completed
  faster. Only instances that at least one solver completed and that at least one solver needed `--min-time`
  seconds (default 10) for are counted.
- **Symmetry pruning:** mean and standard deviation of branches pruned by symmetry as a percentage of branches
  pruned by the bound, and the percentage of instances where symmetry pruned more than the bound.

# Building and running locally
## Prerequisites
- C++17 compiler
- Make

`make` detects the OS (`uname -s`) and picks the right compiler and flags (g++ on Linux, Apple clang on macOS).

### Linux
No extra dependencies (argp is part of glibc). Install Catch2 from your package manager for `make test-unit`.
```shell
$ make
```

### macOS
Install the dependencies with [Homebrew](https://brew.sh) (works on both Apple Silicon and Intel Macs), then build.
```shell
$ brew install argp-standalone catch2   # catch2 is only needed for make test-unit
$ make
```

CMake (e.g. CLion) also works. AddressSanitizer is off by default; enable it with `-DENABLE_ASAN=ON`
(use Apple clang `/usr/bin/clang++`, as Homebrew LLVM's ASan runtime can hang on recent macOS).

## Testing
```shell
$ make test
Running LAD test...
  LAD test passed (MCS size: 7)
Running DIMACS test...
  DIMACS test passed (MCS size: 10)
...
All tests passed.
```

## Running
```shell
$ ./bin/run.o min_max ./data/tests/general/pattern ./data/tests/general/target -l -q -t 100
7, 1, 2.1875e-05, 5.5167e-05, 244, 7, 159, 0, 18, 0
```

Usage: `./bin/run.o HEURISTIC PATTERN TARGET [options]`, where `HEURISTIC` is `min_max` or `min_product`.

| Option | Meaning |
|---|---|
| `-l`, `--lad` | Read LAD format |
| `-d`, `--dimacs` | Read DIMACS format |
| `-t`, `--timeout=SECONDS` | Time limit |
| `-q`, `--quiet` | Print only the result line |
| `-c`, `--connected` | Maximum common *connected* subgraph |
| `-i`, `--directed` | Directed graphs |
| `-a`, `--labelled` | Use edge and vertex labels |
| `-x`, `--vertex-labelled-only` | Use vertex labels only |
| `-b`, `--big-first` | Try an induced subgraph isomorphism first, then decrease the target size |

To run the experiments without Docker, first download the benchmark graphs into `data/` (the archive's
SHA-256 is `5ad4332924186acd6fd5ca9e38577e2556d8e4f3ae6f7867e4c9c04a5e15941d`), then run `run.sh`:
```shell
$ curl -fL -o data.zip https://raw.githubusercontent.com/BuddhiWathsala/mcs-data/main/data.zip
$ shasum -a 256 data.zip        # sha256sum on Linux
$ unzip -q -o data.zip -x '__MACOSX/*' && rm data.zip
$ mkdir -p configs results
$ printf 'MCSP_HEURISTIC=min_max\nTIMEOUT=10\nN_PARALLEL=4\n' > configs/run.env
$ ./run.sh
```
