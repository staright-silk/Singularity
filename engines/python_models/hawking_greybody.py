"""
Semiclassical Hawking Radiation via Numerically Computed Greybody Factors
==========================================================================

PHYSICS SUMMARY
----------------
A free massless scalar field propagating on a Schwarzschild background obeys
a wave equation that separates (in Schwarzschild time t, tortoise coordinate
r*, and spherical harmonics Y_lm) into a 1D scattering (Regge-Wheeler) equation:

    d^2 psi / dr*^2 + [ omega^2 - V_l(r) ] psi = 0

    V_l(r) = f(r) * [ l(l+1)/r^2 + (1 - s^2) * rs / r^3 ],   f(r) = 1 - rs/r

where rs = 2GM/c^2 is the Schwarzschild radius and s is the field spin
(s = 0 for the scalar field used here). The tortoise coordinate is defined by
    dr*/dr = 1/f(r)  =>  r* = r + rs * ln(r/rs - 1)

This is the SAME mathematical structure as 1D quantum-mechanical barrier
scattering: V_l(r) is a real potential barrier that vanishes at both the
horizon (r* -> -infinity) and spatial infinity (r* -> +infinity). Solving
it numerically for the transmission probability gives the "greybody factor"
Gamma_l(omega): the fraction of a mode's flux that tunnels/propagates from
the horizon out to infinity (or, by time-reversal symmetry, the fraction of
an incident external wave that is absorbed).

Hawking's 1974 semiclassical result is that field modes emitted AT the
horizon are thermally populated at the Hawking temperature

    T_H = kappa / (2 pi) = 1 / (4 pi rs)        (units: G = c = hbar = k_B = 1)

Multiplying the thermal (Bose-Einstein) occupation number by the numerically
computed greybody factor gives the physically observed emission spectrum at
infinity (this is exactly the calculation used in the literature, e.g. Page,
Phys. Rev. D 13, 198 (1976)):

    dN/(dt domega)  = (1/2pi) * sum_l (2l+1) * Gamma_l(omega) / (exp(omega/T_H) - 1)
    dE/(dt domega)  = omega * dN/(dt domega)

Because Gamma_l(omega) -> 0 as omega -> 0 and rises smoothly to ~1 at high
omega, the observed spectrum is systematically suppressed at low frequency
relative to a perfect blackbody at T_H -- this "greybody suppression" is a
genuine, checkable prediction of the theory, and reproducing it numerically
(rather than assuming a thermal shape by hand) is the actual scientific
content of this simulation.

UNITS: G = c = hbar = k_B = 1 throughout. Black hole mass M sets the only
physical scale; rs = 2M.
"""

import numpy as np
from scipy.interpolate import interp1d
from scipy.integrate import solve_ivp, simpson


