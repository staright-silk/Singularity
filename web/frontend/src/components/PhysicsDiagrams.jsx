/**
 * Pure SVG + CSS animated diagrams explaining the actual mechanics of the
 * solver (no canvas/WebGL, no animation library — CSS keyframes loop
 * indefinitely and respect prefers-reduced-motion). These are meant to be
 * read alongside the equations, not decoration.
 */

export function WavePacketDiagram() {
  return (
    <svg
      className="phys-diagram"
      viewBox="0 0 640 220"
      role="img"
      aria-label="A wave packet traveling along the radial axis toward the horizon, shrinking as it is absorbed, shown against the effective potential curve."
    >
      {/* axis */}
      <line x1="40" y1="170" x2="600" y2="170" className="phys-axis" />
      <text x="596" y="192" className="phys-label" textAnchor="end">r  (radial coordinate)</text>

      {/* effective potential V(r): steep near the horizon, a bump, then falling off */}
      <path
        className="phys-potential"
        d="M 118 168
           C 140 120, 150 55, 175 55
           C 210 55, 240 95, 300 130
           C 380 155, 460 165, 560 168"
        fill="none"
      />
      <text x="230" y="45" className="phys-label">V(r) — effective potential</text>

      {/* horizon marker */}
      <line x1="112" y1="40" x2="112" y2="170" className="phys-horizon-line" />
      <circle cx="112" cy="170" r="6" className="phys-horizon-dot" />
      <text x="112" y="200" className="phys-label" textAnchor="middle">r_s</text>

      {/* traveling wave packet (Gaussian bump), animated via CSS */}
      <g className="phys-packet-travel">
        <path
          className="phys-packet"
          d="M 470 170
             C 470 170, 490 92, 520 92
             C 550 92, 570 170, 570 170
             Z"
        />
      </g>
      <text x="520" y="212" className="phys-label" textAnchor="middle">|ψ(r,t)|² — probability density</text>
    </svg>
  );
}

export function SplitOperatorDiagram() {
  return (
    <svg
      className="phys-diagram"
      viewBox="0 0 640 160"
      role="img"
      aria-label="The split-operator time step: a half potential step, a full kinetic step done in Fourier space, then another half potential step, repeating."
    >
      <g className="phys-step phys-step-1">
        <rect x="20" y="40" width="170" height="80" rx="10" />
        <text x="105" y="72" className="phys-label phys-label-strong" textAnchor="middle">Û_V (half step)</text>
        <text x="105" y="94" className="phys-label" textAnchor="middle">exp(−iV_eff·dt/2)</text>
      </g>

      <path d="M 195 80 L 235 80" className="phys-arrow" markerEnd="url(#physArrow)" />

      <g className="phys-step phys-step-2">
        <rect x="240" y="40" width="170" height="80" rx="10" />
        <text x="325" y="72" className="phys-label phys-label-strong" textAnchor="middle">Û_K (kinetic, FFT)</text>
        <text x="325" y="94" className="phys-label" textAnchor="middle">exp(−ik²·dt/2)</text>
      </g>

      <path d="M 415 80 L 455 80" className="phys-arrow" markerEnd="url(#physArrow)" />

      <g className="phys-step phys-step-3">
        <rect x="460" y="40" width="170" height="80" rx="10" />
        <text x="545" y="72" className="phys-label phys-label-strong" textAnchor="middle">Û_V (half step)</text>
        <text x="545" y="94" className="phys-label" textAnchor="middle">exp(−iV_eff·dt/2)</text>
      </g>

      <defs>
        <marker id="physArrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto">
          <path d="M0 0 L10 5 L0 10 Z" className="phys-arrowhead" />
        </marker>
      </defs>
    </svg>
  );
}
