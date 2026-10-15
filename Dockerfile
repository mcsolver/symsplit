# syntax=docker/dockerfile:1
#
# Reproduces the MCS experiments (BI, CV, LV, SI) for any McSplit-style solver whose
# `make build` produces bin/run.o (SymSplit, RRSplit and the McSplit baselines). End to end it:
#   1. clones REPO_URL at REPO_REF and adds this project's run.sh and indicators/, so every
#      solver runs the same instances in the same way
#   2. downloads the benchmark graphs (too large for git) from DATA_URL
#   3. builds the solver and runs SMOKE_TEST
#   4. at run time, writes configs/run.env from the environment and runs run.sh
#
# Usually driven by compose.yaml (`docker compose build`, `docker compose run --rm symsplit`).
# Standalone:
#   docker build -t symsplit --build-arg REPO_REF=dev .
#   docker run --rm -v "$PWD/results/symsplit:/app/results" \
#     -e MCSP_HEURISTIC=min_max -e TIMEOUT=1000 -e N_PARALLEL=8 symsplit

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential ca-certificates curl git unzip \
 && rm -rf /var/lib/apt/lists/*

# 1. Clone the repository. Pin REPO_REF to a tag or commit for exact reproduction.
ARG REPO_URL=https://github.com/mcsolver/symsplit.git
ARG REPO_REF=main
WORKDIR /app
RUN GIT_TERMINAL_PROMPT=0 git clone "$REPO_URL" . \
 && git checkout "$REPO_REF" \
 && git log -1 --format='Building commit %H (%s)' \
 || { echo "Cannot clone $REPO_URL at $REPO_REF; is it public and pushed?"; exit 1; }
COPY --chmod=755 run.sh ./run.sh
COPY indicators/ ./indicators/

# 2. Fetch the benchmark data (hosted in github.com/BuddhiWathsala/mcs-data).
#    data.zip has a top-level data/ folder, so it is unpacked over the repo's data/.
ARG DATA_URL=https://raw.githubusercontent.com/BuddhiWathsala/mcs-data/main/data.zip
ARG DATA_SHA256=5ad4332924186acd6fd5ca9e38577e2556d8e4f3ae6f7867e4c9c04a5e15941d
RUN curl -fL --retry 5 -o /tmp/data.zip "$DATA_URL" \
 && if [ -n "$DATA_SHA256" ]; then echo "$DATA_SHA256  /tmp/data.zip" | sha256sum -c -; fi \
 && unzip -q -o /tmp/data.zip -x '__MACOSX/*' -d . \
 && rm /tmp/data.zip \
 && for d in biochemicalReactions images-CVIU11 LV si; do \
      test -d "data/$d" || { echo "data/$d missing from archive"; exit 1; }; \
    done

# 3. Build and sanity-check the solver. The default smoke test solves one LV pair,
#    which works for any solver; override it with the repo's own tests if it has them.
ARG SMOKE_TEST="./bin/run.o min_max ./data/LV/patterns/g10 ./data/LV/targets/g30 -l -q -t 10"
RUN mkdir -p bin && make build && sh -c "$SMOKE_TEST"

# 4. Results directory and entrypoint that writes configs/run.env, then runs run.sh.
RUN mkdir -p results configs
COPY --chmod=755 <<'SCRIPT' /usr/local/bin/entrypoint.sh
#!/bin/bash
set -euo pipefail
cd /app
mkdir -p results configs
cat > configs/run.env <<ENV
MCSP_HEURISTIC=${MCSP_HEURISTIC}
TIMEOUT=${TIMEOUT}
N_PARALLEL=${N_PARALLEL}
ENV
echo "configs/run.env:"; cat configs/run.env
exec ./run.sh "$@"
SCRIPT

ENV MCSP_HEURISTIC=min_max \
    TIMEOUT=1000 \
    N_PARALLEL=4
VOLUME ["/app/results"]
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
