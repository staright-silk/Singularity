import Backdrop from "../components/Backdrop.jsx";
import SiteNav from "../components/SiteNav.jsx";
import SiteFooter from "../components/SiteFooter.jsx";
import Reveal from "../components/Reveal.jsx";
import Dashboard from "../components/Dashboard.jsx";
import { WavePacketDiagram, SplitOperatorDiagram } from "../components/PhysicsDiagrams.jsx";

export default function Simulation() {
  return (
    <div className="site-page">
      <Backdrop intensity={0.7} />
      <SiteNav />

      <main>
        <div className="page-hero">
          <div className="eyebrow mono">Solver · dashboard · process</div>
          <h1 className="page-title">
            A graph that is
            <br />
            <em>trying to be honest.</em>
          </h1>
          <p className="page-sub">
            Everything below is the actual math running in the solver — the initial state, the potential it moves
            through, the numerical method that evolves it, and the stochastic source near the horizon.
          </p>
        </div>

        <section>
          <div className="section-inner">
            <Reveal className="section-head">
              <div className="section-eyebrow mono">01 — the initial state</div>
              <h2 className="section-title">A Gaussian packet, given momentum.</h2>
              <p className="section-desc">
                The solver starts with a normalized Gaussian centered just outside the horizon, multiplied by a plane
                wave so it carries inward momentum:
              </p>
            </Reveal>
            <Reveal className="eqn-block mono">
              ψ(r, 0) = exp( −(r − r₀)² / 4σ² ) · exp( i·p₀·r ) &nbsp;&nbsp;→&nbsp;&nbsp; normalized so ∫|ψ|² dr = 1
            </Reveal>
            <Reveal className="grid-2">
              <div className="card glass">
                <span className="card-index mono">r₀, σ</span>
                <h3 className="card-name">Where it starts</h3>
                <p className="card-desc">
                  r₀ sets the packet's starting distance from the horizon, σ sets how spread out it is. A tighter
                  packet is more localized in position but — by the uncertainty relation — less localized in
                  momentum.
                </p>
              </div>
              <div className="card glass">
                <span className="card-index mono">p₀</span>
                <h3 className="card-name">The momentum it carries</h3>
                <p className="card-desc">
                  The exp(i·p₀·r) factor is what makes the packet move at all — without it the Gaussian would just
                  sit still and spread symmetrically instead of drifting toward the horizon.
                </p>
              </div>
            </Reveal>
            <WavePacketDiagram />
          </div>
        </section>

        <section>
          <div className="section-inner">
            <Reveal className="section-head">
              <div className="section-eyebrow mono">02 — the effective potential</div>
              <h2 className="section-title">The geometry the packet has to move through.</h2>
              <p className="section-desc">
                Near a Schwarzschild black hole, radial motion sees an effective potential combining the usual
                centrifugal barrier with a curvature correction that only matters close to the horizon:
              </p>
            </Reveal>
            <Reveal className="eqn-block mono">
              V(r) = ( 1 − r_s/r ) · ( l(l+1)/r² + r_s/r³ )
            </Reveal>
            <Reveal className="section-desc" as="p">
              The (1 − r_s/r) factor is the redshift term — it's what forces V to vanish exactly at the horizon
              instead of blowing up. The domain edges aren't physical walls, so a complex absorbing layer −iW(r) is
              added on top of V near the horizon and near the outer boundary, damping the wavefunction there so it
              leaves the grid instead of reflecting back and corrupting the result.
            </Reveal>
          </div>
        </section>

        <section>
          <div className="section-inner">
            <Reveal className="section-head">
              <div className="section-eyebrow mono">03 — numerical method</div>
              <h2 className="section-title">Split-operator, step by step.</h2>
              <p className="section-desc">
                The Schrödinger equation gets evolved with a symmetric (Strang) splitting: half a potential step,
                one full kinetic step done in Fourier space via FFT, then another half potential step. This keeps
                the method second-order accurate without ever needing to diagonalize the full Hamiltonian.
              </p>
            </Reveal>
            <SplitOperatorDiagram />
            <Reveal className="grid-2">
              <div className="card glass">
                <span className="card-index mono">Û_V</span>
                <h3 className="card-name">Potential half-step</h3>
                <p className="card-desc">
                  Applied directly in position space: Û_V = exp(−i·V_eff·dt/2). Cheap, since V_eff(r) is already
                  known at every grid point.
                </p>
              </div>
              <div className="card glass">
                <span className="card-index mono">Û_K</span>
                <h3 className="card-name">Kinetic full step</h3>
                <p className="card-desc">
                  Applied in momentum space: Û_K = exp(−i·k²·dt/2), via FFT → multiply → inverse FFT. Momentum space
                  is where the kinetic operator is diagonal, which is the entire reason for splitting the step.
                </p>
              </div>
            </Reveal>
          </div>
        </section>

        <section>
          <div className="section-inner">
            <Reveal className="section-head">
              <div className="section-eyebrow mono">04 — the horizon source</div>
              <h2 className="section-title">A Hawking-like stand-in, not a full derivation.</h2>
              <p className="section-desc">
                Each step, a small stochastic term is injected near the horizon — a random-phase Gaussian burst,
                scaled by a noise amplitude:
              </p>
            </Reveal>
            <Reveal className="eqn-block mono">
              noise(r) = √T_H · e^(iθ) · exp( −(r − (r_s + 0.8))² / 2w² ),&nbsp;&nbsp; θ ~ Uniform(0, 2π)
            </Reveal>
            <Reveal className="section-desc" as="p">
              This is deliberately not presented as a rigorous derivation of Hawking radiation — that would require
              full quantum field theory in curved spacetime, not a single-particle Schrödinger solver. What it does
              give honestly: a visible, randomized signal emerging near the horizon whose statistics can be
              inspected, compared against the absorbed probability, and used to talk about the idea of horizon
              emission without overstating what a toy model can prove.
            </Reveal>
          </div>
        </section>

        <section>
          <div className="section-inner">
            <Reveal className="section-head">
              <div className="section-eyebrow mono">05 — instrument panel</div>
              <h2 className="section-title">The dashboard reads the running model.</h2>
              <p className="section-desc">
                The panel below is connected to the backend. It shows the retained density, the near-horizon signal,
                the effective potential, and the horizon marker as the simulation evolves in real time.
              </p>
            </Reveal>
            <Reveal className="dashboard-embed glass">
              <Dashboard />
            </Reveal>
          </div>
        </section>
      </main>

      <SiteFooter />
    </div>
  );
}
