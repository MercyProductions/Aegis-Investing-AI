import { useEffect, useState } from 'react';
import { currentAccount } from '../lib/api';
import type { AuthUser } from '../types/auth';

export function useSession() {
  const [user, setUser] = useState<AuthUser | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    currentAccount()
      .then((result) => setUser(result.user))
      .catch(() => setUser(null))
      .finally(() => setLoading(false));
  }, []);

  return { user, loading };
}
