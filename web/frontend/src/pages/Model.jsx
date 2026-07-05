import Backdrop from "../components/Backdrop.jsx";
import SiteNav from "../components/SiteNav.jsx";
import SiteFooter from "../components/SiteFooter.jsx";
import Reveal from "../components/Reveal.jsx";

export default function Model() {
  return (
    <div className="site-page">
      <Backdrop intensity={0.6} />
      <SiteNav />

      <main>
        <div className="page-hero">
          <div className="eyebrow mono">Held in the hand</div>
          <h1 className="page-title">
            A dark sphere,
            <br />
            <em>a ring of light.</em>
          </h1>
          <p className="page-sub">
            This object is meant to feel like a sketch of the solver rather than a fantasy version of it.
          </p>
        </div>

        <section>
          <div className="section-inner model-layout">
            <Reveal className="model-figure">
              <div className="core" />
            </Reveal>
            <Reveal>
              <div className="section-eyebrow mono">Construction</div>
              <h2 className="section-title">A horizon, a ring, and a light source.</h2>
              <p className="section-desc">
                The disc and the sphere are printed separately so each one can do its job clearly. The sphere marks
                the horizon; the ring carries the surrounding light.
              </p>
              <ul className="model-list">
                <li>
                  <span className="dot">→</span>A black sphere marks the horizon so the center of the model feels
                  like a boundary rather than an ornament.
                </li>
                <li>
                  <span className="dot">→</span>A translucent ring suggests the space outside the horizon without
                  pretending to be the whole geometry.
                </li>
                <li>
                  <span className="dot">→</span>Addressable LEDs give the object a faint emission field instead of a
                  decorative glow.
                </li>
                <li>
                  <span className="dot">→</span>It is meant to sit on a table and make the numerical setup easier to
                  talk about.
                </li>
              </ul>
            </Reveal>
          </div>
        </section>

        <section>
          <div className="section-inner">
            <Reveal className="section-head">
              <div className="section-eyebrow mono">Bill of materials</div>
              <h2 className="section-title">What actually makes it work.</h2>
            </Reveal>
            <div className="grid-2">
              <Reveal className="card glass">
                <span className="card-index mono">printing</span>
                <h3 className="card-name">Printed parts</h3>
                <p className="card-desc">
                  Matte black PLA or PETG for the sphere, and translucent or clear filament for the ring. FDM is
                  enough for the geometry described in the project notes.
                </p>
                <div className="tags">
                  <span className="tag">PLA</span>
                  <span className="tag">PETG</span>
                  <span className="tag">FDM</span>
                </div>
              </Reveal>
              <Reveal className="card glass">
                <span className="card-index mono">structure</span>
                <h3 className="card-name">Base & mounts</h3>
                <p className="card-desc">
                  An MDF or acrylic plate and simple standoffs keep the ring clear of the sphere while leaving the
                  geometry readable from the side.
                </p>
                <div className="tags">
                  <span className="tag">MDF</span>
                  <span className="tag">Acrylic</span>
                </div>
              </Reveal>
              <Reveal className="card glass">
                <span className="card-index mono">lighting</span>
                <h3 className="card-name">Electronics</h3>
                <p className="card-desc">
                  WS2812B addressable LEDs and an ESP32 or Arduino controller provide a light source for the
                  emission region without turning it into a decorative lamp.
                </p>
                <div className="tags">
                  <span className="tag">WS2812B</span>
                  <span className="tag">ESP32</span>
                </div>
              </Reveal>
              <Reveal className="card glass">
                <span className="card-index mono">finishing</span>
                <h3 className="card-name">Finish & tools</h3>
                <p className="card-desc">
                  Matte black spray for the sphere and a clear coat for the ring are enough to keep it visually
                  direct, with a soldering iron and multimeter for the wiring.
                </p>
                <div className="tags">
                  <span className="tag">Spray paint</span>
                  <span className="tag">Soldering</span>
                </div>
              </Reveal>
            </div>
          </div>
        </section>

        <section>
          <Reveal className="section-inner">
            <p className="pull">
              It is there to remind you that the math has a body.
            </p>
          </Reveal>
        </section>
      </main>

      <SiteFooter />
    </div>
  );
}
