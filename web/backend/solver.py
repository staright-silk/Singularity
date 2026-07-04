import numpy as np


class HawkingSimulation:
    def __init__(self, **params):
        self.set_params(**params)
        self.reset()

    def set_params(
        self,
        rs=2.0,           # Schwarzschild radius
        L=100.0,          # domain length
        N=4096,           # grid points
        dt=0.005,         # timestep
        r0=30.0,          # initial packet center
        sigma=2.0,        # initial width
        p0=-0.5,          # initial momentum
        hawking_temperature=0.15,
        radiation_width=3.0,
        l=0,
        rng_seed: int | None = None,
        eps=0.01,
        awl=5.0,          # absorber left width
        awr=15.0,         # absorber right width
        noise_amp=0.002,
    ):
        """Set simulation params."""
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

        # Schwarzschild-ish potential
        rr = np.maximum(self.r, self.rs + self.eps)
        factor = 1.0 - (self.rs / rr)
        self.V = factor * (self.l * (self.l + 1) / rr**2 + (self.rs) / rr**3)

        # Absorbing boundaries.
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
        self.UK = np.exp(-0.5j * self.k**2 * self.dt)

        self.hawking_mask = (self.r >= self.rs) & (self.r <= self.rs + self.radiation_width)
        self._n_hawking = int(np.sum(self.hawking_mask))
        if self.rng_seed is None:
            self._rng = np.random.default_rng()
        else:
            self._rng = np.random.default_rng(self.rng_seed)

    def reset(self):
        """Reset packet and history."""
        self._build_grid()
        psi = np.exp(-(self.r - self.r0) ** 2 / (4 * self.sigma**2)) * np.exp(1j * self.p0 * self.r)
        psi /= np.sqrt(np.sum(np.abs(psi) ** 2) * self.dr)
        self.psi = psi
        self._last_noise = np.zeros_like(psi)

        self.captured = 0.0
        self.step_count = 0
        self.time = 0.0
        self.times, self.probs, self.flux = [], [], []
        self.running = False

    def step(self):
        """Advance one tick."""
        psi = self.psi * self.UV

        # small noisy source near the horizon
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

        # rough absorber loss estimate
        absorbed = 2.0 * float(np.sum(self.W * np.abs(psi) ** 2) * self.dr) * self.dt
        if absorbed < 0:
            absorbed = 0.0
        self.captured += absorbed

        self.step_count += 1
        self.time = self.step_count * self.dt
        self.times.append(self.time)
        self.probs.append(P)
        self.flux.append(H)
        return P, H

    def get_state(self, downsample: int = 4) -> dict:
        """Current frame for the frontend."""
        idx = slice(None, None, max(1, downsample))
        density = np.abs(self.psi) ** 2

        h_disp = np.zeros_like(self.r)  # hawking display
        h_disp[self.hawking_mask] = 100 * np.abs(self._last_noise[self.hawking_mask]) ** 2

        return {
            "r": self.r[idx].tolist(),
            "density": density[idx].tolist(),
            "hawking": h_disp[idx].tolist(),
            "potential": self.V[idx].tolist(),
            "time": self.time,
            "step": self.step_count,
            "probability": self.probs[-1] if self.probs else 1.0,
            "captured_probability": float(self.captured),
            "flux": self.flux[-1] if self.flux else 0.0,
            "rs": self.rs,
            "running": self.running,
        }

    def get_history(self, max_points: int = 500) -> dict:
        """History for the strip charts."""
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
