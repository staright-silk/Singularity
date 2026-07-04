"""
Core physics engine for Project Singularity's Hawking radiation module.

This is a direct refactor of quantum3_hawking_complete.py into a stateful
class that can be stepped, paused, reset, and reconfigured on the fly from
a server loop instead of driving a matplotlib animation.

Numerics are unchanged from the original script:
  - Split-operator Fourier method (Strang splitting: V/2, K, V/2)
  - Absorbing boundary layers (quadratic imaginary potential) at both ends
  - Stochastic Hawking radiation source injected just outside the horizon
"""
import numpy as np


class HawkingSimulation:
    def __init__(self, **params):
        self.set_params(**params)
        self.reset()

    # ------------------------------------------------------------------
    # Configuration
    # ------------------------------------------------------------------
    def set_params(
        self,
        rs=2.0,
        L=100.0,
        N=4096,
        dt=0.005,
        r0=30.0,
        sigma=2.0,
        p0=-2.5,
        hawking_temperature=0.15,
        radiation_width=3.0,
        l=0,
        rng_seed: int | None = None,
        eps=0.01,
        awl=5.0,
        awr=15.0,
        noise_amp=0.002,
    ):
        """Set (or update) all physical/numerical parameters and rebuild the grid."""
        self.rs = float(rs)
        self.L = float(L)
        self.N = int(N)
        self.dt = float(dt)
        self.r0 = float(r0)
        self.sigma = float(sigma)
        self.p0 = float(p0)
        self.hawking_temperature = float(hawking_temperature)
        self.radiation_width = float(radiation_width)
        self.l = int(l)
        self.rng_seed = rng_seed
        self.eps = float(eps)
        self.awl = float(awl)
        self.awr = float(awr)
        self.noise_amp = float(noise_amp)
        self._build_grid()

    def get_params(self) -> dict:
        return {
            "rs": self.rs,
            "L": self.L,
            "N": self.N,
            "dt": self.dt,
            "r0": self.r0,
            "sigma": self.sigma,
            "p0": self.p0,
            "hawking_temperature": self.hawking_temperature,
            "radiation_width": self.radiation_width,
            "eps": self.eps,
            "awl": self.awl,
            "awr": self.awr,
            "noise_amp": self.noise_amp,
        }

    def _build_grid(self):
        r_start = self.rs + 0.05
        self.r = np.linspace(r_start, r_start + self.L, self.N, endpoint=False)
        self.dr = self.r[1] - self.r[0]
        self.k = 2 * np.pi * np.fft.fftfreq(self.N, d=self.dr)

        # Regge-Wheeler / scalar-like effective potential (approximate)
        # V(r) = (1 - rs/r) * ( l(l+1)/r^2 + rs / r^3 )
        # Use eps to avoid exact divergence at the horizon r=rs.
        rr = np.maximum(self.r, self.rs + self.eps)
        factor = 1.0 - (self.rs / rr)
        self.V = factor * (self.l * (self.l + 1) / rr**2 + (self.rs) / rr**3)

        W = np.zeros_like(self.r)
        mask = self.r < (self.rs + self.awl)
        x = (self.rs + self.awl - self.r[mask]) / self.awl
        W[mask] = 60 * x**2
        mask = self.r > (r_start + self.L - self.awr)
        x = (self.r[mask] - (r_start + self.L - self.awr)) / self.awr
        W[mask] = 60 * x**2
        self.W = W

        Veff = self.V - 1j * self.W
        self.UV = np.exp(-1j * Veff * self.dt / 2)
        self.UK = np.exp(-1j * 0.5 * self.k**2 * self.dt)

        self.hawking_mask = (self.r >= self.rs) & (self.r <= self.rs + self.radiation_width)
        self._n_hawking = int(np.sum(self.hawking_mask))
        # RNG for reproducible stochastic source
        if self.rng_seed is None:
            self._rng = np.random.default_rng()
        else:
            self._rng = np.random.default_rng(self.rng_seed)

    # ------------------------------------------------------------------
    # State control
    # ------------------------------------------------------------------
    def reset(self):
        """Reinitialize the wavepacket and clear history. Rebuilds the grid too,
        so this is safe to call right after set_params()."""
        self._build_grid()
        psi = np.exp(-(self.r - self.r0) ** 2 / (4 * self.sigma**2)) * np.exp(1j * self.p0 * self.r)
        psi /= np.sqrt(np.sum(np.abs(psi) ** 2) * self.dr)
        self.psi = psi
        self._last_noise = np.zeros_like(psi)

        # cumulative absorbed probability via imaginary potential W
        self.captured = 0.0

        self.step_count = 0
        self.time = 0.0
        self.times: list[float] = []
        self.probs: list[float] = []
        self.flux: list[float] = []
        self.running = False

    def step(self):
        """Advance the simulation by one dt. Returns (probability, hawking_flux)."""
        psi = self.psi * self.UV
        noise = np.zeros_like(psi, dtype=complex)
        phase = np.exp(2j * np.pi * self._rng.random(self._n_hawking))
        hw = (
            np.sqrt(self.hawking_temperature)
            * phase
            * np.exp(-((self.r[self.hawking_mask] - (self.rs + 0.8)) ** 2) / (2 * self.radiation_width**2))
        )
        noise[self.hawking_mask] = hw
        psi = psi + self.noise_amp * noise

        psik = np.fft.fft(psi)
        psik *= self.UK
        psi = np.fft.ifft(psik)
        psi = psi * self.UV

        self.psi = psi
        self._last_noise = noise

        P = float(np.sum(np.abs(psi) ** 2) * self.dr)
        H = float(np.sum(np.abs(noise) ** 2) * self.dr)

        # Estimate absorbed probability during this step from the imaginary potential W
        # dP/dt = -2 * ∫ W |psi|^2 dr  => ΔP_absorbed ≈ 2 * ∑ W |psi|^2 * dr * dt
        absorbed = 2.0 * float(np.sum(self.W * np.abs(psi) ** 2) * self.dr) * self.dt
        # ensure non-negative
        if absorbed < 0:
            absorbed = 0.0
        self.captured += absorbed

        self.step_count += 1
        self.time = self.step_count * self.dt
        self.times.append(self.time)
        self.probs.append(P)
        self.flux.append(H)
        return P, H

    # ------------------------------------------------------------------
    # State export (for WebSocket / REST)
    # ------------------------------------------------------------------
    def get_state(self, downsample: int = 4) -> dict:
        """Snapshot of the current frame, downsampled for network transfer.
        With N=4096 and downsample=4 this sends ~1024 points/field per frame."""
        idx = slice(None, None, max(1, downsample))
        density = np.abs(self.psi) ** 2

        hawking_disp = np.zeros_like(self.r)
        hawking_disp[self.hawking_mask] = 100 * np.abs(self._last_noise[self.hawking_mask]) ** 2

        return {
            "r": self.r[idx].tolist(),
            "density": density[idx].tolist(),
            "hawking": hawking_disp[idx].tolist(),
            "potential": self.V[idx].tolist(),
            "time": self.time,
            "step": self.step_count,
            "probability": self.probs[-1] if self.probs else 1.0,
            # use cumulative absorbed probability computed from W rather than 1-P
            "captured_probability": float(self.captured),
            "flux": self.flux[-1] if self.flux else 0.0,
            "rs": self.rs,
            "running": self.running,
        }

    def get_history(self, max_points: int = 500) -> dict:
        """Downsampled full time series for the probability/flux strip charts."""
        n = len(self.times)
        if n == 0:
            return {"times": [], "probs": [], "flux": []}
        if n <= max_points:
            idx = range(n)
        else:
            step = max(1, n // max_points)
            idx = range(0, n, step)
        return {
            "times": [self.times[i] for i in idx],
            "probs": [self.probs[i] for i in idx],
            "flux": [self.flux[i] for i in idx],
        }
