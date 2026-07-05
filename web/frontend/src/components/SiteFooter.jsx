import { Link } from "react-router-dom";

export default function SiteFooter() {
  return (
    <footer className="site-footer">
      <div className="site-footer-links">
        <Link to="/">Home</Link>
        <Link to="/simulation">Simulation</Link>
        <Link to="/model">Physical Model</Link>
        <Link to="/about">About</Link>
        <a href="https://github.com/staright-silk/Project-Singularity" target="_blank" rel="noopener noreferrer">
          GitHub
        </a>
      </div>
      <div className="site-footer-note mono">Project Singularity — a small model of a black hole, made visible.</div>
    </footer>
  );
}
