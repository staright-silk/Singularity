import { useEffect, useMemo, useRef, useState } from "react";

const API_BASE = import.meta.env.VITE_API_URL || `http://${window.location.hostname}:8000`;

const STAGE_LABELS = {
  queued: "Queued",
  baseline: "Baseline run",
  sweeps: "Parameter sweeps",
  convergence: "Convergence check",
  pdf: "Composing PDF",
  done: "Ready",
  error: "Failed",
};

export default function ReportPanel({ state }) {
  const [job, setJob] = useState(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  const timerRef = useRef(null);

  const defaultLabel = useMemo(() => {
    const step = state?.step ?? 0;
    return `singularity_step_${String(step).padStart(5, "0")}`;
  }, [state?.step]);

  useEffect(() => {
    if (!job || !["queued", "running"].includes(job.status)) return;
    timerRef.current = setInterval(async () => {
      try {
        const response = await fetch(`${API_BASE}/api/report/${job.id}`);
        if (!response.ok) throw new Error("Could not read report job");
        const nextJob = await response.json();
        setJob(nextJob);
        if (!["queued", "running"].includes(nextJob.status)) {
          clearInterval(timerRef.current);
          setBusy(false);
        }
      } catch (err) {
        setError(err.message);
        setBusy(false);
        clearInterval(timerRef.current);
      }
    }, 1800);

    return () => clearInterval(timerRef.current);
  }, [job?.id, job?.status]);

  async function generateReport(quality = "quick") {
    setBusy(true);
    setError("");
    try {
      const response = await fetch(`${API_BASE}/api/report`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ label: defaultLabel, quality }),
      });
      if (!response.ok) throw new Error("Report generator did not start");
      setJob(await response.json());
    } catch (err) {
      setError(err.message);
      setBusy(false);
    }
  }

  const ready = job?.status === "ready";
  const failed = job?.status === "failed";
  const stage = STAGE_LABELS[job?.stage] || "Idle";
  const downloadUrl = ready ? `${API_BASE}/api/report/${job.id}/download` : null;

  return (
    <section className="report-panel">
      <div className="panel-heading">
        <div>
          <span className="panel-kicker">Research PDF</span>
          <h2>Export packet</h2>
        </div>
        <span className={`report-pill report-pill--${ready ? "ready" : failed ? "failed" : busy ? "busy" : "idle"}`}>
          {ready ? "READY" : failed ? "FAILED" : busy ? "BUILDING" : "IDLE"}
        </span>
      </div>

      <div className="report-preview">
        <div className="report-sheet">
          <span />
          <span />
          <span />
          <span />
        </div>
        <div className="report-copy">
          <strong>{stage}</strong>
          <span>
            {ready
              ? "Plots, sweeps, convergence tables, and final metrics are packed into the PDF."
              : "Generates the 3D report from the black-hole quantum model in the background."}
          </span>
        </div>
      </div>

      {job?.summary && (
        <div className="report-metrics">
          <span>Captured {job.summary.captured.toFixed(4)}</span>
          <span>Reflected {job.summary.reflected.toFixed(4)}</span>
          <span>Transmitted {job.summary.transmitted.toFixed(4)}</span>
        </div>
      )}

      {error && <div className="report-error">{error}</div>}
      {failed && job?.error && <div className="report-error">{job.error}</div>}

      <div className="report-actions">
        <button className="console-btn report-btn-main" onClick={() => generateReport("quick")} disabled={busy}>
          Generate
        </button>
        <button className="console-btn" onClick={() => generateReport("full")} disabled={busy}>
          Full
        </button>
        <a className={`console-btn report-download ${ready ? "" : "is-disabled"}`} href={downloadUrl || "#"}>
          Download
        </a>
      </div>
    </section>
  );
}
