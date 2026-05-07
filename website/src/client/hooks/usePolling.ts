import { useEffect } from 'react';

export function usePolling(callback: () => void | Promise<void>, intervalMs: number) {
  useEffect(() => {
    void callback();
    const timer = window.setInterval(() => void callback(), intervalMs);
    return () => window.clearInterval(timer);
  }, [callback, intervalMs]);
}
