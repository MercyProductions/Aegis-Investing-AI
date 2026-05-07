export type Route =
  | 'home'
  | 'features'
  | 'platform'
  | 'intelligence'
  | 'security'
  | 'pricing'
  | 'docs'
  | 'changelog'
  | 'about'
  | 'support'
  | 'login'
  | 'register'
  | 'forgot'
  | 'verify'
  | 'two-factor'
  | 'sessions'
  | 'dashboard'
  | 'markets'
  | 'watchlists'
  | 'scanner'
  | 'portfolio'
  | 'research'
  | 'strategy'
  | 'reports'
  | 'journal'
  | 'alerts'
  | 'diagnostics'
  | 'settings';

export const validRoutes: Route[] = [
  'home', 'features', 'platform', 'intelligence', 'security', 'pricing', 'docs', 'changelog', 'about', 'support',
  'login', 'register', 'forgot', 'verify', 'two-factor', 'sessions',
  'dashboard', 'markets', 'watchlists', 'scanner', 'portfolio', 'research', 'strategy', 'reports', 'journal', 'alerts', 'diagnostics', 'settings'
];

export const authRoutes: Route[] = ['login', 'register', 'forgot', 'verify', 'two-factor', 'sessions'];
export const appShellRoutes: Route[] = ['dashboard', 'markets', 'watchlists', 'scanner', 'portfolio', 'research', 'strategy', 'reports', 'journal', 'alerts', 'diagnostics', 'settings'];

export function pathForRoute(route: Route, symbol?: string) {
  if (route === 'home') return '/';
  if (route === 'research' && symbol) return `/research/${encodeURIComponent(symbol.toUpperCase())}`;
  return `/${route}`;
}

export function routeFromString(value: string): Route {
  const route = value.split('/')[0] as Route;
  return validRoutes.includes(route) ? route : 'home';
}

export function routeFromPath(pathname: string): Route {
  const raw = pathname.replace(/^\/+/, '') || 'home';
  return routeFromString(raw);
}

export function symbolFromPath(pathname: string) {
  const parts = pathname.replace(/^\/+/, '').split('/');
  if (parts[0] !== 'research' || !parts[1]) return '';
  return decodeURIComponent(parts[1]).trim().toUpperCase();
}

export function legacyHashToPath(hash: string) {
  const raw = hash.replace(/^#\/?/, '') || 'home';
  const route = routeFromString(raw);
  const symbol = raw.split('/')[1];
  return pathForRoute(route, route === 'research' ? symbol : undefined);
}
