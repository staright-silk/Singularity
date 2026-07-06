# Singularity

A numerical black-hole quantum simulation stack.

This repository contains two related pieces:

- a live web dashboard for a 1D radial Hawking-radiation model
- standalone Python research engines for black-hole wave-packet scattering

The current web app is the cleanest entry point. It evolves a complex wavefunction near a Schwarzschild-like horizon, injects a stochastic Hawking-like source near the horizon, tracks retained probability and emitted flux, and streams the result to a React instrument panel.

This is not a full quantum-gravity simulator. It is a controlled numerical model built from standard ingredients: a wave packet, an effective black-hole potential, split-operator Fourier evolution, absorbing boundaries, and diagnostics.

## Repository Layout

```text
Project Singularity/
+-- engines/
|   +-- python_models/
|       +-- blackhole_quantum_sim(1).py
|       +-- quantum3_hawking_complete.py
+-- web/
    +-- backend/
    |   +-- main.py
    |   +-- solver.py
    |   +-- requirements.txt
    |   +-- test_client.html
    +-- frontend/
        +-- package.json
        +-- src/
        |   +-- App.jsx
        |   +-- hooks/useSimulationSocket.js
        |   +-- components/
        +-- vite.config.js
```

## What The Simulation Does

The live backend solves a 1D radial wave equation in a Schwarzschild-inspired background.

The state variable is a complex wavefunction:

```text
psi(r, t)
```

The plotted density is:

```text
|psi(r, t)|^2
```

The solver starts with a normalized Gaussian wave packet:

```python
psi = np.exp(-(r - r0) ** 2 / (4 * sigma**2)) * np.exp(1j * p0 * r)
psi /= np.sqrt(np.sum(np.abs(psi) ** 2) * dr)
```

That packet moves on a radial grid outside the event horizon. The horizon is represented by the Schwarzschild radius `rs`. The backend keeps the grid just outside the horizon:

```python
r_start = rs + 0.05
r = np.linspace(r_start, r_start + L, N, endpoint=False)
```

The effective potential used by the web solver is:

```python
rr = np.maximum(r, rs + eps)
factor = 1.0 - (rs / rr)
V = factor * (l * (l + 1) / rr**2 + rs / rr**3)
```

This is a compact Schwarzschild-like scalar potential. It gives the wave packet a horizon-dependent scattering environment without pretending to solve the full Einstein field equations.

## Numerical Method

The live solver uses a split-operator FFT step. Each time step is:

```text
half potential step
full kinetic step in Fourier space
half potential step
```

In code:

```python
psi = psi * UV
psik = np.fft.fft(psi)
psik *= UK
psi = np.fft.ifft(psik)
psi = psi * UV
```

The operators are precomputed:

```python
Veff = V - 1j * W
UV = np.exp(-1j * Veff * dt / 2)
UK = np.exp(-1j * 0.5 * k**2 * dt)
```

`W` is an imaginary absorbing layer. It removes outgoing probability near the grid boundaries so the simulation does not reflect waves back into the domain.

The absorbed probability is estimated each step:

```python
absorbed = 2.0 * np.sum(W * np.abs(psi) ** 2) * dr * dt
captured += absorbed
```

That value appears in the UI as `CAPTURED`.

## Hawking-Like Emission Model

The web solver adds a small stochastic source near the horizon. The emission region is:

```python
hawking_mask = (r >= rs) & (r <= rs + radiation_width)
```

At each step the source is regenerated with random complex phase:

```python
phase = np.exp(2j * np.pi * rng.random(n_hawking))
hw = (
    np.sqrt(hawking_temperature)
    * phase
    * np.exp(-((r[hawking_mask] - (rs + 0.8)) ** 2) / (2 * radiation_width**2))
)
noise[hawking_mask] = hw
psi = psi + noise_amp * noise
```

The source strength is controlled mainly by:

- `hawking_temperature`
- `radiation_width`
- `noise_amp`

The displayed Hawking flux is not a measured astrophysical flux. It is the norm of the injected near-horizon stochastic source:

```python
H = np.sum(np.abs(noise) ** 2) * dr
```

That is useful for visualizing the model response as temperature and horizon size change.

## Web Architecture

The browser does not run the physics. The backend owns the simulation state.

