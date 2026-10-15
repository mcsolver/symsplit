## Prerequisites
```shell
pip install fastapi uvicorn slowapi python-multipart
```

## Run
Starting the server:
```shell
uvicorn api:app --host 0.0.0.0 --port 8000
```

## Call the API
```shell
curl -X POST http://localhost:8000/solve -F "graph1=@data/tests/pattern" -F "graph2=@data/tests/target" -F "heuristic=min_max" -F "format=lad" -F "timeout=100"
```