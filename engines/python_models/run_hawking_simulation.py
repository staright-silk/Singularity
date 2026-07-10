"""
Run the semiclassical Hawking radiation calculation and produce:
  1. Greybody factor Gamma_l(omega) vs omega, for l = 0..4
  2. Actual emission spectrum dE/(dt domega) vs a pure blackbody at T_H,
     showing the physically real greybody suppression at low omega
  3. Total emitted power vs black hole mass M, checked against the
     theoretically expected P ~ 1/M^2 scaling law (Hawking's original
     dimensional-analysis result, here reproduced from a first-principles
     numerical calculation rather than assumed)
  4. CSV export of all numerical results for the ISEF data record
"""
import os
import csv
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from hawking_greybody import SchwarzschildGreybody

OUTDIR = "hawking_results"
os.makedirs(OUTDIR, exist_ok=True)


def run_spectrum_for_mass(M, l_max=4, n_omega=45, omega_max_factor=10.0):
    gb = SchwarzschildGreybody(M=M, n_r=6000, r_max_factor=60)
    omegas = np.linspace(1e-3, omega_max_factor * gb.T_H, n_omega)
    result = gb.spectrum(omegas, l_max=l_max)
    return gb, result


def plot_greybody_factors(gb, result, path):
    fig, ax = plt.subplots(figsize=(7, 5))
    omegas = result["omegas"]
    for l, gamma in result["gammas"].items():
        ax.plot(omegas / gb.T_H, gamma, label=f"l = {l}")
    ax.set_xlabel(r"$\omega / T_H$")
    ax.set_ylabel(r"Greybody factor $\Gamma_l(\omega)$")
    ax.set_title(f"Numerically computed greybody factors (M = {gb.M}, $r_s$ = {gb.rs})")
    ax.legend()
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_spectrum_vs_blackbody(gb, result, path):
    fig, ax = plt.subplots(figsize=(7, 5))
    omegas = result["omegas"]
    dE = result["dE_domega"]

    # ideal blackbody comparison (Gamma = 1 for all modes, l=0 only, for scale)
    T_H = gb.T_H
    blackbody = (1.0 / (2 * np.pi)) * omegas**2 / np.expm1(omegas / T_H)

    ax.plot(omegas / T_H, dE, "r-", lw=2, label="Actual Hawking spectrum (greybody-corrected)")
    ax.plot(omegas / T_H, blackbody, "k--", lw=1.5, alpha=0.7, label="Ideal l=0 blackbody (Gamma=1)")
    ax.set_xlabel(r"$\omega / T_H$")
    ax.set_ylabel(r"$dE / (dt\, d\omega)$")
    ax.set_title(f"Hawking emission spectrum vs. ideal blackbody (M = {gb.M})")
    ax.legend()
    ax.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def mass_scaling_study(masses, l_max=3, n_omega=30):
    rows = []
    for M in masses:
        gb = SchwarzschildGreybody(M=M, n_r=5000, r_max_factor=50)
        power, result = gb.total_power(l_max=l_max, n_omega=n_omega)
        rows.append({"M": M, "rs": gb.rs, "T_H": gb.T_H, "total_power": power})
        print(f"M={M:.2f}  rs={gb.rs:.3f}  T_H={gb.T_H:.6f}  P={power:.6e}")
    return rows


def plot_mass_scaling(rows, path):
    M = np.array([r["M"] for r in rows])
    P = np.array([r["total_power"] for r in rows])

    fig, ax = plt.subplots(figsize=(7, 5))
    ax.loglog(M, P, "bo-", label="Computed total power")

    # fit P = A * M^n on log-log data
    coeffs = np.polyfit(np.log(M), np.log(P), 1)
    n_fit, logA = coeffs
    fit_line = np.exp(logA) * M**n_fit
    ax.loglog(M, fit_line, "r--",
               label=f"Power-law fit: P $\\propto M^{{{n_fit:.2f}}}$")

    ax.set_xlabel("Black hole mass M")
    ax.set_ylabel("Total emitted power")
    ax.set_title("Hawking luminosity vs. mass (theory predicts P $\\propto M^{-2}$)")
    ax.legend()
    ax.grid(alpha=0.3, which="both")
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)
    return n_fit


def export_spectrum_csv(gb, result, path):
    with open(path, "w", newline="") as f:
        writer = csv.writer(f)
        header = ["omega", "omega_over_TH", "dN_domega", "dE_domega"] + \
                 [f"Gamma_l{l}" for l in result["gammas"]]
        writer.writerow(header)
        omegas = result["omegas"]
        for i, w in enumerate(omegas):
            row = [w, w / gb.T_H, result["dN_domega"][i], result["dE_domega"][i]] + \
                  [result["gammas"][l][i] for l in result["gammas"]]
            writer.writerow(row)


def export_mass_scaling_csv(rows, path):
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["M", "rs", "T_H", "total_power"])
        writer.writeheader()
        for r in rows:
            writer.writerow(r)


def main():
    print("=== Baseline run: M = 1.0 ===")
    gb, result = run_spectrum_for_mass(M=1.0, l_max=4, n_omega=45)
    plot_greybody_factors(gb, result, os.path.join(OUTDIR, "greybody_factors.png"))
    plot_spectrum_vs_blackbody(gb, result, os.path.join(OUTDIR, "spectrum_vs_blackbody.png"))
    export_spectrum_csv(gb, result, os.path.join(OUTDIR, "baseline_spectrum.csv"))

    print("\n=== Mass scaling study (validates P ~ 1/M^2) ===")
    masses = [0.5, 0.75, 1.0, 1.5, 2.0, 3.0]
    rows = mass_scaling_study(masses, l_max=3, n_omega=30)
    n_fit = plot_mass_scaling(rows, os.path.join(OUTDIR, "mass_scaling.png"))
    export_mass_scaling_csv(rows, os.path.join(OUTDIR, "mass_scaling.csv"))

    print(f"\nFitted power-law exponent: P ~ M^{n_fit:.3f}  (theory: -2.000)")
    print(f"Deviation from theory: {abs(n_fit - (-2.0)):.3f}")

    print(f"\nAll results saved to ./{OUTDIR}/")


if __name__ == "__main__":
    main()
