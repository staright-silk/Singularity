import { useEffect, useRef } from "react";
import { animate, createTimeline, stagger } from "animejs";

/** Always show the intro. */
export function shouldShowIntro() {
  return true;
}

/**
 * Full-screen intro: fixed to prevent letter layout pops and wobbles.
 */
export default function Splash({ onDone }) {
  const rootRef = useRef(null);
  const wordRef = useRef(null);
  const barFillRef = useRef(null);
  const calledRef = useRef(false);

  const finish = () => {
    if (calledRef.current) return;
    calledRef.current = true;
    onDone();
  };

  useEffect(() => {
    const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    if (reduceMotion) {
      finish();
      return;
    }

    const word = wordRef.current;
    const text = word.textContent.trim();
    
    // Fix layout shifting/wobbling: wrap letters securely with strict display properties
    word.innerHTML = text
      .split("")
      .map((ch) => {
        if (ch === " ") return `<span class="intro-letter" style="display: inline-block;">&nbsp;</span>`;
        return `<span class="intro-letter" style="display: inline-block; opacity: 0; transform-origin: center bottom;">${ch}</span>`;
      })
      .join("");
      
    const letterEls = word.querySelectorAll(".intro-letter");
    if (barFillRef.current) barFillRef.current.style.width = "0%";

    const tl = createTimeline({
      defaults: { easing: "easeOutCubic" },
      onComplete: () => {
        animate(rootRef.current, {
          opacity: [1, 0],
          duration: 550,
          easing: "easeInOutQuad",
          onComplete: finish,
        });
      },
    });

    // Clean, direct path configuration to eliminate abrupt popping frames
    tl.add(letterEls, {
      opacity: [0, 1],
      translateY: [24, 0],
      rotate: [-3, 0],
      duration: 800,
      delay: stagger(120), // Slightly accelerated stagger for a cohesive flowing look
    }).add(
      barFillRef.current,
      { width: ["0%", "100%"], duration: 1200, easing: "easeInOutQuad" },
      "-=300"
    );

    const fallback = setTimeout(finish, 9000);

    return () => {
      clearTimeout(fallback);
      tl.pause();
    };
  }, []);

  return (
    <div className="intro-splash" ref={rootRef}>
      <div className="intro-content">
        {/* We keep the text node pristine on initial paint so no structural shift occurs */}
        <div className="intro-word" ref={wordRef} style={{ display: "block", whiteSpace: "nowrap" }}>
          Singularity
        </div>
        <div className="intro-bar">
          <div className="intro-bar-fill" ref={barFillRef} />
        </div>
        <div className="intro-caption mono">loading the model</div>
      </div>
    </div>
  );
}