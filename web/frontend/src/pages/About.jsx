import Backdrop from "../components/Backdrop.jsx";
import SiteNav from "../components/SiteNav.jsx";
import SiteFooter from "../components/SiteFooter.jsx";
import Reveal from "../components/Reveal.jsx";

const TIMELINE = [
  {
    phase: "Phase 1",
    title: "One wave packet",
    desc: "The project begins with a simple packet on a radial grid and a question about what happens when it approaches the horizon.",
  },
  {
    phase: "Phase 2",
    title: "A horizon-shaped potential",
    desc: "The solver moves that packet through a Schwarzschild-like potential so the geometry matters to the evolution.",
  },
  {
    phase: "Phase 3",
    title: "A source near the edge",
    desc: "A small stochastic term is added near the horizon so the model can show emission without pretending to be a complete theory.",
  },
  {
    phase: "Phase 4",
    title: "A live readout in the browser",
    desc: "The backend sends state updates to the page, where the probability curve and the near-horizon signal can be seen as they change.",
  },
  {
    phase: "Phase 5",
    title: "A simple object for a table",
    desc: "The physical piece keeps the same geometry in a form that can be touched, lit, and explained without much ceremony.",
  },
];

export default function About() {
  return (
    <div className="site-page">
      <Backdrop intensity={0.6} />
      <SiteNav />

      <main>
        <div className="page-hero">
          <div className="eyebrow mono">Origin & scope</div>
          <h1 className="page-title">
            A project built
            <br />
            <em>from one question.</em>
          </h1>
          <p className="page-sub">
            What if a black-hole model could be described without turning it into a spectacle? The code, the plots,
            and the object all stay close to the same small setup.
          </p>
        </div>

        <section>
          <div className="section-inner">
            <Reveal className="section-head">
              <div className="section-eyebrow mono">Timeline</div>
              <h2 className="section-title">How the pieces came together.</h2>
            </Reveal>
            <Reveal className="timeline">
              {TIMELINE.map((item) => (
                <div className="tl-item" key={item.phase}>
                  <div className="tl-date mono">{item.phase}</div>
                  <div>
                    <div className="tl-title">{item.title}</div>
                    <div className="tl-desc">{item.desc}</div>
                  </div>
                </div>
              ))}
            </Reveal>
          </div>
        </section>

        <section>
          <div className="section-inner">
            <Reveal className="section-head">
              <div className="section-eyebrow mono">Honesty over polish</div>
              <h2 className="section-title">What the model is, and what it is not.</h2>
              <p className="section-desc">
                The repository does not pretend to be a complete theory. It keeps the terms visible: a packet, a
                potential, a boundary, a source, and a signal.
              </p>
            </Reveal>
          </div>
        </section>
      </main>

      <SiteFooter />
    </div>
  );
}
