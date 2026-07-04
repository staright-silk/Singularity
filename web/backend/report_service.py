import importlib.util
import os
import threading
import traceback
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
from pathlib import Path
from uuid import uuid4


ROOT_DIR = Path(__file__).resolve().parents[2]
MODEL_PATH = ROOT_DIR / "engines" / "python_models" / "blackhole_quantum_sim(1).py"
REPORT_ROOT = Path(__file__).resolve().parent / "reports"

_executor = ThreadPoolExecutor(max_workers=1)
_jobs = {}
_lock = threading.Lock()
_model = None


def _load_model():
    global _model
    if _model is not None:
        return _model
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    os.environ.setdefault("MPLCONFIGDIR", str(REPORT_ROOT / ".matplotlib"))
    spec = importlib.util.spec_from_file_location("blackhole_quantum_report", MODEL_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load model from {MODEL_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    _model = module
    return module


def _set_job(job_id, **patch):
    with _lock:
        _jobs[job_id] = {**_jobs.get(job_id, {}), **patch}


def get_job(job_id):
    with _lock:
        job = _jobs.get(job_id)
        return dict(job) if job else None


def _pdf_name(label):
    cleaned = "".join(ch if ch.isalnum() or ch in "-_" else "_" for ch in label).strip("_")
    return f"{cleaned or 'singularity'}_research_report.pdf"


def _run_report(job_id, label, quality):
    try:
        model = _load_model()
        output_dir = REPORT_ROOT / job_id
        os.makedirs(output_dir, exist_ok=True)

        if quality == "full":
            nx, steps, snapshots = 40, 260, 20
            momentum_values = [1.5, 2.5, 3.5, 4.5, 5.5]
            width_values = [1.5, 2.5, 3.5, 4.5]
            horizon_values = [2.5, 3.5, 4.5, 5.5]
            convergence_values = [16, 24, 32]
        else:
            nx, steps, snapshots = 28, 120, 20
            momentum_values = [2.0, 3.0, 4.0]
            width_values = [2.0, 3.0, 4.0]
            horizon_values = [3.0, 4.0, 5.0]
            convergence_values = [12, 16, 20]

        config = model.SimulationConfig(
            Nx=nx,
            Ny=nx,
            Nz=nx,
            total_steps=steps,
            snapshot_interval=snapshots,
            output_dir=str(output_dir),
            run_label=label,
        )

        _set_job(job_id, status="running", stage="baseline")
        simulation = model.BlackHoleQuantumSimulation(config)
        baseline_record = simulation.run(save_outputs=False, collect_frames=False)
        baseline_figures = simulation.save_results()

        _set_job(job_id, stage="sweeps")
        sweep = model.ParameterSweep(config)
        sweep_data = {
            "momentum_sweep": (
                sweep.sweep_momentum(momentum_values),
                os.path.join(sweep.results_dir, "momentum_sweep_plot.png"),
            ),
            "width_sweep": (
                sweep.sweep_packet_width(width_values),
                os.path.join(sweep.results_dir, "width_sweep_plot.png"),
            ),
            "horizon_sweep": (
                sweep.sweep_horizon_radius(horizon_values),
                os.path.join(sweep.results_dir, "horizon_sweep_plot.png"),
            ),
        }

        _set_job(job_id, stage="convergence")
        convergence = model.ConvergenceTest(config)
        convergence_rows = convergence.run(convergence_values)
        convergence_figure_path = os.path.join(convergence.results_dir, "convergence_plot.png")

        _set_job(job_id, stage="pdf")
        report_generator = model.ReportGenerator(str(output_dir))
        report_path = report_generator.build(
            config,
            baseline_record,
            baseline_figures,
            sweep_data,
            convergence_rows,
            convergence_figure_path,
        )

        final_path = output_dir / _pdf_name(label)
        os.replace(report_path, final_path)
        _set_job(
            job_id,
            status="ready",
            stage="done",
            path=str(final_path),
            filename=final_path.name,
            finished_at=datetime.utcnow().isoformat() + "Z",
            summary={
                "captured": baseline_record["captured_probability"],
                "reflected": baseline_record["reflected_probability"],
                "transmitted": baseline_record["transmitted_probability"],
                "energy": baseline_record["total_energy"],
            },
        )
    except Exception as exc:
        _set_job(job_id, status="failed", stage="error", error=str(exc), traceback=traceback.format_exc())


def start_report(label="singularity_run", quality="quick"):
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    job_id = uuid4().hex[:12]
    _set_job(
        job_id,
        id=job_id,
        label=label,
        quality=quality,
        status="queued",
        stage="queued",
        created_at=datetime.utcnow().isoformat() + "Z",
    )
    _executor.submit(_run_report, job_id, label, quality)
    return get_job(job_id)
