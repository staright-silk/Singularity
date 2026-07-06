import { useCallback, useEffect, useRef, useState } from "react";

const API_BASE = (import.meta.env.VITE_API_URL || `http://${window.location.hostname}:8000`).replace(/\/+$/, "");
const POLL_MS = 1000;

/**
 * Polls the Project Singularity REST backend and exposes simulation state,
 * params, and history, plus control actions.
 */
export function useSimulationSocket() {
  const [status, setStatus] = useState("connecting"); // connecting | connected | disconnected
  const [state, setState] = useState(null);
  const [params, setParams] = useState(null);
  const [history, setHistory] = useState({ times: [], probs: [], flux: [] });

  const mounted = useRef(true);
  const pollTimer = useRef(null);
  const inFlight = useRef(false);

  const request = useCallback(async (path, options = {}) => {
    const response = await fetch(`${API_BASE}${path}`, options);
    if (!response.ok) {
      throw new Error(`Request failed: ${path} (${response.status})`);
    }
    return response.json();
  }, []);

  const refresh = useCallback(async () => {
    if (!mounted.current || inFlight.current) return;
    inFlight.current = true;
    try {
      const [nextState, nextParams, nextHistory] = await Promise.all([
        request("/api/state"),
        request("/api/params"),
        request("/api/history"),
      ]);

      if (!mounted.current) return;
      setState(nextState);
      setParams(nextParams);
      setHistory(nextHistory);
      setStatus("connected");
    } catch (err) {
      if (mounted.current) {
        setStatus("disconnected");
        console.error("[singularity]", err);
      }
    } finally {
      inFlight.current = false;
    }
  }, [request]);

  useEffect(() => {
    mounted.current = true;

    refresh();
    pollTimer.current = setInterval(refresh, POLL_MS);

    return () => {
      mounted.current = false;
      clearInterval(pollTimer.current);
    };
  }, [refresh]);

  const start = useCallback(async () => {
    await request("/api/start", { method: "POST" });
    await refresh();
  }, [refresh, request]);

  const pause = useCallback(async () => {
    await request("/api/pause", { method: "POST" });
    await refresh();
  }, [refresh, request]);

  const reset = useCallback(async () => {
    await request("/api/reset", { method: "POST" });
    await refresh();
  }, [refresh, request]);

  const setSimParams = useCallback(
    async (patch) => {
      await request("/api/params", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(patch),
      });
      await refresh();
    },
    [refresh, request],
  );

  return { status, state, params, history, start, pause, reset, setSimParams };
}