```text
React UI  <---- WebSocket JSON ---->  FastAPI backend  ---->  HawkingSimulation
```

The backend broadcasts simulation frames at about 30 Hz. Each broadcast advances three physics steps:

```python
BROADCAST_HZ = 30
STEPS_PER_BROADCAST = 3
```

The frontend receives:

```json
{
  "type": "state",
  "data": {
    "r": [],
    "density": [],
    "hawking": [],
    "potential": [],
    "time": 0.0,
    "step": 0,
    "probability": 1.0,
    "captured_probability": 0.0,
    "flux": 0.0,
    "rs": 2.0,
    "running": false
  }
}
```

The main scope panel draws:

- cyan trace: retained probability density, `|psi|^2`
- amber trace: near-horizon Hawking-like source display
- gray trace: effective potential
- vertical marker: event horizon, `r = rs`

The dashboard controls the parameters that matter visually and physically:

| UI Control | Backend Parameter | Meaning |
|---|---:|---|
| Black hole mass | `rs` | Schwarzschild radius used as the horizon scale |
| Initial momentum | `p0` | Incoming radial momentum of the packet |
| Packet width | `sigma` | Width of the initial Gaussian |
| Hawking temperature | `hawking_temperature` | Source amplitude scale |
| Radiation width | `radiation_width` | Width of the near-horizon emission region |

## Run The Live App

Open two terminals.

Terminal 1, backend:

```bash
cd web/backend
python -m pip install -r requirements.txt
uvicorn main:app --reload --host 0.0.0.0 --port 8000
```

Terminal 2, frontend:

```bash
cd web/frontend
npm install
npm run dev
```

Then open the Vite URL, usually:

```text
http://localhost:5173
```

The frontend connects by default to:

```text
ws://<page-hostname>:8000/ws
```

To point it at another backend machine, create `web/frontend/.env`:

```bash
VITE_WS_URL=ws://192.168.1.50:8000/ws
```

## Backend API

The backend exposes both WebSocket and REST controls.

Main routes:

| Route | Method | Purpose |
|---|---:|---|
| `/ws` | WebSocket | Live state stream and controls |
| `/api/state` | GET | Current state frame |
| `/api/history` | GET | Probability and flux history |
| `/api/params` | GET | Current parameters |
| `/api/params` | POST | Update parameters and reset |
| `/api/start` | POST | Start stepping |
| `/api/pause` | POST | Pause stepping |
| `/api/reset` | POST | Reset wave packet and history |
| `/docs` | GET | FastAPI interactive docs |

WebSocket commands look like this:

```json
{ "command": "start" }
```

```json
{ "command": "pause" }
```

```json
{ "command": "reset" }
```

```json
{
  "command": "set_params",
  "params": {
    "rs": 3.0,
    "p0": -3.5,
    "sigma": 2.5,
    "hawking_temperature": 0.25
  }
}
```

You can test the backend without the React app:

```bash
cd web/backend
uvicorn main:app --reload --host 0.0.0.0 --port 8000
```

Then open:

```text
web/backend/test_client.html
```

## Core Parameters

These are the important numerical and physical parameters in `web/backend/solver.py`.

| Parameter | Default | Meaning |
|---|---:|---|
| `rs` | `2.0` | Schwarzschild radius / horizon scale |
| `L` | `100.0` | Radial domain length |
| `N` | `4096` | Number of radial grid points |
| `dt` | `0.005` | Time step |
| `r0` | `30.0` | Initial wave-packet center |
| `sigma` | `2.0` | Initial wave-packet width |
| `p0` | `-2.5` | Initial radial momentum |
| `hawking_temperature` | `0.15` | Stochastic source amplitude scale |
| `radiation_width` | `3.0` | Near-horizon source width |
| `l` | `0` | Angular momentum mode in the potential |
| `eps` | `0.01` | Horizon softening term |
| `awl` | `5.0` | Left absorbing-layer width |
| `awr` | `15.0` | Right absorbing-layer width |
| `noise_amp` | `0.002` | Coupling strength for injected noise |

## Standalone Python Engines

The `engines/python_models` folder contains research scripts that are separate from the live web app.

### `quantum3_hawking_complete.py`

This is the compact Matplotlib version of the 1D Hawking-like model. It is useful for quick local experiments without the web stack.

