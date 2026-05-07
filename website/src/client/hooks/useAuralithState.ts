import { useCallback, useState } from 'react';
import { getState } from '../lib/api';
import type { AppState } from '../types/api';

export function useAuralithState(initialState: AppState | null = null) {
  const [state, setState] = useState<AppState | null>(initialState);
  const [error, setError] = useState('');

  const refresh = useCallback(async () => {
    try {
      const next = await getState();
      setState(next);
      setError('');
      return next;
    } catch (caught) {
      const message = caught instanceof Error ? caught.message : 'Could not load Auralith state.';
      setError(message);
      throw caught;
    }
  }, []);

  return { state, setState, refresh, error };
}
