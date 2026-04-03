# api.py
import subprocess
import tempfile
import os
from fastapi import FastAPI, UploadFile, File, Form, Request
from fastapi.responses import JSONResponse
from slowapi import Limiter, _rate_limit_exceeded_handler
from slowapi.util import get_remote_address
from slowapi.errors import RateLimitExceeded

limiter = Limiter(key_func=get_remote_address)
app = FastAPI()
app.state.limiter = limiter
app.add_exception_handler(RateLimitExceeded, _rate_limit_exceeded_handler)

BINARY = "./bin/run.o"

@app.post("/solve")
@limiter.limit("5/day")
async def solve(
        request: Request,
        graph1: UploadFile = File(...),
        graph2: UploadFile = File(...),
        heuristic: str = Form("min_max"),        # "min_max" or "min_product"
        format: str = Form("lad"),               # "lad" or "dimacs"
        connected: bool = Form(False),
        directed: bool = Form(False),
        labelled: bool = Form(False),
        timeout: int = Form(100),
):
    with tempfile.NamedTemporaryFile(delete=False) as f1, \
            tempfile.NamedTemporaryFile(delete=False) as f2:
        f1.write(await graph1.read())
        f2.write(await graph2.read())
        f1_path, f2_path = f1.name, f2.name

    try:
        cmd = [BINARY, heuristic, f1_path, f2_path,
               f"--{format}", "--quiet", f"--timeout={timeout}"]
        if connected: cmd.append("--connected")
        if directed:  cmd.append("--directed")
        if labelled:  cmd.append("--labelled")

        result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 5)

        if result.returncode != 0:
            return JSONResponse(status_code=500, content={"error": result.stderr})

        # Parse output: line 1 is CSV stats, line 2 is the MCIS vertex mapping
        lines = result.stdout.strip().splitlines()
        fields = [x.strip() for x in lines[0].split(",")]
        keys = ["solution_size", "check_sol", "time_to_optimal", "elapsed",
                "nodes", "calls_for_optimal", "cut_branches",
                "g0_pruned", "g1_pruned", "aborted"]
        d = dict(zip(keys, fields))
        response = {k: v for k, v in d.items() if not k.startswith("g")}
        response["pruned"] = [d.get("g0_pruned"), d.get("g1_pruned")]

        g0_nodes, g1_nodes = [], []
        if len(lines) > 1 and lines[1].strip():
            for pair in lines[1].split(","):
                v, w = pair.strip().split()
                g0_nodes.append(int(v))
                g1_nodes.append(int(w))
        response["mapping"] = [g0_nodes, g1_nodes]

        return response

    finally:
        os.unlink(f1_path)
        os.unlink(f2_path)