class SchwarzschildGreybody:
    """
    Computes transmission (greybody) factors for a massless scalar field
    scattering off the Regge-Wheeler potential of a Schwarzschild black hole,
    by direct numerical integration of the wave equation (shooting method).
    """

    def __init__(self, M=1.0, r_max_factor=80.0, n_r=20000, spin=0):
        self.M = M
        self.rs = 2.0 * M
        self.T_H = 1.0 / (4.0 * np.pi * self.rs)  # Hawking temperature
        self.spin = spin

        # Build an r-grid finely resolved near the horizon (where the
        # tortoise coordinate map is most rapidly varying) and coarser far out.
        r_min = self.rs * 1.0000001
        r_max = self.rs * r_max_factor
        # geometric spacing concentrates points near the horizon
        self.r = self.rs + (r_max - self.rs) * (
            np.linspace(0, 1, n_r) ** 3
        )
        self.r = np.unique(np.concatenate(([r_min], self.r)))
        self.r = self.r[self.r >= r_min]

        f = 1.0 - self.rs / self.r
        # dr*/dr = 1/f  -> integrate cumulatively to get r*(r)
        integrand = 1.0 / f
        rstar = np.concatenate(([0.0], np.cumsum(
            0.5 * (integrand[1:] + integrand[:-1]) * np.diff(self.r)
        )))
        # shift so rstar is anchored consistently; absolute offset is
        # physically irrelevant (only differences enter the wave equation)
        self.rstar = rstar

        self.r_of_rstar = interp1d(
            self.rstar, self.r, kind="cubic",
            bounds_error=False, fill_value=(self.r[0], self.r[-1])
        )

    def potential(self, r, l):
        f = 1.0 - self.rs / r
        s = self.spin
        return f * (l * (l + 1) / r**2 + (1 - s**2) * self.rs / r**3)

    def _rhs(self, rstar, y, omega, l):
        # y = [Re(psi), Im(psi), Re(psi'), Im(psi')]
        r = float(self.r_of_rstar(rstar))
        V = self.potential(r, l)
        psi_re, psi_im, dpsi_re, dpsi_im = y
        d2psi_re = (V - omega**2) * psi_re
        d2psi_im = (V - omega**2) * psi_im
        return [dpsi_re, dpsi_im, d2psi_re, d2psi_im]

    def transmission(self, omega, l):
        """
        Shooting-method solve for the greybody factor Gamma_l(omega).

        Boundary condition at the horizon (rstar -> -infinity): purely
        ingoing wave, psi ~ exp(-i*omega*rstar), representing a mode fully
        absorbed with no reflection back out of the horizon (the physically
        correct horizon boundary condition for a classical black hole).

        We integrate this outward to large rstar and decompose the result
        into outgoing + incoming plane waves there; the incoming-wave
        coefficient C_in tells us how large an external incident wave would
        have been needed to produce unit transmitted amplitude at the
        horizon. The greybody factor is then Gamma = 1/|C_in|^2.
        """
        if omega <= 0:
            return 0.0

        rstar_min = self.rstar[0] - 40.0 / max(omega, 1e-3)
        rstar_max = self.rstar[-1]
        # ensure the horizon start is within our interpolation domain's
        # asymptotic (flat) region -- V(r) -> 0 exponentially fast near
        # the horizon in tortoise coordinates, so extending rstar_min
        # into the constant-r(rs) plateau is physically valid.
        rstar_min = max(rstar_min, self.rstar[0] - 200.0)

        psi0 = np.exp(-1j * omega * rstar_min)
        dpsi0 = -1j * omega * psi0
        y0 = [psi0.real, psi0.imag, dpsi0.real, dpsi0.imag]

        sol = solve_ivp(
            self._rhs, [rstar_min, rstar_max], y0,
            args=(omega, l), method="RK45",
            rtol=1e-8, atol=1e-10, dense_output=False,
        )

        psi = sol.y[0, -1] + 1j * sol.y[1, -1]
        dpsi = sol.y[2, -1] + 1j * sol.y[3, -1]
        rs_end = rstar_max

        phase_in = np.exp(-1j * omega * rs_end)
        phase_out = np.exp(1j * omega * rs_end)

        C_in = 0.5 * (psi - dpsi / (1j * omega)) / phase_in
        C_out = 0.5 * (psi + dpsi / (1j * omega)) / phase_out

        gamma_from_amplitude = 1.0 / np.abs(C_in) ** 2
        # flux-conservation consistency check (should closely match above)
        gamma_from_flux = 1.0 - np.abs(C_out / C_in) ** 2

        return float(np.clip(gamma_from_amplitude, 0.0, 1.0)), float(
            np.clip(gamma_from_flux, 0.0, 1.0)
        )

    def spectrum(self, omegas, l_max=4):
        """
        Returns dict with greybody factors per l, and the summed Hawking
        emission spectrum dN/(dt domega) and dE/(dt domega).

        The sum over l starts at l = spin, not l = 0, for spin >= 1 fields.
        This is not a numerical convenience -- it reflects a real physical
        constraint: a spin-s field's lowest allowed multipole is l = s.
        For the electromagnetic field (s=1) this means there is NO l=0
        (monopole) radiation channel at all, a direct consequence of gauge
        invariance (equivalently: no magnetic monopoles / charge
        conservation forbids monopole EM radiation). The lowest multipole
        that can radiate is therefore the dipole, l=1.
        """
        l_min = self.spin
        l_max = max(l_max, l_min)
        gammas = {l: np.zeros_like(omegas) for l in range(l_min, l_max + 1)}
        gammas_flux = {l: np.zeros_like(omegas) for l in range(l_min, l_max + 1)}

        for l in range(l_min, l_max + 1):
            for i, w in enumerate(omegas):
                g_amp, g_flux = self.transmission(w, l)
                gammas[l][i] = g_amp
                gammas_flux[l][i] = g_flux

        occupation = 1.0 / (np.expm1(omegas / self.T_H))  # Bose-Einstein
        dN_domega = np.zeros_like(omegas)
        dE_domega_per_l = {}
        for l in range(l_min, l_max + 1):
            contribution = (2 * l + 1) * gammas[l] * occupation / (2 * np.pi)
            dN_domega += contribution
            dE_domega_per_l[l] = omegas * contribution
        dE_domega = omegas * dN_domega

        return {
            "omegas": omegas,
            "gammas": gammas,
            "gammas_flux_check": gammas_flux,
            "dN_domega": dN_domega,
            "dE_domega": dE_domega,
            "dE_domega_per_l": dE_domega_per_l,
            "T_H": self.T_H,
            "rs": self.rs,
            "l_min": l_min,
        }

    def total_power(self, l_max=4, n_omega=60, omega_max_factor=12.0):
        """Integrate dE/(dt domega) over omega to get total emitted power."""
        omegas = np.linspace(1e-4, omega_max_factor * self.T_H, n_omega)
        result = self.spectrum(omegas, l_max=l_max)
        power = simpson(result["dE_domega"], x=omegas)
        return power, result
