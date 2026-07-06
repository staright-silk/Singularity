# Project Singularity — Frontend

React + Vite dashboard for the Hawking radiation backend. No 3D yet — this
is the instrument-panel layer: live wavefunction scope, probability/flux
strip charts, and console-style controls, driven by the FastAPI backend REST
API.

## Run it

```bash
npm install
npm run dev
```

Then open the printed local URL (default `http://localhost:5173`). Make
sure the backend is running first — `uvicorn main:app --host 0.0.0.0 --port 8000`
from the `singularity-backend` folder.

By default the app calls `http://<page-hostname>:8000`. If the backend lives
elsewhere (for example Render), create a `.env` file:

```
VITE_API_URL=https://singularity-6kns.onrender.com
```

## What's here

- `src/hooks/useSimulationSocket.js` — polls REST endpoints (`/api/state`,
  `/api/params`, `/api/history`) once a second and exposes
  `{status, state, params, history, start, pause, reset, setSimParams}`.
- `src/components/ScopePanel.jsx` — the main live display. Canvas-drawn
  oscilloscope showing the bound wavefunction (cyan) and escaping Hawking
  radiation (amber) against the potential well, with the event horizon
  marked. Color coding is functional: cyan = still bound, amber = escaped.
- `src/components/StripChart.jsx` — small rolling time-series chart, used
  for retained probability and Hawking flux.
- `src/components/ControlPanel.jsx` — Start/Pause/Reset plus sliders for
  the physically meaningful parameters (black hole mass, initial momentum,
  packet width, Hawking temperature, radiation width). Sliders update
  local state live but only push updates to `/api/params` on release,
  so dragging doesn't flood the backend.
- `src/components/Readout.jsx` — small digital-style numeric readout.
- `src/styles.css` — the whole design system as CSS variables (`--bg`,
  `--accent-bound`, `--accent-radiation`, etc.) plus the instrument-panel
  styling: scope-panel HUD corners, console buttons, custom slider thumbs.

## Design notes

The two accent colors aren't decorative — they map directly to the physics:
cyan (`--accent-bound`) is always what's still inside the wavefunction,
amber (`--accent-radiation`) is always what has escaped past the horizon.
That mapping holds across the scope panel, strip charts, and readouts.

Fonts are system-stack only (`JetBrains Mono` / `IBM Plex Sans` with
generous fallbacks) — no external font CDN — since this is meant to run
on an exhibition machine that might not have reliable internet.

## Verified

- `npm run build` succeeds cleanly (no errors/warnings).
- Ran frontend + backend together and confirmed over a raw WebSocket
  client that every field the components destructure (`r`, `density`,
  `hawking`, `potential`, `time`, `step`, `probability`,
  `captured_probability`, `flux`, `rs`, `running`, and history's `times`/
  `probs`/`flux`) is present in the backend's actual messages.

## Not done yet

- Three.js 3D scene (separate piece, per your plan).
- Advanced params (`N`, `dt`, `eps`, `awl`, `awr`, `noise_amp`, `L`) are
  supported by the backend but not exposed as sliders — add them to
  `PARAM_DEFS` in `ControlPanel.jsx` if you want them tunable live. `N`
  in particular changes grid resolution/performance, worth exposing as
  an "advanced" toggle rather than a plain slider.
- No auth/multi-viewer story — anyone who can reach the WebSocket can
  drive the sim. Fine for a single exhibition kiosk, not fine if this
  goes further.
