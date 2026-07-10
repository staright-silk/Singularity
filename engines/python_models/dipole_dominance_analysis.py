"""
Electromagnetic (Photon) Hawking Radiation and Dipole Dominance
==================================================================
A massless scalar field's Hawking spectrum (spin s=0) is dominated by the
monopole (l=0) channel. The electromagnetic field (spin s=1) has NO l=0
channel at all -- this is a direct, rigorous consequence of gauge
invariance (equivalently: there are no magnetic monopoles, so a photon
cannot be emitted with zero angular momentum). Its lowest allowed
radiation channel is therefore the electric DIPOLE, l=1.

This script computes both spectra from the same underlying numerical
machinery (only the spin parameter changes) and quantifies what fraction
of a black hole's photon luminosity is carried by the dipole channel
specifically -- a concrete, checkable link between electric multipole
radiation theory and Hawking's black hole radiation.
"""
import os
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from scipy.integrate import simpson

from hawking_greybody import SchwarzschildGreybody

OUTDIR = "hawking_results"
os.makedirs(OUTDIR, exist_ok=True)


def compute_field(spin, M=1.0, l_max=4, n_omega=45, omega_max_factor=10.0):
    gb = SchwarzschildGreybody(M=M, n_r=6000, r_max_factor=60, spin=spin)
    omegas = np.linspace(1e-3, omega_max_factor * gb.T_H, n_omega)
    result = gb.spectrum(omegas, l_max=l_max)
    return gb, result


def dipole_fraction(result):
    """Fraction of total EM power carried by the l=1 (dipole) channel."""
    omegas = result["omegas"]
    per_l_power = {
        l: simpson(dE, x=omegas) for l, dE in result["dE_domega_per_l"].items()
    }
    total = sum(per_l_power.values())
    return per_l_power, total


def main():
    print("=== Scalar field (s=0): monopole-allowed ===")
    gb0, res0 = compute_field(spin=0)
    power0_per_l, power0_total = dipole_fraction(res0)
    for l, p in power0_per_l.items():
        print(f"  l={l}: power={p:.6e}  ({100*p/power0_total:.2f}% of total)")

    print("\n=== Electromagnetic field (s=1): NO monopole channel ===")
    gb1, res1 = compute_field(spin=1)
    power1_per_l, power1_total = dipole_fraction(res1)
    for l, p in power1_per_l.items():
        print(f"  l={l}: power={p:.6e}  ({100*p/power1_total:.2f}% of total)")

    dipole_pct = 100 * power1_per_l[1] / power1_total
    print(f"\n==> Dipole (l=1) channel carries {dipole_pct:.2f}% of the black "
          f"hole's total photon (electromagnetic) luminosity.")
    print("    (l=0 is physically forbidden for the electromagnetic field "
          "-- gauge invariance / absence of magnetic monopoles.)")

    # --- Plot: multipole breakdown, scalar vs EM ---
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))

    ls0 = sorted(power0_per_l.keys())
    axes[0].bar([str(l) for l in ls0],
                [100 * power0_per_l[l] / power0_total for l in ls0],
                color="steelblue")
    axes[0].set_title("Scalar field (s=0)\nmultipole breakdown")
    axes[0].set_xlabel("l (multipole order)")
    axes[0].set_ylabel("% of total emitted power")
    axes[0].grid(alpha=0.3, axis="y")

    ls1 = sorted(power1_per_l.keys())
    colors = ["darkorange" if l == 1 else "gray" for l in ls1]
    axes[1].bar([str(l) for l in ls1],
                [100 * power1_per_l[l] / power1_total for l in ls1],
                color=colors)
    axes[1].set_title("Electromagnetic field (s=1)\nmultipole breakdown"
                       "\n(l=0 forbidden by gauge invariance)")
    axes[1].set_xlabel("l (multipole order)")
    axes[1].set_ylabel("% of total emitted power")
    axes[1].grid(alpha=0.3, axis="y")

    fig.suptitle(f"Hawking radiation multipole content: dipole channel carries "
                 f"{dipole_pct:.1f}% of EM luminosity", fontsize=12)
    fig.tight_layout()
    fig.savefig(os.path.join(OUTDIR, "dipole_dominance.png"), dpi=150)
    plt.close(fig)

    # --- Plot: EM spectrum shape (dipole-dominated) vs scalar spectrum ---
    fig, ax = plt.subplots(figsize=(7, 5))
    ax.plot(res0["omegas"] / gb0.T_H, res0["dE_domega"], "b-", lw=2,
            label="Scalar field (monopole-allowed)")
    ax.plot(res1["omegas"] / gb1.T_H, res1["dE_domega"], "r-", lw=2,
            label="Electromagnetic field (dipole-dominated)")
    ax.set_xlabel(r"$\omega / T_H$")
    ax.set_ylabel(r"$dE/(dt\,d\omega)$")
    ax.set_title("Hawking spectra: scalar vs. electromagnetic field")
    ax.legend()
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(OUTDIR, "scalar_vs_em_spectrum.png"), dpi=150)
    plt.close(fig)

    print(f"\nPlots saved to ./{OUTDIR}/")


if __name__ == "__main__":
    main()
