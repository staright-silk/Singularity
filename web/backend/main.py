"""
Project Singularity — Hawking Radiation backend.

Serves the quantum simulation (solver.HawkingSimulation) over:
  - a WebSocket at /ws that streams live frames and accepts control commands
  - a REST API for one-shot control/state (useful for testing, or a UI that
    doesn't want a persistent socket)

Run with:
    uvicorn main:app --reload --host 0.0.0.0 --port 8000
"""
import asyncio
import json
import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

from report_service import get_job, start_report
from solver import HawkingSimulation

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("singularity")

sim = HawkingSimulation()

# How often we advance + broadcast physics, independent of client count.
BROADCAST_HZ = 30
STEPS_PER_BROADCAST = 3  # sim advances this many dt's per broadcast tick


class ConnectionManager:
    def __init__(self):
        self.active: list[WebSocket] = []

    async def connect(self, ws: WebSocket):
        await ws.accept()
        self.active.append(ws)
        logger.info("client connected (%d total)", len(self.active))

    def disconnect(self, ws: WebSocket):
        if ws in self.active:
            self.active.remove(ws)
        logger.info("client disconnected (%d total)", len(self.active))

    async def broadcast(self, message: dict):
        dead = []
        for ws in self.active:
            try:
                await ws.send_json(message)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self.disconnect(ws)


manager = ConnectionManager()
_loop_task: asyncio.Task | None = None


async def simulation_loop():
    """Background task: steps the sim and broadcasts frames while running."""
    period = 1.0 / BROADCAST_HZ
    while True:
        if sim.running and manager.active:
            for _ in range(STEPS_PER_BROADCAST):
                sim.step()
            await manager.broadcast({"type": "state", "data": sim.get_state()})
        elif sim.running:
            # No one connected — still advance so state isn't stale on (re)connect,
            # but at a much lower rate to avoid burning CPU for nobody.
            sim.step()
            await asyncio.sleep(period * 5)
            continue
        await asyncio.sleep(period)


@asynccontextmanager
async def lifespan(app: FastAPI):
    global _loop_task
    _loop_task = asyncio.create_task(simulation_loop())
    yield
    if _loop_task:
        _loop_task.cancel()


app = FastAPI(title="Project Singularity — Hawking Radiation Backend", lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


# ----------------------------------------------------------------------
# WebSocket
# ----------------------------------------------------------------------
@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        await websocket.send_json({"type": "state", "data": sim.get_state()})
        await websocket.send_json({"type": "params", "data": sim.get_params()})
        while True:
            raw = await websocket.receive_text()
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                await websocket.send_json({"type": "error", "message": "invalid JSON"})
                continue
            await handle_command(msg, websocket)
    except WebSocketDisconnect:
        manager.disconnect(websocket)


async def handle_command(msg: dict, websocket: WebSocket):
    cmd = msg.get("command")

    if cmd == "start":
        sim.running = True

    elif cmd == "pause":
        sim.running = False

    elif cmd == "reset":
        sim.running = False
        sim.reset()
        await websocket.send_json({"type": "state", "data": sim.get_state()})

    elif cmd == "set_params":
        was_running = sim.running
        sim.running = False
        new_params = {**sim.get_params(), **msg.get("params", {})}
        try:
            sim.set_params(**new_params)
        except TypeError as e:
            await websocket.send_json({"type": "error", "message": str(e)})
            return
        sim.reset()
        sim.running = was_running
        await manager.broadcast({"type": "params", "data": sim.get_params()})
        await websocket.send_json({"type": "state", "data": sim.get_state()})

    elif cmd == "get_history":
        await websocket.send_json({"type": "history", "data": sim.get_history()})

    elif cmd == "get_params":
        await websocket.send_json({"type": "params", "data": sim.get_params()})

    else:
        await websocket.send_json({"type": "error", "message": f"unknown command: {cmd}"})


# ----------------------------------------------------------------------
# REST fallback / testing surface
# ----------------------------------------------------------------------
class ParamUpdate(BaseModel):
    rs: float | None = None
    L: float | None = None
    N: int | None = None
    dt: float | None = None
    r0: float | None = None
    sigma: float | None = None
    p0: float | None = None
    hawking_temperature: float | None = None
    radiation_width: float | None = None
    eps: float | None = None
    awl: float | None = None
    awr: float | None = None
    noise_amp: float | None = None


class ReportRequest(BaseModel):
    label: str = "singularity_run"
    quality: str = "quick"


@app.get("/api/state")
def api_get_state():
    return sim.get_state()


@app.get("/api/history")
def api_get_history():
    return sim.get_history()


@app.get("/api/params")
def api_get_params():
    return sim.get_params()


@app.post("/api/start")
def api_start():
    sim.running = True
    return {"running": sim.running}


@app.post("/api/pause")
def api_pause():
    sim.running = False
    return {"running": sim.running}


@app.post("/api/reset")
def api_reset():
    sim.running = False
    sim.reset()
    return {"status": "reset", "state": sim.get_state()}


@app.post("/api/params")
def api_set_params(update: ParamUpdate):
    was_running = sim.running
    sim.running = False
    new_params = {**sim.get_params(), **update.model_dump(exclude_none=True)}
    sim.set_params(**new_params)
    sim.reset()
    sim.running = was_running
    return sim.get_params()


@app.post("/api/report")
def api_start_report(request: ReportRequest):
    quality = request.quality if request.quality in {"quick", "full"} else "quick"
    return start_report(label=request.label, quality=quality)


@app.get("/api/report/{job_id}")
def api_get_report(job_id: str):
    job = get_job(job_id)
    if not job:
        raise HTTPException(status_code=404, detail="report job not found")
    job.pop("traceback", None)
    return job


@app.get("/api/report/{job_id}/download")
def api_download_report(job_id: str):
    job = get_job(job_id)
    if not job:
        raise HTTPException(status_code=404, detail="report job not found")
    if job.get("status") != "ready" or not job.get("path"):
        raise HTTPException(status_code=409, detail="report is not ready yet")
    return FileResponse(job["path"], media_type="application/pdf", filename=job.get("filename", "research_report.pdf"))


@app.get("/")
def root():
    return {
        "status": "ok",
        "message": "Project Singularity backend is running.",
        "websocket": "/ws",
        "docs": "/docs",
    }
