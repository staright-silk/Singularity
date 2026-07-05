import Backdrop from "../components/Backdrop.jsx";
import SiteNav from "../components/SiteNav.jsx";
import SiteFooter from "../components/SiteFooter.jsx";
import Reveal from "../components/Reveal.jsx";
import Dashboard from "../components/Dashboard.jsx";

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
            The page below shows the same calculation from a few angles: the packet, the solver step, and the live
            readout. It is meant to be clear rather than dramatic.
          </p>
        </div>

        <section>
          <div className="section-inner">
            <Reveal className="section-head">
              <div className="section-eyebrow mono">01 — wavefunction</div>
              <h2 className="section-title">The state is a wavefunction.</h2>
              <p className="section-desc">
                The solver begins with a normalized Gaussian packet and moves it through a radial grid just outside
                the horizon. The line you see is the probability density, and the values beside it tell you what is
                still present and what has been absorbed.
              </p>
            </Reveal>
            <div className="grid-2">
              <Reveal className="card glass">
                <span className="card-index mono">state</span>
                <h3 className="card-name">The packet</h3>
                <p className="card-desc">
                  The packet moves through the radial grid while the Schwarzschild radius shapes the environment and
                  the potential does the scattering.
                </p>
              </Reveal>
              <Reveal className="card glass">
                <span className="card-index mono">diagnostics</span>
                <h3 className="card-name">What remains</h3>
                <p className="card-desc">
                  The live readout tracks what stays in the domain, what is absorbed, and the signal that is being
                  emitted near the horizon.
                </p>
              </Reveal>
            </div>
          </div>
        </section>

        <section>
          <div className="section-inner">
            <Reveal className="section-head">
              <div className="section-eyebrow mono">02 — numerical method</div>
              <h2 className="section-title">The step is simple enough to follow.</h2>
              <p className="section-desc">
                Each update is a half potential step, a full kinetic step in Fourier space, and another half
                potential step. An absorbing layer removes the part that would otherwise reflect back into the
                domain.
              </p>
            </Reveal>
            <div className="grid-2">
              <Reveal className="card glass">
                <span className="card-index mono">step</span>
                <h3 className="card-name">Half / full / half</h3>
                <p className="card-desc">
                  The method alternates kinetic and potential updates in a symmetric sequence, which keeps the
                  evolution compact and easy to reason about.
                </p>
              </Reveal>
              <Reveal className="card glass">
                <span className="card-index mono">absorption</span>
                <h3 className="card-name">Absorbing edges</h3>
                <p className="card-desc">
                  The absorbed probability is estimated every step and appears in the UI, making the boundary behavior
                  visible rather than hidden.
                </p>
              </Reveal>
            </div>
          </div>
        </section>

        <section>
          <div className="section-inner">
            <Reveal className="section-head">
              <div className="section-eyebrow mono">03 — instrument panel</div>
              <h2 className="section-title">The dashboard reads the running model.</h2>
              <p className="section-desc">
                The panel below is connected to the backend. It shows the retained density, the near-horizon signal,
                the effective potential, and the horizon marker as the simulation evolves.
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
