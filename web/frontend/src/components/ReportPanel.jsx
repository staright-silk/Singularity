import { useEffect, useState } from "react";
import { useReportJob } from "../hooks/useReportJob.js";

const STAGES = ["queued", "baseline", "sweeps", "convergence", "pdf"];
const STAGE_LABEL = {
  queued: "Queued",
  baseline: "Baseline simulation",
  sweeps: "Parameter sweeps",
  convergence: "Convergence test",
  pdf: "Assembling PDF",
  done: "Done",
  error: "Failed",
};

const HISTORY_KEY = "singularity_report_history";
const MAX_HISTORY = 8;

function loadHistory() {
  try {
    const raw = localStorage.getItem(HISTORY_KEY);
    return raw ? JSON.parse(raw) : [];
  } catch {
    return [];
  }
}
function saveHistory(list) {
  try {
    localStorage.setItem(HISTORY_KEY, JSON.stringify(list.slice(0, MAX_HISTORY)));
  } catch {
    /* storage unavailable — history just won't persist, not fatal */
  }
}

export default function ReportPanel() {
  const { job, generate, downloadUrl } = useReportJob();
  const [history, setHistory] = useState(loadHistory);

  const isRunning = job && (job.status === "queued" || job.status === "running");
  const isFailed = job?.status === "failed";
  const isReady = job?.status === "ready";

  // When a job finishes, save it into the persisted history list.
  useEffect(() => {
    if (isReady && job.id && !history.some((h) => h.id === job.id)) {
      const entry = {
        id: job.id,
        label: job.label,
        quality: job.quality,
        filename: job.filename,
        finished_at: job.finished_at,
        summary: job.summary,
      };
      const next = [entry, ...history].slice(0, MAX_HISTORY);
      setHistory(next);
      saveHistory(next);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [isReady, job?.id]);

  const currentStageIndex = job ? STAGES.indexOf(job.stage) : -1;

  return (
    <div className="report-panel glass">
      <div className="report-panel-head">
        <div>
          <h3 className="report-panel-title">Generate the full research report</h3>
          <p className="report-panel-desc">
            This re-runs the actual solver on the backend — the baseline evolution, momentum/width/horizon parameter
            sweeps, and a grid-convergence test — then assembles the results into a PDF. "Quick" finishes in roughly
            a minute; "Full" uses a larger grid and more steps and takes longer.
          </p>
        </div>
      </div>

      <div className="report-panel-actions">
        <button
          className="report-btn report-btn-primary"
          disabled={isRunning}
          onClick={() => generate("singularity_run", "quick")}
        >
          {isRunning ? "Working…" : "Generate quick report"}
        </button>
        <button
          className="report-btn"
          disabled={isRunning}
          onClick={() => generate("singularity_run", "full")}
        >
          Generate full report
        </button>
      </div>

      {job && !isFailed && (
        <div className="report-progress">
          {STAGES.map((stage, i) => (
            <div
              key={stage}
              className={
                "report-progress-step" +
                (i < currentStageIndex || isReady ? " is-done" : "") +
                (i === currentStageIndex && !isReady ? " is-active" : "")
              }
            >
              <span className="report-progress-dot" />
              <span className="report-progress-label mono">{STAGE_LABEL[stage]}</span>
            </div>
          ))}
        </div>
      )}

      {isFailed && (
        <div className="report-status mono">
          <span className="report-status-error">Failed: {job.error || "unknown error"}</span>
        </div>
      )}

      {isReady && (
        <div className="report-summary">
          {job.summary && (
            <div className="report-summary-grid mono">
              <div>
                <span className="report-summary-label">Captured</span>
                <span className="report-summary-value">{formatPct(job.summary.captured)}</span>
              </div>
              <div>
                <span className="report-summary-label">Reflected</span>
                <span className="report-summary-value">{formatPct(job.summary.reflected)}</span>
              </div>
              <div>
                <span className="report-summary-label">Transmitted</span>
                <span className="report-summary-value">{formatPct(job.summary.transmitted)}</span>
              </div>
              <div>
                <span className="report-summary-label">Total energy</span>
                <span className="report-summary-value">{formatNum(job.summary.energy)}</span>
              </div>
            </div>
          )}
          <a className="report-btn report-btn-download" href={downloadUrl} download>
            Download PDF
          </a>
        </div>
      )}

      {history.length > 0 && (
        <div className="report-history">
          <div className="report-history-title mono">Previous reports (this browser)</div>
          {history.map((h) => (
            <div key={h.id} className="report-history-row">
              <div className="report-history-meta">
                <span className="report-history-name">{h.filename || h.id}</span>
                <span className="report-history-time mono">
                  {h.quality} · {h.finished_at ? new Date(h.finished_at).toLocaleString() : ""}
                </span>
              </div>
              <a
                className="report-btn report-btn-small"
                href={`${downloadUrlBase()}/api/report/${h.id}/download`}
                download
              >
                Download
              </a>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

function downloadUrlBase() {
  return import.meta.env.VITE_API_URL || `http://${window.location.hostname}:8000`;
}

function formatPct(v) {
  if (typeof v !== "number") return "—";
  return `${(v * 100).toFixed(1)}%`;
}
function formatNum(v) {
  if (typeof v !== "number") return "—";
  return v.toFixed(4);
}
