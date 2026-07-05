import { Link } from "react-router-dom";
import Backdrop from "../components/Backdrop.jsx";
import SiteNav from "../components/SiteNav.jsx";
import SiteFooter from "../components/SiteFooter.jsx";
import Reveal from "../components/Reveal.jsx";

export default function Home() {
  return (
    <div className="site-page">
      <Backdrop intensity={1.0} />
      <SiteNav />

      <main>
        <div className="page-hero">
          <div className="eyebrow mono">A black-hole model, kept visible</div>
          <h1 className="page-title">
            A wave packet
            <br />
            <em>near a horizon</em>
          </h1>
          <p className="page-sub">
            This project started with a plain question: what happens when a packet of probability moves toward a
            horizon? The answer is a compact numerical model that keeps the steps visible instead of hiding them
            behind a polished animation.
          </p>
          <div className="hud glass">
            <div className="hud-item">
              <div className="hud-label mono">State</div>
              <div className="hud-value mono">psi(r, t)</div>
            </div>
            <div className="hud-item">
              <div className="hud-label mono">Density</div>
              <div className="hud-value mono">|psi|²</div>
            </div>
            <div className="hud-item">
              <div className="hud-label mono">Method</div>
              <div className="hud-value mono">FFT split-operator</div>
            </div>
            <div className="hud-item">
              <div className="hud-label mono">Source</div>
              <div className="hud-value mono">Hawking-like noise</div>
            </div>
          </div>
        </div>

        <section>
          <div className="section-inner">
            <Reveal className="section-head">
              <div className="section-eyebrow mono">The parts</div>
              <h2 className="section-title">The model is simple enough to describe.</h2>
              <p className="section-desc">
                Each page focuses on one piece of the setup: the packet, the grid, the solver step, and the readout
                that comes back from it.
              </p>
            </Reveal>
            <div className="grid-2">
              <Reveal as={Link} to="/simulation" className="card glass">
                <span className="card-index mono">01 / solver</span>
                <h3 className="card-name">The wave packet</h3>
                <p className="card-desc">
                  A normalized Gaussian begins on a radial grid just outside the horizon. The solver follows it as it
                  interacts with a Schwarzschild-like potential.
                </p>
                <div className="tags">
                  <span className="tag">Python</span>
                  <span className="tag">FFT</span>
                </div>
              </Reveal>
              <Reveal as={Link} to="/simulation" className="card glass">
                <span className="card-index mono">02 / engines</span>
                <h3 className="card-name">The code</h3>
                <p className="card-desc">
                  The repository has two ways to look at the same idea: a live web version and a Python version for
                  more direct experiments.
                </p>
                <div className="tags">
                  <span className="tag">Wave packet</span>
                  <span className="tag">Schwarzschild</span>
                </div>
              </Reveal>
              <Reveal as={Link} to="/simulation" className="card glass">
                <span className="card-index mono">03 / dashboard</span>
                <h3 className="card-name">The readout</h3>
                <p className="card-desc">
                  The browser receives state updates and plots the probability curve, the absorbed part, and the
                  near-horizon source as the calculation goes forward.
                </p>
                <div className="tags">
                  <span className="tag">React</span>
                  <span className="tag">FastAPI</span>
                </div>
              </Reveal>
              <Reveal as={Link} to="/model" className="card glass">
                <span className="card-index mono">04 / object</span>
                <h3 className="card-name">The object</h3>
                <p className="card-desc">
                  The physical model takes the same geometry and turns it into something you can place on a table and
                  look at for a minute.
                </p>
                <div className="tags">
                  <span className="tag">PLA/PETG</span>
                  <span className="tag">ESP32</span>
                </div>
              </Reveal>
            </div>
          </div>
        </section>

        <section>
          <Reveal className="section-inner" as="div">
            <p className="pull">
              The point is not to look spectacular. The point is to make the assumptions legible.
            </p>
          </Reveal>
        </section>
      </main>

      <SiteFooter />
    </div>
  );
}
