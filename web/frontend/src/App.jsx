import { useState } from "react";
import { Routes, Route } from "react-router-dom";
import Home from "./pages/Home.jsx";
import Simulation from "./pages/Simulation.jsx";
import Model from "./pages/Model.jsx";
import About from "./pages/About.jsx";
import Splash, { shouldShowIntro } from "./components/Splash.jsx";

export default function App() {
  const [showIntro, setShowIntro] = useState(shouldShowIntro());

  return (
    <>
      {/* The real page (with its own live black-hole backdrop) mounts and
          starts rendering immediately, underneath the splash overlay. When
          the splash fades its black overlay away, it reveals this exact
          already-running backdrop — a true crossfade, not a cut to a
          second, separately-started black hole. */}
      <Routes>
        <Route path="/" element={<Home />} />
        <Route path="/simulation" element={<Simulation />} />
        <Route path="/model" element={<Model />} />
        <Route path="/about" element={<About />} />
      </Routes>

      {showIntro && <Splash onDone={() => setShowIntro(false)} />}
    </>
  );
}
