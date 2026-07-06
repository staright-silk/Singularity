import { useEffect, useRef } from "react";
import { animate, createTimeline, stagger } from "animejs";

/** Always show the intro. (Previously session-locked via sessionStorage —
 * removed so it plays on every load/refresh.) */
export function shouldShowIntro() {
  return true;
}

/**
 * Full-screen intro: the black-hole backdrop dimmed behind a hand-lettered
 * "Singularity" wordmark that writes itself on, followed by a loading bar,
 * then fades out to reveal the actual site. Calls onDone() exactly once,
 * either after the animation finishes or immediately if the browser prefers
 * reduced motion.
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
    const text = word.textContent;
    word.innerHTML = text
      .split("")
      .map((ch) => `<span class="intro-letter">${ch === " " ? "&nbsp;" : ch}</span>`)
      .join("");
    const letterEls = word.querySelectorAll(".intro-letter");
    letterEls.forEach((el) => { el.style.opacity = "0"; });
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

    tl.add(letterEls, {
      opacity: [0, 1],
      translateY: [18, 0],
      rotate: [-6, 0],
      duration: 480,
      delay: stagger(65),
    }).add(
      barFillRef.current,
      { width: ["0%", "100%"], duration: 850, easing: "easeInOutQuad" },
      "-=150"
    );

    // Safety net: if anything about the animation library misbehaves,
    // never trap the user on the splash screen forever.
    const fallback = setTimeout(finish, 6000);

    return () => {
      clearTimeout(fallback);
      tl.pause();
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return (
    <div className="intro-splash" ref={rootRef}>
      <div className="intro-content">
        <div className="intro-word" ref={wordRef}>
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
