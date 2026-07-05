import { useSimulationSocket } from "../hooks/useSimulationSocket.js";
import ScopePanel from "./ScopePanel.jsx";
import StripChart from "./StripChart.jsx";
import ControlPanel from "./ControlPanel.jsx";
import Readout from "./Readout.jsx";

const STATUS_LABEL = {
  connected: "LINK ESTABLISHED",
  connecting: "LINKING…",
  disconnected: "LINK LOST — RETRYING",
};

export default function Dashboard() {
  const { status, state, params, history, start, pause, reset, setSimParams } = useSimulationSocket();

  return (
    <div className="app">
      <header className="topbar">
        <div className="brand">
          <span className="brand-eyebrow">Project Singularity</span>
          <h1 className="brand-title">Near-Horizon Model</h1>
        </div>
        <div className={`conn-status conn-status--${status}`}>
          <span className="conn-dot" />
          {STATUS_LABEL[status]}
        </div>
      </header>

      <main className="layout">
        <section className="scope-column">
          <ScopePanel state={state} />
          <div className="strips">
            <StripChart label="RETAINED PROBABILITY" data={history.probs} color="#7dcfff" />
            <StripChart label="HAWKING FLUX" data={history.flux} color="#ff9e64" />
          </div>
        </section>

        <aside className="side-column">
          <ControlPanel
            params={params}
            running={state?.running ?? false}
            onStart={start}
            onPause={pause}
            onReset={reset}
            onSetParams={setSimParams}
          />
          <div className="readouts">
            <Readout label="STEP" value={state?.step ?? 0} />
            <Readout label="TIME" value={(state?.time ?? 0).toFixed(2)} unit="s" />
            <Readout label="PROBABILITY" value={(state?.probability ?? 1).toFixed(4)} />
            <Readout label="CAPTURED" value={(state?.captured_probability ?? 0).toFixed(4)} />
          </div>
        </aside>
      </main>
    </div>
  );
}