Run:

```bash
cd engines/python_models
python quantum3_hawking_complete.py
```

It evolves the same general idea:

- 1D radial grid outside `rs`
- incoming Gaussian packet
- absorbing boundaries
- stochastic near-horizon source
- Matplotlib plots for density, probability, and flux

### `blackhole_quantum_sim(1).py`

This is the heavier 3D research engine. It uses a Cartesian grid, a Schwarzschild-inspired central potential, split-operator FFT evolution, diagnostics, parameter sweeps, convergence tests, images, GIF output, CSV files, and a generated PDF report.

The 3D engine is organized around these classes:

| Class | Role |
|---|---|
| `SimulationConfig` | All grid, potential, packet, time-step, and output settings |
| `SchwarzschildPotential` | Central potential and capture mask |
| `AbsorbingBoundary` | Complex boundary absorber on all box faces |
| `WavePacket` | Initial 3D Gaussian wave packet |
| `SplitOperatorSolver` | FFT-based time evolution |
| `Diagnostics` | Norm, energy, captured, reflected, transmitted probability |
| `Visualization` | Slices, profiles, isosurfaces, animations |
| `ParameterSweep` | Momentum, width, horizon, absorber, and time-step sweeps |
| `ConvergenceTest` | Resolution sensitivity checks |
| `ReportGenerator` | PDF report assembly |

Run:

```bash
cd engines/python_models
python 'blackhole_quantum_sim(1).py'
```

Expected output directory:

```text
bh_quantum_results/
+-- animations/
+-- convergence/
+-- csv/
+-- figures/
+-- sweeps/
+-- research_report.pdf
```

The 3D script requires packages beyond the web backend:

```bash
python -m pip install numpy matplotlib scikit-image pillow reportlab
```

## Scientific Notes

The model is intentionally numerical and approximate.

What it does model:

- wave-packet scattering in a black-hole-inspired potential
- probability loss through absorbing regions
- horizon-local stochastic emission as a Hawking-like source
- sensitivity to packet momentum, width, horizon scale, time step, and grid resolution

What it does not model:

- full general relativity
- backreaction of radiation on the metric
- quantum field theory on curved spacetime from first principles
- real astrophysical evaporation rates
- particle species, spin, greybody factors, or thermodynamic consistency

The point is to make the dynamics inspectable. You can see the packet move, lose probability, interact with the potential, and respond to source terms.

## Numerical Stability

Good defaults for the live solver:

```python
N = 4096
dt = 0.005
L = 100.0
sigma = 2.0
p0 = -2.5
```

If the browser or backend struggles, reduce the grid:

```json
{
  "command": "set_params",
  "params": {
    "N": 2048
  }
}
```

If the wave packet behaves too sharply, reduce `dt` or widen `sigma`.

If boundary reflection becomes visible, increase `awl` or `awr`.

If the Hawking trace dominates the display, reduce `hawking_temperature` or `noise_amp`.

## Minimal Backend Usage From Python

You can use the solver directly:

```python
from solver import HawkingSimulation

sim = HawkingSimulation(rs=2.0, N=2048, rng_seed=42)

for _ in range(1000):
    probability, flux = sim.step()

state = sim.get_state()
history = sim.get_history()

print(state["time"])
print(state["probability"])
print(state["captured_probability"])
print(state["flux"])
```

From `web/backend`, run that in a Python shell or small script.

## Current Status

Working:

- FastAPI backend
- WebSocket streaming
- REST fallback
- React + Vite frontend
- live wavefunction scope
- probability and Hawking-flux strip charts
- parameter controls
- standalone 1D Matplotlib simulation
- standalone 3D report-generating simulation

Not yet built:

- full 3D browser rendering
- authentication or multi-user control rules
- production deployment config
- unified package/environment file for both web and research engines

## Main Files

Start here:

- `web/backend/solver.py` for the live physics engine
- `web/backend/main.py` for the API and WebSocket server
- `web/frontend/src/hooks/useSimulationSocket.js` for the client protocol
- `web/frontend/src/components/ScopePanel.jsx` for the oscilloscope rendering
- `engines/python_models/quantum3_hawking_complete.py` for the original 1D script
- `engines/python_models/blackhole_quantum_sim(1).py` for the 3D research engine
