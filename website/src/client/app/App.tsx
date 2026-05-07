import { useEffect, useMemo, useState } from 'react';
import type { CSSProperties, FormEvent, ReactNode } from 'react';
import { useLocation, useNavigate } from 'react-router-dom';
import {
  Activity,
  BarChart3,
  Bell,
  BookOpen,
  BrainCircuit,
  BriefcaseBusiness,
  CheckCircle2,
  ChevronRight,
  Clock3,
  Command,
  Database,
  FileText,
  Fingerprint,
  Gauge,
  History,
  KeyRound,
  LayoutDashboard,
  LineChart,
  Lock,
  Mail,
  Monitor,
  Newspaper,
  PanelLeft,
  Play,
  Plus,
  RefreshCw,
  RotateCcw,
  Search,
  Settings,
  ShieldCheck,
  Smartphone,
  TrendingDown,
  TrendingUp,
  UserCircle,
  Wallet
} from 'lucide-react';
import {
  accountSessions,
  acknowledgeAlert,
  addMarketSymbol,
  createAlert,
  createJournalEntry,
  deleteAlert,
  generateReport,
  getEndOfDayReview,
  getProviderDiagnostics,
  getMorningBriefing,
  getPlatformHealth,
  getResearchTimeline,
  getState,
  listLayouts,
  listNotifications,
  listReports,
  listStrategies,
  listWatchlists,
  loginAccount,
  markNotificationRead,
  placeOrder,
  registerAccount,
  resetPaperAccount,
  runBacktest,
  saveLayout,
  saveStrategy,
  snoozeAlert,
  updatePaperCash,
  updateAlert,
  updateSettings,
  updateWatchlist,
  validatePlatformBackup,
  createWatchlist,
  deleteWatchlist,
  deleteStrategy
} from '../lib/api';
import { APP_STATE_STORAGE_KEY, SELECTED_SYMBOL_STORAGE_KEY } from '../lib/constants';
import { MarketChart } from '../components/charts/MarketChart';
import { appShellRoutes, authRoutes, legacyHashToPath, pathForRoute, routeFromPath, routeFromString, symbolFromPath } from './routes';
import type { Route } from './routes';
import type {
  AlertType,
  AppState,
  AssetClass,
  AuthSession,
  DataQualitySnapshot,
  MarketAsset,
  NotificationRecord,
  PlatformHealthSnapshot,
  ProviderDiagnostics,
  ResearchTimelineEvent,
  ResearchWorkflow,
  SavedReport,
  Signal,
  StrategyRecord,
  WatchlistRecord,
  WorkspaceLayoutRecord
} from '../types/api';

const marketingRoutes: Array<{ route: Route; label: string }> = [
  { route: 'features', label: 'Features' },
  { route: 'platform', label: 'Platform' },
  { route: 'intelligence', label: 'Research' },
  { route: 'security', label: 'Security' },
  { route: 'pricing', label: 'Pricing' },
  { route: 'docs', label: 'Docs' }
];

const appRoutes: Array<{ route: Route; label: string; icon: ReactNode }> = [
  { route: 'dashboard', label: 'Dashboard', icon: <LayoutDashboard size={17} /> },
  { route: 'watchlists', label: 'Watchlists', icon: <LineChart size={17} /> },
  { route: 'scanner', label: 'Scanner', icon: <Gauge size={17} /> },
  { route: 'portfolio', label: 'Portfolio', icon: <BriefcaseBusiness size={17} /> },
  { route: 'research', label: 'Research', icon: <BrainCircuit size={17} /> },
  { route: 'strategy', label: 'Strategy Lab', icon: <BarChart3 size={17} /> },
  { route: 'reports', label: 'Reports', icon: <FileText size={17} /> },
  { route: 'journal', label: 'Journal', icon: <BookOpen size={17} /> },
  { route: 'alerts', label: 'Alerts', icon: <Bell size={17} /> },
  { route: 'diagnostics', label: 'Diagnostics', icon: <Database size={17} /> },
  { route: 'settings', label: 'Settings', icon: <Settings size={17} /> }
];

const mobileRoutes = appRoutes.filter((item) => ['dashboard', 'watchlists', 'portfolio', 'research', 'reports', 'alerts'].includes(item.route));
function App() {
  const routerLocation = useLocation();
  const routerNavigate = useNavigate();
  const [route, setRoute] = useState<Route>(readRoute());
  const [state, setState] = useState<AppState | null>(() => readSavedAppState());
  const [selectedSymbol, setSelectedSymbol] = useState(() => readSavedSelectedSymbol());
  const [marketSearch, setMarketSearch] = useState('');
  const [assetFilter, setAssetFilter] = useState<'all' | 'crypto' | 'stock'>('all');
  const [marketSort, setMarketSort] = useState<'signals' | 'movers' | 'volume'>('signals');
  const [busy, setBusy] = useState(false);
  const [status, setStatus] = useState(state ? 'Restored last web session while providers refresh.' : '');
  const [commandOpen, setCommandOpen] = useState(false);
  const [isOnline, setIsOnline] = useState(() => (typeof navigator === 'undefined' ? true : navigator.onLine));
  const [usingCachedState, setUsingCachedState] = useState(() => Boolean(state));
  const [notificationPermission, setNotificationPermission] = useState<NotificationPermission>(() =>
    typeof Notification === 'undefined' ? 'default' : Notification.permission
  );

  useEffect(() => {
    if (routerLocation.hash.startsWith('#/')) {
      routerNavigate(legacyHashToPath(routerLocation.hash), { replace: true });
      return;
    }
    const nextRoute = routeFromPath(routerLocation.pathname);
    const nextSymbol = symbolFromPath(routerLocation.pathname);
    setRoute(nextRoute);
    if (nextRoute === 'research' && nextSymbol) setSelectedSymbol(nextSymbol);
  }, [routerLocation.hash, routerLocation.pathname, routerNavigate]);

  useEffect(() => {
    refresh();
    const timer = window.setInterval(refresh, 12000);
    return () => window.clearInterval(timer);
  }, []);

  useEffect(() => {
    const onOnline = () => {
      setIsOnline(true);
      void refresh();
    };
    const onOffline = () => {
      setIsOnline(false);
      setUsingCachedState(true);
      setStatus('Offline shell active. Showing the last saved Auralith session.');
    };
    window.addEventListener('online', onOnline);
    window.addEventListener('offline', onOffline);
    return () => {
      window.removeEventListener('online', onOnline);
      window.removeEventListener('offline', onOffline);
    };
  }, []);

  useEffect(() => {
    saveSelectedSymbol(selectedSymbol);
  }, [selectedSymbol]);

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'k') {
        event.preventDefault();
        setCommandOpen(true);
      }
    };
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, []);

  const selectedAsset = useMemo(
    () => state?.market.find((asset) => asset.symbol === selectedSymbol) ?? state?.market[0] ?? null,
    [selectedSymbol, state]
  );
  const selectedSignal = useMemo(
    () => state?.signals.find((signal) => signal.symbol === selectedAsset?.symbol) ?? null,
    [selectedAsset, state]
  );
  const filteredMarket = useMemo(() => {
    if (!state) return [];
    const query = marketSearch.trim().toLowerCase();
    const signalScore = new Map(state.signals.map((signal) => [signal.symbol, signal.confidence]));
    return state.market
      .filter((asset) => assetFilter === 'all' || asset.assetClass === assetFilter)
      .filter((asset) => !query || asset.symbol.toLowerCase().includes(query) || asset.name.toLowerCase().includes(query))
      .sort((a, b) => {
        if (marketSort === 'movers') return Math.abs(b.changePct) - Math.abs(a.changePct);
        if (marketSort === 'volume') return b.volume - a.volume;
        return (signalScore.get(b.symbol) ?? 0) - (signalScore.get(a.symbol) ?? 0);
      });
  }, [assetFilter, marketSearch, marketSort, state]);

  async function refresh() {
    try {
      const next = await getState();
      setState(next);
      saveAppState(next);
      setUsingCachedState(false);
      if (!next.market.some((asset) => asset.symbol === selectedSymbol) && next.market[0]) setSelectedSymbol(next.market[0].symbol);
      setStatus('');
    } catch (error) {
      const cached = readSavedAppState();
      if (cached) {
        setState(cached);
        setUsingCachedState(true);
        setStatus('Using saved web session. Provider refresh will resume when AuralithCore is reachable.');
      } else {
        setStatus(error instanceof Error ? error.message : 'Could not load Auralith Markets');
      }
    }
  }

  async function action(task: () => Promise<AppState>, success: string) {
    setBusy(true);
    setStatus('');
    try {
      const next = await task();
      setState(next);
      saveAppState(next);
      setUsingCachedState(false);
      setStatus(success);
    } catch (error) {
      setStatus(error instanceof Error ? error.message : 'Action failed');
    } finally {
      setBusy(false);
    }
  }

  async function paperOrder(side: 'buy' | 'sell') {
    if (!selectedAsset || !state) return;
    const notional = Math.min(state.settings.maxTradeNotional, state.account.equity * state.settings.maxPositionPct * 0.25);
    const qty = side === 'buy' ? notional / selectedAsset.price : state.positions.find((position) => position.symbol === selectedAsset.symbol)?.qty ?? 0;
    await action(
      () => placeOrder(selectedAsset.symbol, side, qty, selectedSignal?.reason ?? 'Manual cockpit paper execution'),
      side === 'buy' ? 'Paper long plan recorded.' : 'Paper exit review recorded.'
    );
  }

  async function addSymbol() {
    const symbol = marketSearch.trim().toUpperCase();
    if (!symbol) return;
    setBusy(true);
      setStatus('');
    try {
      const next = await addMarketSymbol(symbol);
      setState(next);
      saveAppState(next);
      setUsingCachedState(false);
      setSelectedSymbol(symbol.replace(/\//g, '-'));
      setStatus(`${symbol} added to the market board.`);
    } catch (error) {
      setStatus(error instanceof Error ? error.message : 'Could not add symbol');
    } finally {
      setBusy(false);
    }
  }

  function navigate(nextRoute: Route) {
    routerNavigate(pathForRoute(nextRoute, nextRoute === 'research' ? selectedSymbol : undefined));
    setRoute(nextRoute);
  }

  async function requestNotifications() {
    if (typeof Notification === 'undefined') {
      setStatus('This browser does not expose notification permissions yet.');
      return;
    }
    const permission = await Notification.requestPermission();
    setNotificationPermission(permission);
    setStatus(
      permission === 'granted'
        ? 'Auralith web notifications are enabled for alerts, reports, provider status, risk warnings, and paper order updates.'
        : 'Notifications remain disabled. You can still review everything in the Alerts center.'
    );
  }

  const appProps = {
    state,
    selectedAsset,
    selectedSignal,
    selectedSymbol,
    setSelectedSymbol,
    filteredMarket,
    marketSearch,
    setMarketSearch,
    assetFilter,
    setAssetFilter,
    marketSort,
    setMarketSort,
    busy,
    refresh,
    action,
    addSymbol,
    paperOrder,
    navigate,
    openCommandPalette: () => setCommandOpen(true),
    isOnline,
    usingCachedState,
    notificationPermission,
    requestNotifications
  };

  if (isAppRoute(route)) {
    return (
      <>
        <ApplicationShell route={route} {...appProps} />
        <CommandPalette
          open={commandOpen}
          state={state}
          onClose={() => setCommandOpen(false)}
          navigate={navigate}
          selectedSymbol={selectedSymbol}
          setSelectedSymbol={setSelectedSymbol}
          refresh={refresh}
        />
        {status ? <Toast message={status} /> : null}
      </>
    );
  }

  if (isAuthRoute(route)) return <AuthExperience route={route} />;

  return <MarketingExperience route={route} state={state} />;
}

function MarketingExperience({ route, state }: { route: Route; state: AppState | null }) {
  return (
    <main className="site-shell">
      <MarketingNav />
      {route === 'home' ? <Homepage state={state} /> : null}
      {route === 'features' ? <FeaturesPage /> : null}
      {route === 'platform' ? <PlatformPage /> : null}
      {route === 'intelligence' ? <IntelligencePage /> : null}
      {route === 'security' ? <SecurityPage /> : null}
      {route === 'pricing' ? <PricingPage /> : null}
      {route === 'docs' ? <DocsPage /> : null}
      {route === 'changelog' ? <ChangelogPage /> : null}
      {route === 'about' ? <AboutPage /> : null}
      {route === 'support' ? <SupportPage /> : null}
      <SiteFooter />
    </main>
  );
}

function MarketingNav() {
  return (
    <header className="site-nav">
      <a className="brand" href={pathForRoute('home')} aria-label="Auralith home">
        <BrandLogo />
      </a>
      <nav>
        {marketingRoutes.map((item) => <a href={pathForRoute(item.route)} key={item.route}>{item.label}</a>)}
      </nav>
      <div className="nav-actions">
        <a className="ghost-link" href={pathForRoute('login')}>Sign In</a>
        <a className="solid-link" href={pathForRoute('dashboard')}>Launch Platform</a>
      </div>
    </header>
  );
}

function BrandLogo() {
  return (
    <>
      <img className="brand-logo brand-logo-wordmark" src="/auralith-wordmark.png" alt="" aria-hidden="true" />
      <img className="brand-logo brand-logo-mark" src="/auralith-mark.png" alt="" aria-hidden="true" />
    </>
  );
}

function Homepage({ state }: { state: AppState | null }) {
  return (
    <>
      <section className="hero">
        <div className="hero-copy">
          <p className="eyebrow">AI-assisted market intelligence</p>
          <h1>A disciplined research terminal for portfolio intelligence.</h1>
          <p>Research-first portfolio analysis, paper execution, risk visibility, and long-term investment workflows in a calm local-first platform.</p>
          <div className="hero-actions">
            <a className="solid-link large" href={pathForRoute('dashboard')}>Launch Platform</a>
            <a className="ghost-link large" href={pathForRoute('platform')}>View Demo</a>
            <a className="text-link" href={pathForRoute('intelligence')}>Learn More <ChevronRight size={15} /></a>
          </div>
        </div>
        <MarketTape state={state} />
      </section>

      <section className="section-grid">
        <NarrativeBlock
          icon={<BrainCircuit size={20} />}
          title="Market Intelligence"
          copy="Watchlists, scanners, AI summaries, reports, and portfolio-aware signal context without anonymous data."
        />
        <NarrativeBlock
          icon={<ShieldCheck size={20} />}
          title="Risk Intelligence"
          copy="Exposure analysis, concentration checks, volatility awareness, historical comparison, and paper-only guardrails."
        />
        <NarrativeBlock
          icon={<History size={20} />}
          title="Research Continuity"
          copy="AI memory, journal context, timelines, scanner history, and long-term symbol research records."
        />
        <NarrativeBlock
          icon={<Lock size={20} />}
          title="Paper Execution"
          copy="Simulation-first order workflows with manual confirmation, audit history, and no accidental live-trading path."
        />
      </section>

      <section className="ecosystem-band">
        <div>
          <p className="eyebrow">Desktop + Web Ecosystem</p>
          <h2>Terminal depth, browser convenience, local-first ownership.</h2>
            <p>Auralith keeps the desktop terminal as the primary advanced research workstation while the web cockpit gradually adopts proven workflows through shared intelligence services.</p>
        </div>
        <div className="device-grid">
          <DeviceCard icon={<Monitor size={18} />} title="Desktop Terminal" meta="Primary workstation" />
          <DeviceCard icon={<PanelLeft size={18} />} title="Web Cockpit" meta="Monitoring companion" />
          <DeviceCard icon={<Smartphone size={18} />} title="Mobile Concept" meta="Alerts and briefings" />
        </div>
      </section>

      <FeatureGrid />
      <FinalCta />
    </>
  );
}

function FeaturesPage() {
  return (
    <ContentPage
      eyebrow="Product Showcase"
      title="Professional tools for disciplined market research."
      copy="Auralith organizes analytics, reports, AI systems, portfolio analysis, risk tools, watchlists, scanner workflows, journaling, layouts, and alerts into a restrained daily workflow."
      sections={[
        ['Analytics', 'Relative strength, volatility expansion, trend persistence, factor summaries, and benchmark comparison.'],
        ['Portfolio Console', 'Exposure, concentration, cash allocation, drawdown, overlap, and simulated portfolio risk.'],
        ['Scanner Engine', 'Multi-factor ranking with conviction, trend, volatility, catalyst, and data-quality context.'],
        ['Reports', 'Morning briefings, weekly reviews, risk reports, strategy reviews, and printable research exports.'],
        ['Journal System', 'Notes, lessons, tags, grades, behavioral patterns, and research continuity.'],
        ['Workspace System', 'Persistent layouts, watchlists, filters, selected symbols, and daily operating modes.']
      ]}
    />
  );
}

function PlatformPage() {
  return (
    <ContentPage
      eyebrow="Architecture"
      title="A local-first platform spanning desktop, core service, and web cockpit."
      copy="Auralith separates the terminal/client experience from the core intelligence service, persistence, providers, reporting, AI memory, and shared model contracts."
      sections={[
        ['Desktop App', 'The primary premium research terminal and reference implementation for dense portfolio, scanner, chart, journal, and report workflows.'],
        ['Web Dashboard', 'A polished companion cockpit for monitoring, watchlists, alerts, reports, portfolio review, and lightweight research.'],
        ['Core Service', 'Provider synchronization, historical storage, audit logging, AI memory, report readiness, and background tasks.'],
        ['Provider System', 'Source labeling, rate-limit awareness, freshness scoring, fallback routing, and health scoring.'],
        ['Shared Models', 'Symbols, quotes, alerts, risk events, scanner outputs, provider health, and AI summaries use stable contracts across UI layers.'],
        ['Performance Posture', 'Fast startup, local persistence, graceful failures, and offline resilience are product requirements.']
      ]}
    />
  );
}

function IntelligencePage() {
  return (
    <ContentPage
      eyebrow="Research & Intelligence"
      title="AI that explains conditions instead of pretending to predict them."
      copy="Auralith's AI layer should connect scanner logic, historical memory, portfolio state, journal patterns, risk models, and report context with cautious language."
      sections={[
        ['AI Reasoning', 'Explains why a signal exists, what changed, what risk increased, and what would invalidate a setup.'],
        ['Historical Memory', 'Connects symbol timelines, previous setups, scanner outcomes, volatility behavior, and research notes.'],
        ['Portfolio Insights', 'Surfaces concentration, exposure drift, repeated mistakes, allocation patterns, and risk changes.'],
        ['Risk Models', 'Highlights volatility, sector exposure, stop coverage, drawdown pressure, beta, and scenario impact.'],
        ['Reporting Engine', 'Turns research context into concise, printable, executive-quality reports.'],
        ['Language Standard', 'Uses cautious phrasing: suggests, may indicate, risk appears elevated, confirmation remains needed.']
      ]}
    />
  );
}

function SecurityPage() {
  return (
    <ContentPage
      eyebrow="Security & Privacy"
      title="Local-first by design, transparent by default."
      copy="Auralith should protect user ownership, keep live execution locked, expose audit trails, and avoid forced cloud dependency."
      sections={[
        ['Local-first Data', 'Research history, reports, journal entries, layouts, and AI memory remain locally owned by default.'],
        ['Encrypted Secrets', 'Provider keys belong behind isolated secure storage, not in plaintext workflows.'],
        ['Audit Logs', 'Settings changes, provider failures, paper execution, reports, migrations, and overrides are auditable.'],
        ['Paper-first Safety', 'Manual confirmation is required and live execution paths remain intentionally locked.'],
        ['No Forced Cloud', 'Optional sync can exist later, but the terminal must remain useful offline.'],
        ['Transparency', 'Every quote, report, signal, and AI summary should show source, freshness, fallback, and confidence.']
      ]}
    />
  );
}

function PricingPage() {
  return (
    <section className="content-page">
      <PageHeader eyebrow="Pricing" title="Simple tiers for a local-first research platform." copy="Pricing should stay calm and transparent. No urgency tricks, no casino language." />
      <div className="pricing-grid">
        {[
          ['Community', '$0', 'Local research workflows, paper portfolio, watchlists, and core reports.'],
          ['Professional', '$19', 'Advanced analytics, workspace profiles, richer reports, and expanded provider workflows.'],
          ['Research', '$49', 'Historical intelligence, strategy lab depth, scenario reports, and AI memory workflows.'],
          ['Enterprise', 'Future', 'Deployment planning, controls, policy support, and team-oriented governance if ever needed.']
        ].map(([name, price, copy]) => (
          <article className="price-card" key={name}>
            <span>{name}</span>
            <strong>{price}</strong>
            <p>{copy}</p>
            <a className="ghost-link" href={pathForRoute('login')}>Start</a>
          </article>
        ))}
      </div>
    </section>
  );
}

function DocsPage() {
  return (
    <ContentPage
      eyebrow="Documentation"
      title="Everything needed to operate Auralith with discipline."
      copy="Docs should feel like product-grade operating instructions, not marketing fluff."
      sections={[
        ['Getting Started', 'Configure watchlists, paper mode, provider keys, risk preferences, and your first portfolio.'],
        ['Providers', 'Understand source labeling, freshness, fallback routing, rate limits, and provider diagnostics.'],
        ['Scanners', 'Read scanner score, conviction, risk, data confidence, and signal explanations.'],
        ['Reports', 'Generate morning briefings, weekly reviews, risk reports, and strategy summaries.'],
        ['Layouts', 'Use daily operating layouts for market open, research, portfolio review, and strategy testing.'],
        ['Risk Model', 'Understand Healthy, Caution, Elevated, Defensive, and Critical risk states.']
      ]}
    />
  );
}

function ChangelogPage() {
  return (
    <ContentPage
      eyebrow="Changelog"
      title="Maintained with care."
      copy="A clear changelog makes Auralith feel alive, accountable, and stable."
      sections={[
        ['2026.05', 'Professional platform foundations, shared contracts, SDK boundaries, and stewardship doctrine.'],
        ['2026.04', 'Auralith rebrand, safety hardening, paper-first UX, SQLite migration planning, and web cockpit polish.'],
        ['Next', 'Quality, refinement, AI memory depth, data trustworthiness, and workflow simplification.']
      ]}
    />
  );
}

function AboutPage() {
  return (
    <ContentPage
      eyebrow="Philosophy"
      title="Built for disciplined research, not market theater."
      copy="Auralith exists to help users understand markets, understand risk, maintain research continuity, and improve decision quality without pretending to predict the future."
      sections={[
        ['Research-first', 'The product is designed around observation, explanation, and review.'],
        ['Paper-first', 'Execution workflows remain simulated and manually confirmed.'],
        ['Local-first', 'User research history and AI memory should remain owned by the user.'],
        ['Calm by default', 'The interface avoids hype, neon urgency, and excessive motion.']
      ]}
    />
  );
}

function SupportPage() {
  return (
    <ContentPage
      eyebrow="Contact & Support"
      title="Support for a serious research workstation."
      copy="Keep support channels direct, calm, and useful."
      sections={[
        ['Support Email', 'support@auralith.local'],
        ['Bug Reports', 'Include diagnostics, provider state, timestamps, and reproduction steps.'],
        ['Feedback', 'Prioritize clarity, workflow friction, stale data behavior, and report quality.'],
        ['Documentation', 'Start with the docs page for setup, providers, risk states, reports, and troubleshooting.']
      ]}
    />
  );
}

function AuthExperience({ route }: { route: Route }) {
  const routerNavigate = useNavigate();
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [workspaceName, setWorkspaceName] = useState('');
  const [rememberDevice, setRememberDevice] = useState(true);
  const [authStatus, setAuthStatus] = useState('');
  const [authBusy, setAuthBusy] = useState(false);
  const [sessions, setSessions] = useState<AuthSession[]>([]);
  const meta = {
    login: ['Sign in to Auralith', 'Access the research cockpit and synchronized workspace controls.'],
    register: ['Create your workspace', 'Start with paper mode, local-first defaults, and provider controls.'],
    forgot: ['Recover access', 'Reset credentials without weakening device and session protections.'],
    verify: ['Verify email', 'Confirm the address attached to your Auralith workspace.'],
    'two-factor': ['Two-factor setup', 'Add a second step before sensitive account or sync changes.'],
    sessions: ['Device history', 'Review trusted sessions, devices, and recent access.']
  }[route as 'login'] ?? ['Sign in to Auralith', 'Access the research cockpit.'];

  useEffect(() => {
    setAuthStatus('');
    if (route !== 'sessions') return;
    accountSessions()
      .then((result) => setSessions(result.sessions))
      .catch((error) => setAuthStatus(error instanceof Error ? error.message : 'No active Auralith session.'));
  }, [route]);

  async function submitAuth(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    setAuthBusy(true);
    setAuthStatus('');
    try {
      if (route === 'login') {
        const result = await loginAccount({ email, password, rememberDevice });
        setAuthStatus(`Signed in as ${result.user.email}. Opening the Auralith platform.`);
        routerNavigate(pathForRoute('dashboard'));
        return;
      }
      if (route === 'register') {
        const result = await registerAccount({ email, password, workspaceName, rememberDevice });
        setAuthStatus(`Created ${result.user.workspaceName}. Opening the Auralith platform.`);
        routerNavigate(pathForRoute('dashboard'));
        return;
      }
      if (route === 'forgot') {
        setAuthStatus('Password recovery is staged for the local-first account system. Create a new local workspace or sign in with an existing one.');
        return;
      }
      if (route === 'verify') {
        setAuthStatus('Email verification is staged. Local Auralith accounts can use the platform immediately.');
        return;
      }
      if (route === 'two-factor') {
        setAuthStatus('2FA setup is staged. Device sessions and password auth are active now.');
      }
    } catch (error) {
      setAuthStatus(error instanceof Error ? error.message : 'Authentication failed.');
    } finally {
      setAuthBusy(false);
    }
  }

  return (
    <main className="auth-shell">
      <a className="brand auth-brand" href={pathForRoute('home')} aria-label="Auralith home">
        <BrandLogo />
      </a>
      <section className="auth-card">
        <p className="eyebrow">Secure Workspace</p>
        <h1>{meta[0]}</h1>
        <p>{meta[1]}</p>
        <form onSubmit={(event) => void submitAuth(event)}>
          {route === 'two-factor' ? <Input icon={<KeyRound size={17} />} label="Authentication code" type="text" autoComplete="one-time-code" /> : null}
          {route === 'sessions' ? <SessionList sessions={sessions} /> : null}
          {route !== 'two-factor' && route !== 'sessions' ? <Input icon={<Mail size={17} />} label="Email" type="email" value={email} onChange={setEmail} autoComplete="email" /> : null}
          {route === 'login' || route === 'register' ? <Input icon={<Lock size={17} />} label="Password" type="password" value={password} onChange={setPassword} autoComplete={route === 'login' ? 'current-password' : 'new-password'} /> : null}
          {route === 'register' ? <Input icon={<UserCircle size={17} />} label="Workspace name" type="text" value={workspaceName} onChange={setWorkspaceName} autoComplete="organization" /> : null}
          {route === 'login' ? (
            <label className="remember-row">
              <input type="checkbox" checked={rememberDevice} onChange={(event) => setRememberDevice(event.target.checked)} />
              <span>Remember this device</span>
            </label>
          ) : null}
          {route === 'register' ? (
            <label className="remember-row">
              <input type="checkbox" checked={rememberDevice} onChange={(event) => setRememberDevice(event.target.checked)} />
              <span>Remember this device</span>
            </label>
          ) : null}
          {authStatus ? <p className="auth-status">{authStatus}</p> : null}
          {route !== 'sessions' ? <button className="solid-link full" disabled={authBusy} type="submit">{route === 'login' ? 'Sign In' : 'Continue'}</button> : null}
        </form>
        <div className="auth-links">
          <a href={pathForRoute('forgot')}>Forgot password</a>
          <a href={pathForRoute('register')}>Create account</a>
          <a href={pathForRoute('sessions')}>Device history</a>
        </div>
      </section>
    </main>
  );
}

type ApplicationProps = {
  route: Route;
  state: AppState | null;
  selectedAsset: MarketAsset | null;
  selectedSignal: Signal | null;
  selectedSymbol: string;
  setSelectedSymbol: (value: string) => void;
  filteredMarket: MarketAsset[];
  marketSearch: string;
  setMarketSearch: (value: string) => void;
  assetFilter: 'all' | 'crypto' | 'stock';
  setAssetFilter: (value: 'all' | 'crypto' | 'stock') => void;
  marketSort: 'signals' | 'movers' | 'volume';
  setMarketSort: (value: 'signals' | 'movers' | 'volume') => void;
  busy: boolean;
  refresh: () => Promise<void>;
  action: (task: () => Promise<AppState>, success: string) => Promise<void>;
  addSymbol: () => Promise<void>;
  paperOrder: (side: 'buy' | 'sell') => Promise<void>;
  navigate: (route: Route) => void;
  openCommandPalette: () => void;
  isOnline: boolean;
  usingCachedState: boolean;
  notificationPermission: NotificationPermission;
  requestNotifications: () => Promise<void>;
};

function ApplicationShell(props: ApplicationProps) {
  const state = props.state;
  return (
    <main className="terminal-shell">
      <aside className="app-sidebar">
        <a className="brand" href={pathForRoute('home')} aria-label="Auralith home">
          <BrandLogo />
        </a>
        <nav>
          {appRoutes.map((item) => (
            <a className={props.route === item.route ? 'active' : ''} href={pathForRoute(item.route)} key={item.route}>
              {item.icon}
              {item.label}
            </a>
          ))}
        </nav>
        <div className="sidebar-status">
          <span><ShieldCheck size={15} /> Paper Mode: ON</span>
          <span><Activity size={15} /> {state ? `${state.core.providerHealth.source} / ${state.core.providerHealth.freshness}` : 'Providers loading'}</span>
          <span><Database size={15} /> {props.usingCachedState ? 'Saved session active' : 'Live session cache'}</span>
        </div>
      </aside>

      <section className="terminal-main">
        <TopBar {...props} />
        {!state ? (
          <LoadingState />
        ) : (
          <div className="workspace-with-rail">
            <div className="workspace-primary">
              <AppRouteContent {...props} state={state} />
            </div>
            <ContextRail state={state} selectedAsset={props.selectedAsset} selectedSignal={props.selectedSignal} navigate={props.navigate} />
          </div>
        )}
      </section>
      <MobileAppNav route={props.route} />
    </main>
  );
}

function TopBar(props: ApplicationProps) {
  return (
    <header className="terminal-topbar">
      <div className="command-search">
        <Search size={17} />
        <input
          value={props.marketSearch}
          onChange={(event) => props.setMarketSearch(event.target.value)}
          onKeyDown={(event) => {
            if (event.key === 'Enter') void props.addSymbol();
          }}
          placeholder="Search symbol, report, layout, or command"
        />
      </div>
      <div className="top-status">
        <button className="command-trigger" onClick={props.openCommandPalette} type="button">
          <Command size={15} /> Ctrl K
        </button>
        <span className="workspace-pill"><PanelLeft size={15} /> Research Workspace</span>
        <span className={props.isOnline ? undefined : 'warn'}><Activity size={15} /> {props.isOnline ? 'Online' : 'Offline shell'}</span>
        <span><Clock3 size={15} /> Market: {marketSessionLabel()}</span>
        <span><Database size={15} /> {props.state ? `${props.state.core.providerHealth.confidence}% / ${props.state.core.providerHealth.freshness}` : 'Syncing'}</span>
        <span className={props.usingCachedState ? 'warn' : undefined}><History size={15} /> {props.usingCachedState ? 'Saved session' : 'Live cache'}</span>
        <button className="icon-button" onClick={props.refresh} disabled={props.busy} type="button" title="Refresh providers">
          <RefreshCw size={16} />
        </button>
      </div>
    </header>
  );
}

function MobileAppNav({ route }: { route: Route }) {
  return (
    <nav className="mobile-nav" aria-label="Mobile application navigation">
      {mobileRoutes.map((item) => (
        <a className={route === item.route ? 'active' : ''} href={pathForRoute(item.route)} key={item.route}>
          {item.icon}
          <span>{item.label.replace('Watchlists', 'Watch').replace('Portfolio', 'Port')}</span>
        </a>
      ))}
    </nav>
  );
}

function ContextRail({
  state,
  selectedAsset,
  selectedSignal,
  navigate
}: {
  state: AppState;
  selectedAsset: MarketAsset | null;
  selectedSignal: Signal | null;
  navigate: (route: Route) => void;
}) {
  const selectedConfidence = selectedSignal ? Math.round(selectedSignal.confidence * 100) : state.core.providerHealth.confidence;
  const selectedFreshness = selectedAsset ? assetFreshness(selectedAsset) : state.core.providerHealth.freshness;
  return (
    <aside className="context-rail">
      <Panel title="AI Context" icon={<BrainCircuit size={17} />} variant="active">
        <p className="analysis-copy">
          {selectedSignal?.reason ?? 'Select a symbol to link scanner context, risk state, notes, and alerts across the web cockpit.'}
        </p>
        <MiniSparkline asset={selectedAsset} />
        <ConfidenceMeter label="Research Confidence" value={selectedConfidence} />
        <div className="rail-badge-row">
          <FreshnessBadge value={selectedFreshness} />
          <span>{selectedAsset ? assetProvider(selectedAsset) : state.core.providerHealth.source}</span>
        </div>
        <StatusLine label="Symbol" value={selectedAsset?.symbol ?? '--'} />
        <StatusLine label="Asset Type" value={selectedAsset ? assetClassLabel(selectedAsset.assetClass) : '--'} />
        <StatusLine label="Current Price" value={selectedAsset ? money(selectedAsset.price) : '--'} />
        <StatusLine label="Setup" value={selectedSignal?.setupType ?? '--'} />
        <StatusLine label="Sell Zone" value={selectedSignal ? sellZoneWithPrice(selectedSignal) : '--'} />
        <StatusLine label="Risk/Reward" value={selectedSignal ? `${selectedSignal.riskReward.toFixed(1)}R` : '--'} />
        <StatusLine label="Risk" value={selectedSignal?.risk ?? riskState(state)} />
      </Panel>
      <Panel title="Provider State" icon={<Database size={17} />} variant="utility">
        <div className="rail-badge-row">
          <FreshnessBadge value={state.core.providerHealth.freshness} />
          <span>{state.core.providerHealth.fallback ? 'Fallback active' : 'Primary route'}</span>
        </div>
        <ConfidenceMeter label="Data Confidence" value={state.core.providerHealth.confidence} />
        <StatusLine label="Core" value={state.core.providerHealth.coreAvailable ? 'Connected' : 'Bridge fallback'} />
        <StatusLine label="Source" value={state.core.providerHealth.source} />
        <StatusLine label="Last Refresh" value={formatTime(state.core.providerHealth.lastRefresh)} />
        <StatusLine label="Quotes" value={String(state.core.providerHealth.quoteCount)} />
      </Panel>
      <Panel title="Quick Actions" icon={<Command size={17} />} variant="utility">
        <div className="quick-actions">
          <button type="button" onClick={() => navigate('research')}>Open Symbol Research</button>
          <button type="button" onClick={() => navigate('reports')}>Open Reports Hub</button>
          <button type="button" onClick={() => navigate('alerts')}>Review Notifications</button>
        </div>
      </Panel>
    </aside>
  );
}

function CommandPalette({
  open,
  state,
  onClose,
  navigate,
  selectedSymbol,
  setSelectedSymbol,
  refresh
}: {
  open: boolean;
  state: AppState | null;
  onClose: () => void;
  navigate: (route: Route) => void;
  selectedSymbol: string;
  setSelectedSymbol: (symbol: string) => void;
  refresh: () => Promise<void>;
}) {
  const [query, setQuery] = useState('');
  const [commandStatus, setCommandStatus] = useState('');
  useEffect(() => {
    if (open) {
      setQuery('');
      setCommandStatus('');
    }
  }, [open]);

  if (!open) return null;

  const pageCommands = [
    ['Open Dashboard', 'Daily command center', 'dashboard'],
    ['Open Watchlists', 'Grouped watchlist monitoring', 'watchlists'],
    ['Open Scanner', 'Institutional scanner stream', 'scanner'],
    ['Open Portfolio', 'Portfolio health and exposure', 'portfolio'],
    ['Open Reports', 'Morning, weekly, risk, and strategy reports', 'reports'],
    ['Open Journal', 'Research notes and lessons', 'journal'],
    ['Open Diagnostics', 'Provider health, cache usage, fallback state, and live channel', 'diagnostics'],
    ['Open Settings', 'Providers, security, themes, and risk settings', 'settings']
  ] as Array<[string, string, Route]>;
  const symbolCommands = (state?.market ?? []).slice(0, 12).map((asset) => [`Research ${asset.symbol}`, `${asset.name} / ${asset.source}`, asset.symbol] as const);
  const selectedAsset = state?.market.find((asset) => asset.symbol === selectedSymbol);
  const actionCommands = [
    ['Create alert', selectedAsset ? `Price alert for ${selectedAsset.symbol}` : 'Create a selected-symbol alert', async () => {
      if (!selectedAsset) throw new Error('Select a symbol first.');
      await createAlert({ type: 'price_above', symbol: selectedAsset.symbol, threshold: selectedAsset.price * 1.03, note: 'Created from command palette.' });
      await refresh();
      return `Alert created for ${selectedAsset.symbol}.`;
    }],
    ['Add to watchlist', selectedAsset ? `Create a research queue for ${selectedAsset.symbol}` : 'Save selected symbol to watchlist workflow', async () => {
      if (!selectedAsset) throw new Error('Select a symbol first.');
      await createWatchlist({ name: `${selectedAsset.symbol} Research`, role: 'Command palette symbol queue', tags: ['command'], symbols: [selectedAsset.symbol] });
      await refresh();
      return `${selectedAsset.symbol} watchlist created.`;
    }],
    ['Generate report', selectedAsset ? `Create ${selectedAsset.symbol} research pack` : 'Generate symbol report', async () => {
      await generateReport('symbol', selectedSymbol);
      await refresh();
      return `${selectedSymbol} report generated.`;
    }],
    ['Run scanner', 'Refresh providers and scanner cycle', async () => {
      await refresh();
      return 'Scanner and provider state refreshed.';
    }],
    ['Switch workspace', 'Open saved layouts and workspace controls', async () => {
      navigate('settings');
      return 'Workspace controls opened.';
    }]
  ] as Array<[string, string, () => Promise<string>]>;
  const normalized = query.trim().toLowerCase();
  const visiblePages = pageCommands.filter(([label, detail]) => !normalized || `${label} ${detail}`.toLowerCase().includes(normalized));
  const visibleSymbols = symbolCommands.filter(([label, detail]) => !normalized || `${label} ${detail}`.toLowerCase().includes(normalized));
  const visibleActions = actionCommands.filter(([label, detail]) => !normalized || `${label} ${detail}`.toLowerCase().includes(normalized));

  async function runAction(task: () => Promise<string>) {
    try {
      setCommandStatus(await task());
    } catch (error) {
      setCommandStatus(error instanceof Error ? error.message : 'Command failed.');
    }
  }

  return (
    <div className="command-overlay" role="dialog" aria-modal="true">
      <div className="command-panel">
        <div className="command-input">
          <Command size={18} />
          <input autoFocus value={query} onChange={(event) => setQuery(event.target.value)} onKeyDown={(event) => { if (event.key === 'Escape') onClose(); }} placeholder="Search symbols, pages, reports, or actions" />
        </div>
        <div className="command-results">
          {visiblePages.map(([label, detail, target]) => (
            <button key={label} type="button" onClick={() => { navigate(target); onClose(); }}>
              <span>{label}</span>
              <small>{detail}</small>
            </button>
          ))}
          {visibleSymbols.map(([label, detail, symbol]) => (
            <button key={label} type="button" onClick={() => { setSelectedSymbol(symbol); navigate('research'); onClose(); }}>
              <span>{label}</span>
              <small>{detail}</small>
            </button>
          ))}
          {visibleActions.map(([label, detail, task]) => (
            <button key={label} type="button" onClick={() => void runAction(task)}>
              <span>{label}</span>
              <small>{detail}</small>
            </button>
          ))}
          {commandStatus ? <p className="auth-status">{commandStatus}</p> : null}
          {!visiblePages.length && !visibleSymbols.length && !visibleActions.length ? <p className="muted">No command found. Try a symbol, page name, report, alert, or workspace action.</p> : null}
        </div>
      </div>
      <button className="command-backdrop" type="button" onClick={onClose} aria-label="Close command palette" />
    </div>
  );
}

function AppRouteContent(props: ApplicationProps & { state: AppState }) {
  if (props.route === 'watchlists' || props.route === 'markets') return <WatchlistsPageApp {...props} />;
  if (props.route === 'scanner') return <ScannerPageApp {...props} />;
  if (props.route === 'portfolio') return <PortfolioPageApp {...props} />;
  if (props.route === 'research') return <SymbolResearchApp {...props} />;
  if (props.route === 'strategy') return <StrategyPageApp {...props} />;
  if (props.route === 'reports') return <ReportsPageApp {...props} />;
  if (props.route === 'journal') return <JournalPageApp {...props} />;
  if (props.route === 'alerts') return <AlertsPageApp {...props} />;
  if (props.route === 'diagnostics') return <DiagnosticsPageApp {...props} />;
  if (props.route === 'settings') return <SettingsPageApp {...props} />;
  return <DashboardApp {...props} />;
}

function DashboardApp(props: ApplicationProps & { state: AppState }) {
  const { state } = props;
  return (
    <div className="dashboard-stack">
      <MarketRibbon market={state.market} />
      <CoreStatusStrip state={state} isOnline={props.isOnline} usingCachedState={props.usingCachedState} />
      <DataQualityBanner state={state} />
      <MobileCommandDeck state={state} selectedSignal={props.selectedSignal} />
      <section className="dashboard-grid">
        <Panel title="Portfolio Snapshot" icon={<Wallet size={17} />} variant="hero">
          <div className="metric-strip">
            <MiniMetric label="Equity" value={money(state.account.equity)} tone={state.account.dailyPnl >= 0 ? 'good' : 'bad'} />
            <MiniMetric label="Daily P/L" value={money(state.account.dailyPnl)} tone={state.account.dailyPnl >= 0 ? 'good' : 'bad'} />
            <MiniMetric label="Exposure" value={money(state.account.exposure)} tone="neutral" />
            <MiniMetric label="Risk State" value={riskState(state)} tone={riskState(state) === 'Healthy' ? 'good' : 'warn'} />
          </div>
          <div className="hero-meta-row">
            <ConfidenceMeter label="Portfolio Confidence" value={state.core.providerHealth.confidence} />
            <FreshnessBadge value={state.core.providerHealth.freshness} />
          </div>
        </Panel>

        <Panel title="AI Briefing" icon={<BrainCircuit size={17} />} variant="hero">
          <AiBriefing state={state} />
        </Panel>

        <Panel title="Watchlist Movers" icon={<LineChart size={17} />} variant="primary">
          <Heatmap assets={watchlistMoverAssets(state)} onSelect={props.setSelectedSymbol} selected={props.selectedSymbol} />
        </Panel>

        <Panel title="Scanner Highlights" icon={<Gauge size={17} />} variant="primary">
          <ScannerHighlights state={state} onSelect={props.setSelectedSymbol} />
        </Panel>

        <Panel title="Risk Overview" icon={<ShieldCheck size={17} />} variant="utility">
          <RiskOverview state={state} />
        </Panel>

        <Panel title="Alerts" icon={<Bell size={17} />} variant="utility">
          <AuditList events={state.audit.filter((event) => event.level !== 'info').slice(0, 5)} empty="No triggered alerts or warnings." />
        </Panel>

        <Panel title="Paper Order Queue" icon={<FileText size={17} />} variant="utility">
          <DataTable
            empty="No paper executions recorded."
            headers={['Symbol', 'Side', 'Status', 'Notional', 'Time']}
            rows={state.orders.slice(0, 6).map((order) => [
              order.symbol,
              order.side.toUpperCase(),
              order.status,
              money(order.notional),
              formatTime(order.createdAt)
            ])}
          />
        </Panel>
      </section>
    </div>
  );
}

function CoreStatusStrip({ state, isOnline, usingCachedState }: { state: AppState; isOnline: boolean; usingCachedState: boolean }) {
  const provider = state.core.providerHealth;
  return (
    <section className="core-status-strip">
      <MiniMetric label="Shared Core" value={provider.coreAvailable ? 'Connected' : 'Fallback'} tone={provider.coreAvailable ? 'good' : 'warn'} />
      <MiniMetric label="Provider" value={provider.source} tone={provider.fallback ? 'warn' : 'good'} />
      <MiniMetric label="Freshness" value={provider.freshness} tone={provider.freshness === 'live' ? 'good' : provider.freshness === 'failed' ? 'bad' : 'warn'} />
      <MiniMetric label="Confidence" value={`${provider.confidence}%`} tone={provider.confidence >= 75 ? 'good' : provider.confidence >= 55 ? 'warn' : 'bad'} />
      <MiniMetric label="Paper Mode" value={state.core.safety.paperOnly ? 'ON' : 'Review'} tone={state.core.safety.paperOnly ? 'good' : 'bad'} />
      <MiniMetric label="Manual Confirmation" value={state.core.safety.manualConfirmationRequired ? 'Required' : 'Review'} tone={state.core.safety.manualConfirmationRequired ? 'good' : 'bad'} />
      <MiniMetric label="Web Shell" value={isOnline ? 'Online' : 'Offline'} tone={isOnline ? 'good' : 'warn'} />
      <MiniMetric label="Session" value={usingCachedState ? 'Saved' : 'Live'} tone={usingCachedState ? 'warn' : 'good'} />
    </section>
  );
}

function DataQualityBanner({ state }: { state: AppState }) {
  const fallback = state.market.filter((asset) => asset.metadata?.fallback || asset.metadata?.freshness === 'fallback').length;
  const stale = state.market.filter((asset) => asset.metadata?.freshness === 'stale').length;
  const failed = state.market.filter((asset) => asset.metadata?.freshness === 'failed' || asset.metadata?.error).length;
  if (!fallback && !stale && !failed) return null;
  const message = [
    fallback ? `${fallback} fallback/demo` : '',
    stale ? `${stale} stale` : '',
    failed ? `${failed} failed` : ''
  ].filter(Boolean).join(' / ');
  return (
    <section className="data-quality-banner">
      <div>
        <strong>Data quality warning</strong>
        <span>{message}. Research confidence is reduced until providers refresh successfully.</span>
      </div>
      <a className="ghost-link" href={pathForRoute('diagnostics')}>Open Diagnostics</a>
    </section>
  );
}

function MobileCommandDeck({ state, selectedSignal }: { state: AppState; selectedSignal: Signal | null }) {
  const nextReport = state.core.reports[0];
  const nextAlert = state.core.alerts[0];
  return (
    <section className="mobile-command-deck" aria-label="Mobile daily intelligence summary">
      <MiniMetric label="Portfolio" value={state.core.portfolio.riskState} tone={state.core.portfolio.riskState === 'Healthy' ? 'good' : 'warn'} />
      <MiniMetric label="Top Signal" value={selectedSignal?.symbol ?? state.core.scanner[0]?.symbol ?? 'Review'} tone={selectedSignal ? 'good' : 'neutral'} />
      <MiniMetric label="Alerts" value={nextAlert ? nextAlert.severity : 'Clear'} tone={nextAlert?.severity === 'error' ? 'bad' : nextAlert ? 'warn' : 'good'} />
      <MiniMetric label="Report" value={nextReport?.status ?? 'Draft'} tone={nextReport?.status === 'ready' ? 'good' : 'warn'} />
    </section>
  );
}

function AiBriefing({ state }: { state: AppState }) {
  const [briefing, setBriefing] = useState<ResearchWorkflow | null>(null);

  useEffect(() => {
    getMorningBriefing()
      .then((result) => setBriefing(result.workflow))
      .catch(() => setBriefing(null));
  }, [state.generatedAt]);

  return (
    <div className="research-feed">
      {briefing ? (
        <article className="feed-item workflow-feature">
          <span>morning workflow</span>
          <strong>{briefing.title} / {briefing.confidence}%</strong>
          <p>{briefing.summary}</p>
          <small>{briefing.actionChecklist.slice(0, 2).map((item) => item.label).join(' / ')}</small>
        </article>
      ) : null}
      {state.core.aiSummaries.map((summary) => (
        <article className="feed-item" key={summary.id}>
          <span>{summary.scope}</span>
          <strong>{summary.title} / {Math.round(summary.confidence * 100)}%</strong>
          <p>{summary.summary}</p>
          <small>{summary.evidence.join(' / ')}</small>
        </article>
      ))}
    </div>
  );
}

function ScannerHighlights({ state, onSelect }: { state: AppState; onSelect: (symbol: string) => void }) {
  const leaders = [...state.core.scanner].sort((a, b) => b.score - a.score).slice(0, 5);
  if (!leaders.length) return <p className="muted">No scanner highlights yet. Refresh providers to build signal context.</p>;
  return (
    <div className="signal-stack">
      {leaders.map((signal) => (
        <button className="signal-row" key={signal.symbol} onClick={() => onSelect(signal.symbol)} type="button">
          <span>{signal.symbol}</span>
          <strong>{Math.round(signal.score)}</strong>
          <div>
            <small>{assetClassLabel(signal.assetClass)} / {signal.setupType} / {sellZoneWithPrice(signal)}</small>
            <ConfidenceMeter label="Confidence" value={Math.round(signal.confidence * 100)} compact />
          </div>
        </button>
      ))}
    </div>
  );
}

function WatchlistsPageApp(props: ApplicationProps & { state: AppState }) {
  const [watchlists, setWatchlists] = useState<WatchlistRecord[]>([]);
  const [watchlistName, setWatchlistName] = useState('Research Queue');
  const [watchlistSymbols, setWatchlistSymbols] = useState(props.selectedSymbol);
  const [watchlistStatus, setWatchlistStatus] = useState('');

  useEffect(() => {
    listWatchlists()
      .then((result) => setWatchlists(result.watchlists))
      .catch(() => setWatchlists([]));
  }, [props.state.generatedAt]);

  async function saveWatchlist() {
    try {
      const result = await createWatchlist({
        name: watchlistName,
        role: 'Web-managed research watchlist',
        tags: ['web'],
        symbols: watchlistSymbols.split(/[,\s]+/).filter(Boolean)
      });
      setWatchlists((current) => [result.watchlist, ...current]);
      setWatchlistStatus(`${result.watchlist.name} saved.`);
      await props.refresh();
    } catch (error) {
      setWatchlistStatus(error instanceof Error ? error.message : 'Could not save watchlist.');
    }
  }

  async function updateSymbols(watchlist: WatchlistRecord, symbol: string, mode: 'add' | 'remove') {
    const symbols = mode === 'add'
      ? [...new Set([...watchlist.symbols, symbol])]
      : watchlist.symbols.filter((item) => item !== symbol);
    const result = await updateWatchlist(watchlist.id, { symbols });
    setWatchlists((current) => current.map((item) => item.id === result.watchlist.id ? result.watchlist : item));
    await props.refresh();
  }

  return (
    <div className="page-stack">
      <AppPageHeader title="Watchlists" copy="Fast grouped market monitoring with scoring, volatility, source freshness, and alert readiness." />
      <ScannerControls {...props} />
      <section className="two-column">
        <Panel title="Create Watchlist" icon={<Plus size={17} />}>
          <div className="journal-editor">
            <input value={watchlistName} onChange={(event) => setWatchlistName(event.target.value)} placeholder="Watchlist name" />
            <input value={watchlistSymbols} onChange={(event) => setWatchlistSymbols(event.target.value.toUpperCase())} placeholder="Symbols, comma separated" />
            <button className="solid-link" onClick={() => void saveWatchlist()} type="button">Save Watchlist</button>
            {watchlistStatus ? <p className="auth-status">{watchlistStatus}</p> : null}
          </div>
        </Panel>
        <Panel title="Saved Watchlists" icon={<LineChart size={17} />}>
          <div className="audit-list">
            {watchlists.map((watchlist) => (
              <article className="audit-row info" key={watchlist.id}>
                <strong>{watchlist.name} / {watchlist.symbols.length} symbols</strong>
                <span>{watchlist.role} / {watchlist.tags.join(', ') || 'untagged'}</span>
                <div className="rail-badge-row">
                  <button className="ghost-link" onClick={() => void updateSymbols(watchlist, props.selectedSymbol, 'add')} type="button">Add {props.selectedSymbol}</button>
                  <button className="ghost-link" onClick={() => void deleteWatchlist(watchlist.id).then(() => setWatchlists((items) => items.filter((item) => item.id !== watchlist.id)))} type="button">Delete</button>
                </div>
              </article>
            ))}
            {!watchlists.length ? <p className="muted">No saved web watchlists yet.</p> : null}
          </div>
        </Panel>
      </section>
      <section className="watchlist-grid">
        {props.state.core.watchlists.map((group) => (
          <Panel title={group.name} icon={<LineChart size={17} />} key={group.id}>
            <p className="muted">{group.role}</p>
            {group.movers.length ? (
              <div className="watchlist-cards">
                {group.movers.map((mover) => {
                  const asset = props.state.market.find((item) => item.symbol === mover.symbol);
                  if (!asset) return null;
                  const signal = props.state.signals.find((item) => item.symbol === asset.symbol);
                  return (
                    <button className="watch-card" key={`${group.id}-${asset.symbol}`} onClick={() => props.setSelectedSymbol(asset.symbol)} type="button">
                      <div>
                        <strong>{asset.symbol}</strong>
                        <span>{asset.name}</span>
                      </div>
                      <div>
                        <strong>{money(asset.price)}</strong>
                        <span className={asset.changePct >= 0 ? 'good' : 'bad'}>{pct(asset.changePct)}</span>
                      </div>
                      <small>Score {signal ? Math.round(signal.score) : '--'} / Vol {pct(asset.volatility)} / {freshnessLabel(Date.parse(asset.updatedAt))}</small>
                    </button>
                  );
                })}
              </div>
            ) : (
              <p className="muted">No symbols match this group yet. Add symbols or adjust filters to begin tracking.</p>
            )}
          </Panel>
        ))}
      </section>
    </div>
  );
}

function ScannerPageApp(props: ApplicationProps & { state: AppState }) {
  const signalBySymbol = new Map(props.state.signals.map((signal) => [signal.symbol, signal]));
  const showAssetClass = props.assetFilter === 'all';
  return (
    <div className="page-stack">
      <AppPageHeader title="Institutional Scanner" copy="Conviction, momentum, volatility, sector context, catalyst notes, and risk posture." />
      <ScannerControls {...props} />
      <Panel title="Scanner Stream" icon={<Gauge size={17} />}>
        <div className="scanner-table">
          <div className={`scanner-row header ${showAssetClass ? 'with-asset-class' : ''}`}>
            <span>Symbol</span>
            {showAssetClass ? <span>Type</span> : null}
            <span>Score</span><span>Setup</span><span>Confidence</span><span>Momentum</span><span>Volatility</span><span>Risk</span><span>Provider</span><span>Sell Zone</span>
          </div>
          {props.filteredMarket.map((asset) => {
            const signal = signalBySymbol.get(asset.symbol);
            const score = signal ? Math.round(signal.score) : 0;
            const confidence = signal ? Math.round(signal.confidence * 100) : asset.metadata?.confidence ?? 50;
            return (
              <button
                className={`scanner-row ${showAssetClass ? 'with-asset-class' : ''} ${signal ? riskToneClass(signal.riskScore) : 'risk-neutral'}`}
                key={asset.symbol}
                onClick={() => props.setSelectedSymbol(asset.symbol)}
                style={{ '--score': score, '--confidence': confidence, '--volatility': Math.min(100, Math.round(asset.volatility * 1000)) } as CSSProperties}
                type="button"
              >
                <span>{asset.symbol}</span>
                {showAssetClass ? <span className={`asset-class-pill ${asset.assetClass}`}>{assetClassLabel(asset.assetClass)}</span> : null}
                <span className="numeric score-cell">{signal ? score : '--'}</span>
                <span>{signal?.setupType ?? 'No Paper Setup'}</span>
                <span className="numeric confidence-cell">{signal ? confidence : '--'}%</span>
                <span className={`numeric ${asset.changePct >= 0 ? 'good' : 'bad'}`}>{pct(asset.changePct)}</span>
                <span className="numeric">{pct(asset.volatility)}</span>
                <span>{signal?.risk ?? 'Review data quality'}</span>
                <span>{assetProvider(asset)}</span>
                <span className="sell-zone-cell">{signal ? sellZoneWithPrice(signal) : `Current ${money(asset.price)} / Zone --`}</span>
              </button>
            );
          })}
        </div>
      </Panel>
    </div>
  );
}

function PortfolioPageApp(props: ApplicationProps & { state: AppState }) {
  const { state } = props;
  const [cashDraft, setCashDraft] = useState(String(Math.round(state.account.cash)));
  const [orderSymbol, setOrderSymbol] = useState(props.selectedSymbol);
  const [orderQty, setOrderQty] = useState('1');
  const [orderSide, setOrderSide] = useState<'buy' | 'sell'>('buy');
  const assetMap = new Map(state.market.map((asset) => [asset.symbol, asset]));
  const byAssetClass = state.positions.reduce<Record<string, number>>((acc, position) => {
    const group = position.assetClass || assetMap.get(position.symbol)?.assetClass || 'stock';
    acc[group] = (acc[group] ?? 0) + position.marketValue;
    return acc;
  }, {});

  return (
    <div className="page-stack">
      <AppPageHeader title="Portfolio Console" copy="Holdings, allocation, exposure, performance, diversification, and risk state." />
      <section className="two-column">
        <Panel title="Portfolio Snapshot" icon={<Wallet size={17} />}>
          <div className="metric-strip">
            <MiniMetric label="Realized P/L" value={money(state.core.portfolio.realizedPnl)} tone={state.core.portfolio.realizedPnl >= 0 ? 'good' : 'bad'} />
            <MiniMetric label="Unrealized P/L" value={money(state.core.portfolio.unrealizedPnl)} tone={state.core.portfolio.unrealizedPnl >= 0 ? 'good' : 'bad'} />
            <MiniMetric label="Risk State" value={state.core.portfolio.riskState} tone={state.core.portfolio.riskState === 'Healthy' ? 'good' : 'warn'} />
            <MiniMetric label="Exposure" value={money(state.core.portfolio.exposure)} tone="neutral" />
          </div>
        </Panel>
        <Panel title="Holdings" icon={<BriefcaseBusiness size={17} />}>
          <div className="journal-editor compact-editor">
            <input value={orderSymbol} onChange={(event) => setOrderSymbol(event.target.value.toUpperCase())} placeholder="Symbol" />
            <input value={orderQty} onChange={(event) => setOrderQty(event.target.value)} placeholder="Quantity" />
            <select value={orderSide} onChange={(event) => setOrderSide(event.target.value as 'buy' | 'sell')}>
              <option value="buy">Paper buy</option>
              <option value="sell">Paper sell</option>
            </select>
            <button
              className="solid-link"
              onClick={() => props.action(() => placeOrder(orderSymbol, orderSide, Number(orderQty), 'Portfolio console paper conversion'), 'Paper portfolio order recorded.')}
              type="button"
            >
              Convert to Paper Trade
            </button>
          </div>
          <DataTable
            empty="No open paper positions."
            headers={['Symbol', 'Qty', 'Avg', 'Value', 'P/L']}
            rows={state.positions.map((position) => [
              position.symbol,
              fmt(position.qty, 5),
              money(position.avgPrice),
              money(position.marketValue),
              `${money(position.unrealizedPnl)} (${pct(position.unrealizedPnlPct)})`
            ])}
          />
        </Panel>
        <Panel title="Cash Model" icon={<Database size={17} />}>
          <div className="journal-editor compact-editor">
            <input value={cashDraft} onChange={(event) => setCashDraft(event.target.value)} placeholder="Paper cash" />
            <button className="ghost-link" onClick={() => props.action(() => updatePaperCash(Number(cashDraft)), 'Paper cash model updated.')} type="button">Update Cash</button>
          </div>
          <StatusLine label="Cash" value={money(state.account.cash)} />
          <StatusLine label="Equity" value={money(state.account.equity)} />
          <StatusLine label="Exposure" value={money(state.account.exposure)} />
          <StatusLine label="Cash Allocation" value={pct(state.account.cash / Math.max(state.account.equity, 1))} />
        </Panel>
        <Panel title="Exposure & Diversification" icon={<ShieldCheck size={17} />}>
          <RiskOverview state={state} />
          <div className="allocation-bars">
            {Object.entries(byAssetClass).map(([group, value]) => (
              <div className="allocation-row" key={group}>
                <span>{assetClassLabel(group as AssetClass)}</span>
                <div><i style={{ width: `${Math.min(100, (value / Math.max(state.account.equity, 1)) * 100)}%` }} /></div>
                <strong>{pct(value / Math.max(state.account.equity, 1))}</strong>
              </div>
            ))}
            {!Object.keys(byAssetClass).length ? <p className="muted">No allocation exposure yet.</p> : null}
          </div>
          <div className="warning-list">
            {state.core.portfolio.warnings.length ? state.core.portfolio.warnings.map((warning) => <p key={warning}>{warning}</p>) : <p className="muted">No current portfolio risk warnings.</p>}
          </div>
          <DataTable
            empty="No concentration rows yet."
            headers={['Symbol', 'Weight', 'Value']}
            rows={state.core.portfolio.concentration.slice(0, 6).map((row) => [row.symbol, pct(row.weight), money(row.value)])}
          />
        </Panel>
      </section>
    </div>
  );
}

function SymbolResearchApp(props: ApplicationProps & { state: AppState }) {
  const asset = props.selectedAsset ?? props.state.market[0];
  const signal = props.selectedSignal;
  const research = props.state.core.symbolResearch.find((item) => item.symbol === asset?.symbol);
  const [researchStatus, setResearchStatus] = useState('');
  const [timeline, setTimeline] = useState<ResearchTimelineEvent[]>([]);

  useEffect(() => {
    if (!asset?.symbol) {
      setTimeline([]);
      return;
    }
    getResearchTimeline(asset.symbol)
      .then((result) => setTimeline(result.timeline))
      .catch(() => setTimeline([]));
  }, [asset?.symbol, props.state.generatedAt]);

  async function createSymbolAlert() {
    if (!asset) return;
    try {
      await createAlert({ type: 'price_above', symbol: asset.symbol, threshold: asset.price * 1.03, note: 'Created from symbol research page.' });
      setResearchStatus(`Alert created for ${asset.symbol}.`);
      await props.refresh();
    } catch (error) {
      setResearchStatus(error instanceof Error ? error.message : 'Could not create alert.');
    }
  }

  async function saveSymbolNote() {
    if (!asset) return;
    try {
      await createJournalEntry({ symbol: asset.symbol, entryType: 'research', tags: ['symbol'], note: `${asset.symbol} reviewed from Symbol Research. ${signal?.reason ?? 'No active signal.'}` });
      setResearchStatus(`Journal note saved for ${asset.symbol}.`);
      await props.refresh();
    } catch (error) {
      setResearchStatus(error instanceof Error ? error.message : 'Could not save note.');
    }
  }

  async function generateSymbolReport() {
    if (!asset) return;
    try {
      await generateReport('symbol', asset.symbol);
      setResearchStatus(`${asset.symbol} research pack generated.`);
      await props.refresh();
    } catch (error) {
      setResearchStatus(error instanceof Error ? error.message : 'Could not generate report.');
    }
  }

  return (
    <div className="symbol-layout">
      <section className="chart-workspace">
        <AppPageHeader title={`${asset?.symbol ?? 'Symbol'} Research`} copy={asset?.name ?? 'Select a symbol from the scanner.'} />
        <div className="scanner-controls-bar">
          <button className="solid-link" onClick={() => props.paperOrder('buy')} disabled={!signal} type="button">Create Paper Plan</button>
          <button className="ghost-link" onClick={() => void createSymbolAlert()} type="button">Create Alert</button>
          <button className="ghost-link" onClick={() => void saveSymbolNote()} type="button">Save Note</button>
          <button className="ghost-link" onClick={() => void generateSymbolReport()} type="button">Generate Report</button>
        </div>
        {researchStatus ? <p className="auth-status">{researchStatus}</p> : null}
        <div className="chart-panel">
          <MarketChart asset={asset} signal={signal} />
        </div>
        <section className="research-workspace-grid">
          <Panel title="Overview" icon={<LineChart size={17} />}>
            <StatusLine label="Company" value={asset?.name ?? '--'} />
            <StatusLine label="Asset Type" value={asset ? assetClassLabel(asset.assetClass) : '--'} />
            <StatusLine label="Current Price" value={asset ? money(asset.price) : '--'} />
            <StatusLine label="Provider" value={asset ? assetProvider(asset) : '--'} />
            <StatusLine label="Freshness" value={asset ? assetFreshness(asset) : '--'} />
          </Panel>
          <Panel title="Signals" icon={<Gauge size={17} />}>
            <StatusLine label="Momentum Score" value={signal ? String(Math.round(signal.momentumScore)) : '--'} />
            <StatusLine label="Conviction" value={signal ? `${Math.round(signal.confidence * 100)}%` : '--'} />
            <StatusLine label="Trend" value={signal ? `${Math.round(signal.components.trend)} / 100` : '--'} />
            <StatusLine label="Volatility" value={asset ? pct(asset.volatility) : '--'} />
            <StatusLine label="Risk Score" value={signal ? String(Math.round(signal.riskScore)) : '--'} />
          </Panel>
          <Panel title="Paper Plan" icon={<FileText size={17} />}>
            <StatusLine label="Entry" value={signal ? money(signal.entry) : '--'} />
            <StatusLine label="Stop" value={signal ? money(signal.stop) : '--'} />
            <StatusLine label="Target 1" value={signal ? money(signal.target1) : '--'} />
            <StatusLine label="Target 2" value={signal ? money(signal.target2) : '--'} />
            <StatusLine label="Sell Zone" value={signal ? sellZoneWithPrice(signal) : '--'} />
            <p className="analysis-copy">{signal?.invalidation ?? 'No paper setup currently meets scanner confirmation.'}</p>
          </Panel>
          <Panel title="ETF Exposure" icon={<Database size={17} />}>
            <p className="analysis-copy">ETF overlap is staged for AuralithCore provider extraction. Current proxy uses asset class, provider quality, and portfolio concentration.</p>
            <StatusLine label="Portfolio Weight" value={stateWeightForSymbol(props.state, asset?.symbol)} />
            <StatusLine label="Coverage" value={asset?.assetClass === 'stock' ? 'Stock universe' : 'Crypto universe'} />
          </Panel>
        </section>
      </section>
      <aside className="research-sidebar">
        <Panel title="AI Analysis" icon={<BrainCircuit size={17} />}>
          <p className="analysis-copy">{signal?.reason ?? 'Research context will appear after selecting a symbol with a scanner signal.'}</p>
          <StatusLine label="Setup" value={signal?.setupType ?? '--'} />
          <StatusLine label="Asset Type" value={asset ? assetClassLabel(asset.assetClass) : '--'} />
          <StatusLine label="Current Price" value={asset ? money(asset.price) : '--'} />
          <StatusLine label="Entry" value={signal ? money(signal.entry) : '--'} />
          <StatusLine label="Stop" value={signal ? money(signal.stop) : '--'} />
          <StatusLine label="Target 1" value={signal ? money(signal.target1) : '--'} />
          <StatusLine label="Target 2" value={signal ? money(signal.target2) : '--'} />
          <StatusLine label="Sell Zone" value={signal ? sellZoneWithPrice(signal) : '--'} />
          <StatusLine label="Trailing Stop" value={signal ? money(signal.trailingStop) : '--'} />
          <StatusLine label="Risk/Reward" value={signal ? `${signal.riskReward.toFixed(1)}R` : '--'} />
          <StatusLine label="Risk" value={signal?.risk ?? 'Review'} />
          {signal?.exitPlan ? <p className="analysis-copy">{signal.exitPlan}</p> : null}
          {signal?.invalidation ? <p className="analysis-copy">{signal.invalidation}</p> : null}
          <StatusLine label="Freshness" value={asset ? assetFreshness(asset) : '--'} />
          <StatusLine label="Source" value={asset ? assetProvider(asset) : '--'} />
          <StatusLine label="Data Confidence" value={asset?.metadata ? `${asset.metadata.confidence}%` : '--'} />
        </Panel>
        <Panel title="Research Timeline" icon={<History size={17} />}>
          <TimelineList events={timeline} />
        </Panel>
        <Panel title="Fundamentals & Events" icon={<Newspaper size={17} />}>
          {(research?.fundamentals ?? []).map((item) => <StatusLine key={item.label} label={item.label} value={item.value} />)}
          {(research?.earnings ?? []).map((item) => <StatusLine key={item.label} label={item.label} value={item.value} />)}
        </Panel>
        <Panel title="News & SEC" icon={<Newspaper size={17} />}>
          {(research?.news ?? []).map((item) => <p className="analysis-copy" key={item.title}>{item.title} <span className="muted">/ {item.source}</span></p>)}
          {(research?.secLinks ?? []).map((item) => <a className="text-link" href={item.url} key={item.url} rel="noreferrer" target="_blank">{item.title}</a>)}
        </Panel>
        <Panel title="Journal" icon={<BookOpen size={17} />}>
          {research?.journalNotes.length ? research.journalNotes.map((entry) => <p className="analysis-copy" key={entry.id}>{entry.note}</p>) : <p className="muted">No symbol notes linked in the web cockpit yet.</p>}
        </Panel>
        <Panel title="Alerts" icon={<Bell size={17} />}>
          {research?.alerts.length ? <AlertList alerts={research.alerts} /> : <p className="muted">No symbol alerts linked yet.</p>}
        </Panel>
      </aside>
    </div>
  );
}

function StrategyPageApp(props: ApplicationProps & { state: AppState }) {
  const { state } = props;
  const [strategies, setStrategies] = useState<StrategyRecord[]>([]);
  const [strategyName, setStrategyName] = useState('Momentum Confirmation');
  const [strategyRules, setStrategyRules] = useState('Price above 20 EMA\nRSI between 50 and 70\nVolume above average\nRisk/reward >= 2R');
  const [strategyStatus, setStrategyStatus] = useState('');
  const activeSignals = [...state.signals].sort((a, b) => b.score - a.score).slice(0, 6);

  useEffect(() => {
    listStrategies()
      .then((result) => setStrategies(result.strategies))
      .catch(() => setStrategies([]));
  }, [state.generatedAt]);

  async function saveCurrentStrategy() {
    try {
      const result = await saveStrategy({
        name: strategyName,
        description: 'Web strategy builder paper setup.',
        preset: 'web-builder',
        enabled: false,
        rules: strategyRules.split('\n').map((rule) => rule.trim()).filter(Boolean)
      });
      setStrategies((current) => [result.strategy, ...current.filter((item) => item.id !== result.strategy.id)]);
      setStrategyStatus(`${result.strategy.name} saved.`);
      await props.refresh();
    } catch (error) {
      setStrategyStatus(error instanceof Error ? error.message : 'Could not save strategy.');
    }
  }

  async function runStrategyBacktest() {
    setStrategyStatus('Running paper backtest summary...');
    try {
      const result = await runBacktest();
      setStrategyStatus(`Backtest complete: ${result.totalTrades ?? result.trades?.length ?? 0} simulated trades reviewed.`);
    } catch (error) {
      setStrategyStatus(error instanceof Error ? error.message : 'Backtest failed.');
    }
  }

  return (
    <div className="page-stack">
      <AppPageHeader title="Strategy Lab" copy="Paper strategy design, backtests, scoring logic, and performance review." />
      <section className="two-column">
        <Panel title="Rule Builder" icon={<BarChart3 size={17} />}>
          <div className="journal-editor">
            <input value={strategyName} onChange={(event) => setStrategyName(event.target.value)} placeholder="Strategy name" />
            <textarea value={strategyRules} onChange={(event) => setStrategyRules(event.target.value)} placeholder="One rule per line" />
            <div className="scanner-controls-bar">
              <button className="solid-link" onClick={() => void saveCurrentStrategy()} type="button">Save Strategy</button>
              <button className="ghost-link" onClick={() => void runStrategyBacktest()} type="button">Run Backtest</button>
            </div>
            {strategyStatus ? <p className="auth-status">{strategyStatus}</p> : null}
          </div>
        </Panel>
        <Panel title="Active Paper Signals" icon={<Gauge size={17} />}>
          <DataTable
            empty="No active scanner signals."
            headers={['Symbol', 'Score', 'Setup', 'Risk/Reward']}
            rows={activeSignals.map((signal) => [signal.symbol, String(Math.round(signal.score)), signal.setupType, `${signal.riskReward.toFixed(1)}R`])}
          />
        </Panel>
      </section>
      <section className="two-column">
        <Panel title="Saved Strategies" icon={<Database size={17} />}>
          <div className="audit-list">
            {strategies.map((strategy) => (
              <article className="audit-row info" key={strategy.id}>
                <strong>{strategy.name} / {strategy.enabled ? 'Enabled' : 'Paused'}</strong>
                <span>{strategy.description}</span>
                <small>{strategy.rules.join(' / ')}</small>
                <div className="rail-badge-row">
                  <button className="ghost-link" onClick={() => { setStrategyName(strategy.name); setStrategyRules(strategy.rules.join('\n')); }} type="button">Edit</button>
                  <button className="ghost-link" onClick={() => void deleteStrategy(strategy.id).then(() => setStrategies((items) => items.filter((item) => item.id !== strategy.id)))} type="button">Delete</button>
                </div>
              </article>
            ))}
          </div>
        </Panel>
        <Panel title="Paper Simulation Preview" icon={<Play size={17} />}>
          <StatusLine label="Signals" value={String(state.signals.length)} />
          <StatusLine label="Eligible Presets" value={String(strategies.length)} />
          <StatusLine label="Manual Confirmation" value="Required" />
          <StatusLine label="Execution" value="Paper only" />
          <p className="analysis-copy">Strategies create paper plans and backtest summaries. Live automation remains outside the web safety boundary.</p>
        </Panel>
      </section>
      <Panel title="Strategy Performance" icon={<BarChart3 size={17} />}>
        <DataTable
          empty="No strategy performance data yet."
          headers={['Strategy', 'Trades', 'Wins', 'Total P/L', 'Avg P/L', 'Win Rate']}
          rows={state.strategyPerformance.map((perf) => [
            perf.strategy,
            String(perf.total_trades),
            String(perf.winning_trades),
            money(perf.total_pnl),
            money(perf.avg_pnl),
            pct(perf.win_rate)
          ])}
        />
      </Panel>
    </div>
  );
}

function ReportsPageApp(props: ApplicationProps & { state: AppState }) {
  const { state } = props;
  const [reportStatus, setReportStatus] = useState('');
  const [savedReports, setSavedReports] = useState<SavedReport[]>([]);
  const [selectedReportId, setSelectedReportId] = useState('');
  const [eodReview, setEodReview] = useState<ResearchWorkflow | null>(null);
  const reportKinds: Array<[SavedReport['kind'], string]> = [
    ['morning', 'Morning Briefing'],
    ['eod', 'End-of-Day Review'],
    ['weekly', 'Weekly Portfolio Review'],
    ['risk', 'Risk Report'],
    ['strategy', 'Strategy Report'],
    ['symbol', 'Symbol Research Pack']
  ];

  useEffect(() => {
    listReports()
      .then((result) => {
        setSavedReports(result.reports);
        if (!selectedReportId && result.reports[0]) setSelectedReportId(result.reports[0].id);
      })
      .catch(() => setSavedReports([]));
    getEndOfDayReview()
      .then((result) => setEodReview(result.workflow))
      .catch(() => setEodReview(null));
  }, [state.generatedAt]);

  async function createReport(kind: SavedReport['kind']) {
    setReportStatus('Generating report...');
    try {
      const result = await generateReport(kind, kind === 'symbol' ? props.selectedSymbol : undefined);
      setReportStatus(`${result.report.title} saved with ${result.report.freshness} provider context.`);
      setSavedReports((current) => [result.report, ...current.filter((report) => report.id !== result.report.id)]);
      setSelectedReportId(result.report.id);
      await props.refresh();
    } catch (error) {
      setReportStatus(error instanceof Error ? error.message : 'Could not generate report.');
    }
  }

  return (
    <div className="page-stack">
      <AppPageHeader title="Reports" copy="Executive-level research outputs with source, freshness, and risk context." />
      <Panel title="Generate Research Output" icon={<FileText size={17} />}>
        <div className="scanner-controls-bar">
          {reportKinds.map(([kind, label]) => (
            <button className="ghost-link" key={kind} onClick={() => void createReport(kind)} type="button">
              <Plus size={16} /> {label}
            </button>
          ))}
        </div>
        {reportStatus ? <p className="auth-status">{reportStatus}</p> : null}
      </Panel>
      {eodReview ? (
        <Panel title="End-of-Day Workflow Preview" icon={<History size={17} />}>
          <p className="analysis-copy">{eodReview.summary}</p>
          <div className="scanner-controls-bar">
            {eodReview.actionChecklist.slice(0, 3).map((item) => (
              <span className={`freshness-badge ${item.severity === 'positive' ? 'fresh' : item.severity === 'critical' ? 'stale' : item.severity === 'warning' ? 'fallback' : 'delayed'}`} key={item.label}>
                <i />{item.label}
              </span>
            ))}
          </div>
        </Panel>
      ) : null}
      <div className="report-grid">
        {state.core.reports.map((report) => (
          <ReportCard
            key={report.id}
            title={report.title}
            copy={`${report.summary} / ${report.status} / ${report.source}`}
          />
        ))}
      </div>
      <section className="two-column report-preview-layout">
        <Panel title="Saved Report Library" icon={<Database size={17} />}>
          <div className="audit-list">
            {savedReports.map((report) => (
              <button className={`audit-row info ${selectedReportId === report.id ? 'selected' : ''}`} key={report.id} onClick={() => setSelectedReportId(report.id)} type="button">
                <strong>{report.title} / {report.kind}</strong>
                <span>{report.summary} / {report.freshness} / {formatTime(report.createdAt)}</span>
              </button>
            ))}
            {!savedReports.length ? <p className="muted">No saved reports yet. Generate one above to build the archive.</p> : null}
          </div>
        </Panel>
        <Panel title="Report Preview" icon={<FileText size={17} />}>
          {savedReports.find((report) => report.id === selectedReportId) ? (
            <>
              <iframe
                className="report-preview-frame"
                srcDoc={savedReports.find((report) => report.id === selectedReportId)?.html}
                title="Auralith report preview"
              />
              <a className="ghost-link full" href={`/api/reports/${encodeURIComponent(selectedReportId)}/export.html`} target="_blank" rel="noreferrer">Export HTML</a>
              <p className="muted">PDF export hook is staged after HTML report rendering stabilizes.</p>
            </>
          ) : (
            <p className="muted">Select a saved report to preview executive output, source metadata, and paper-only disclosure.</p>
          )}
        </Panel>
      </section>
    </div>
  );
}

function JournalPageApp(props: ApplicationProps & { state: AppState }) {
  const { state } = props;
  const [symbol, setSymbol] = useState(props.selectedSymbol);
  const [tag, setTag] = useState('research');
  const [note, setNote] = useState('');
  const [mistake, setMistake] = useState('');
  const [lesson, setLesson] = useState('');
  const [grade, setGrade] = useState('');
  const [journalStatus, setJournalStatus] = useState('');

  async function saveJournal() {
    if (!note.trim()) {
      setJournalStatus('Add a note before saving the journal entry.');
      return;
    }
    try {
      await createJournalEntry({
        symbol,
        entryType: mistake.trim() ? 'mistake' : 'research',
        tags: tag.split(',').map((item) => item.trim()).filter(Boolean),
        note,
        mistake,
        lesson,
        grade
      });
      setNote('');
      setMistake('');
      setLesson('');
      setJournalStatus('Journal entry saved to Auralith SQLite.');
      await props.refresh();
    } catch (error) {
      setJournalStatus(error instanceof Error ? error.message : 'Could not save journal entry.');
    }
  }

  return (
    <div className="page-stack">
      <AppPageHeader title="Journal" copy="Reflective research notes, trade reviews, lessons, tags, and behavioral patterns." />
      <Panel title="Research Notes" icon={<BookOpen size={17} />}>
        <div className="journal-editor">
          <input placeholder="Symbol" value={symbol} onChange={(event) => setSymbol(event.target.value.toUpperCase())} />
          <input placeholder="Tags" value={tag} onChange={(event) => setTag(event.target.value)} />
          <textarea placeholder="Write a research note, lesson, or review." value={note} onChange={(event) => setNote(event.target.value)} />
          <input placeholder="Mistake or behavior pattern (optional)" value={mistake} onChange={(event) => setMistake(event.target.value)} />
          <input placeholder="Lesson learned (optional)" value={lesson} onChange={(event) => setLesson(event.target.value)} />
          <input placeholder="Grade (optional)" value={grade} onChange={(event) => setGrade(event.target.value)} />
          <button className="solid-link" onClick={() => void saveJournal()} type="button">Save Note</button>
          {journalStatus ? <p className="auth-status">{journalStatus}</p> : null}
        </div>
      </Panel>
      <Panel title="Recent Audit Context" icon={<History size={17} />}>
        <AuditList events={state.audit.slice(0, 6)} empty="No recent events." />
      </Panel>
      <Panel title="Core Journal Stream" icon={<BookOpen size={17} />}>
        <JournalList entries={state.core.journal} />
      </Panel>
    </div>
  );
}

function AlertsPageApp(props: ApplicationProps & { state: AppState }) {
  const { state } = props;
  const [alertType, setAlertType] = useState<AlertType>('price_above');
  const [alertSymbol, setAlertSymbol] = useState(props.selectedSymbol);
  const [alertThreshold, setAlertThreshold] = useState('');
  const [alertNote, setAlertNote] = useState('');
  const [alertStatus, setAlertStatus] = useState('');
  const [notifications, setNotifications] = useState<NotificationRecord[]>([]);

  useEffect(() => {
    listNotifications()
      .then((result) => setNotifications(result.notifications))
      .catch(() => setNotifications([]));
  }, [state.generatedAt]);

  async function saveAlert() {
    try {
      const numeric = Number(alertThreshold);
      await createAlert({
        type: alertType,
        symbol: alertSymbol,
        threshold: ['price_above', 'price_below', 'risk_threshold'].includes(alertType) && Number.isFinite(numeric) ? numeric : undefined,
        percentMove: alertType === 'percent_move' && Number.isFinite(numeric) ? numeric : undefined,
        note: alertNote
      });
      if (props.notificationPermission === 'granted' && typeof Notification !== 'undefined') {
        new Notification('Auralith alert created', { body: `${alertSymbol || 'Portfolio'} ${alertType.replace(/_/g, ' ')}` });
      }
      setAlertStatus('Alert saved to Auralith SQLite.');
      await props.refresh();
    } catch (error) {
      setAlertStatus(error instanceof Error ? error.message : 'Could not save alert.');
    }
  }

  async function alertAction(task: Promise<unknown>, success: string) {
    try {
      await task;
      setAlertStatus(success);
      await props.refresh();
    } catch (error) {
      setAlertStatus(error instanceof Error ? error.message : 'Alert action failed.');
    }
  }

  return (
    <div className="page-stack">
      <AppPageHeader title="Alerts" copy="Triggered alerts, stale data warnings, provider degradation, risk guardrails, and paper order events." />
      <section className="two-column">
        <Panel title="Create Alert" icon={<Bell size={17} />}>
          <div className="journal-editor">
            <select value={alertType} onChange={(event) => setAlertType(event.target.value as AlertType)}>
              <option value="price_above">Price above</option>
              <option value="price_below">Price below</option>
              <option value="percent_move">Percent move</option>
              <option value="provider_stale">Provider stale</option>
              <option value="risk_threshold">Risk threshold</option>
              <option value="paper_order_update">Paper order update</option>
            </select>
            <input placeholder="Symbol" value={alertSymbol} onChange={(event) => setAlertSymbol(event.target.value.toUpperCase())} />
            <input placeholder="Threshold or percent move" value={alertThreshold} onChange={(event) => setAlertThreshold(event.target.value)} />
            <input placeholder="Note" value={alertNote} onChange={(event) => setAlertNote(event.target.value)} />
            <button className="solid-link full" onClick={() => void saveAlert()} type="button">
              Create Alert
            </button>
            {alertStatus ? <p className="auth-status">{alertStatus}</p> : null}
          </div>
        </Panel>
        <Panel title="Notification Readiness" icon={<Bell size={17} />}>
          <StatusLine label="Browser Permission" value={props.notificationPermission} />
          <StatusLine label="Delivery Types" value="Alerts / reports / stale providers / risk / paper orders" />
          <StatusLine label="Offline Inbox" value={props.usingCachedState ? 'Saved session available' : 'Live session cache ready'} />
          <button className="solid-link full" onClick={() => void props.requestNotifications()} type="button">
            Enable Notifications
          </button>
        </Panel>
        <Panel title="Alert Actions" icon={<Smartphone size={17} />}>
          <StatusLine label="Desktop Browser" value="Dense terminal layout" />
          <StatusLine label="Tablet" value="Split research panels" />
          <StatusLine label="Phone" value="Focused monitoring cards" />
          <StatusLine label="Push Shell" value="Service worker ready" />
        </Panel>
      </section>
      <Panel title="Notification Center" icon={<Bell size={17} />}>
        <AlertList alerts={state.core.alerts} />
        <div className="alert-actions-list">
          {state.core.alerts.slice(0, 8).map((alert) => (
            <div className="status-line" key={`actions-${alert.id}`}>
              <span>{alert.title}</span>
              <strong>
                <button className="ghost-link" onClick={() => void alertAction(acknowledgeAlert(alert.id), 'Alert acknowledged.')} type="button">Ack</button>
                <button className="ghost-link" onClick={() => void alertAction(snoozeAlert(alert.id), 'Alert snoozed.')} type="button">Snooze</button>
                <button className="ghost-link" onClick={() => void alertAction(updateAlert(alert.id, { enabled: false } as any), 'Alert disabled.')} type="button">Disable</button>
                <button className="ghost-link" onClick={() => void alertAction(deleteAlert(alert.id), 'Alert deleted.')} type="button">Delete</button>
              </strong>
            </div>
          ))}
        </div>
      </Panel>
      <Panel title="Notification Inbox" icon={<Bell size={17} />}>
        <div className="audit-list">
          {notifications.map((notification) => (
            <article className={`audit-row ${notification.severity}`} key={notification.id}>
              <strong>{notification.title}{notification.relatedSymbol ? ` / ${notification.relatedSymbol}` : ''}</strong>
              <span>{notification.detail} / {notification.source} / {formatTime(notification.createdAt)}</span>
              <button className="ghost-link" onClick={() => void markNotificationRead(notification.id).then(() => setNotifications((items) => items.map((item) => item.id === notification.id ? { ...item, readAt: new Date().toISOString() } : item)))} type="button">
                {notification.readAt ? 'Read' : 'Mark Read'}
              </button>
            </article>
          ))}
          {!notifications.length ? <p className="muted">No web notifications yet.</p> : null}
        </div>
      </Panel>
    </div>
  );
}

function DiagnosticsPageApp(props: ApplicationProps & { state: AppState }) {
  const [diagnostics, setDiagnostics] = useState<ProviderDiagnostics[]>([]);
  const [dataQuality, setDataQuality] = useState<DataQualitySnapshot | null>(null);
  const [liveStatus, setLiveStatus] = useState('Connecting to live provider channel...');
  const [lastLiveEvent, setLastLiveEvent] = useState('No live events yet.');

  useEffect(() => {
    getProviderDiagnostics()
      .then((result) => {
        setDiagnostics(result.diagnostics);
        setDataQuality(result.dataQuality);
      })
      .catch((error) => {
        setLiveStatus(error instanceof Error ? error.message : 'Provider diagnostics unavailable.');
      });
  }, [props.state.generatedAt]);

  useEffect(() => {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const socket = new WebSocket(`${protocol}//${window.location.host}/api/live`);
    socket.addEventListener('open', () => setLiveStatus('Live provider channel connected.'));
    socket.addEventListener('message', (event) => {
      try {
        const body = JSON.parse(event.data) as { type?: string; generatedAt?: string };
        setLastLiveEvent(`${body.type ?? 'event'} / ${body.generatedAt ? formatTime(body.generatedAt) : 'now'}`);
      } catch {
        setLastLiveEvent('Provider event received.');
      }
    });
    socket.addEventListener('close', () => setLiveStatus('Live provider channel closed. The next refresh will reconnect.'));
    socket.addEventListener('error', () => setLiveStatus('Live provider channel degraded.'));
    return () => socket.close();
  }, []);

  const quality = dataQuality ?? {
    counts: { live: 0, delayed: 0, stale: 0, fallback: 0, failed: 0 },
    confidence: props.state.core.providerHealth.confidence,
    warnings: [],
    generatedAt: props.state.generatedAt
  };

  return (
    <div className="page-stack">
      <AppPageHeader title="Provider Diagnostics" copy="Health, freshness, fallback usage, cache behavior, candle coverage, and live update state for the Auralith market data pipeline." />
      <section className="core-status-strip">
        <MiniMetric label="Data Confidence" value={`${quality.confidence}%`} tone={quality.confidence >= 75 ? 'good' : quality.confidence >= 55 ? 'warn' : 'bad'} />
        <MiniMetric label="Live" value={String(quality.counts.live)} tone={quality.counts.live ? 'good' : 'neutral'} />
        <MiniMetric label="Delayed" value={String(quality.counts.delayed)} tone={quality.counts.delayed ? 'warn' : 'neutral'} />
        <MiniMetric label="Stale" value={String(quality.counts.stale)} tone={quality.counts.stale ? 'bad' : 'neutral'} />
        <MiniMetric label="Fallback" value={String(quality.counts.fallback)} tone={quality.counts.fallback ? 'warn' : 'neutral'} />
        <MiniMetric label="Failed" value={String(quality.counts.failed)} tone={quality.counts.failed ? 'bad' : 'neutral'} />
        <MiniMetric label="Quotes" value={String(props.state.market.length)} tone="neutral" />
        <MiniMetric label="Live Channel" value={lastLiveEvent} tone={liveStatus.includes('connected') ? 'good' : 'warn'} />
      </section>
      {quality.warnings.length ? (
        <section className="data-quality-banner">
          <div>
            <strong>Research confidence reduced</strong>
            <span>{quality.warnings.join(' ')}</span>
          </div>
          <button className="ghost-link" type="button" onClick={() => void props.refresh()}>Refresh Providers</button>
        </section>
      ) : null}
      <section className="two-column">
        <Panel title="Provider Health" icon={<Database size={17} />} variant="hero">
          <DataTable
            empty="No provider diagnostics have been collected yet."
            headers={['Provider', 'Tier', 'Status', 'Capabilities', 'Requests', 'Failures', 'Cache', 'Fallback', 'Latency', 'Rate Limit']}
            rows={diagnostics.map((provider) => [
              provider.name,
              provider.tier,
              provider.status,
              provider.capabilities.join(', '),
              String(provider.requestCount),
              String(provider.failureCount),
              String(provider.cacheHits),
              String(provider.fallbackCount),
              `${provider.averageLatencyMs}ms`,
              provider.rateLimitState
            ])}
          />
        </Panel>
        <Panel title="Live Reliability State" icon={<Activity size={17} />} variant="active">
          <StatusLine label="WebSocket" value={liveStatus} />
          <StatusLine label="Last Event" value={lastLiveEvent} />
          <StatusLine label="Generated" value={formatTime(quality.generatedAt)} />
          <StatusLine label="Provider Source" value={props.state.core.providerHealth.source} />
          <StatusLine label="Provider Freshness" value={props.state.core.providerHealth.freshness} />
          <ConfidenceMeter label="Current Research Confidence" value={quality.confidence} />
          <button className="solid-link full" onClick={() => void props.refresh()} disabled={props.busy} type="button">
            <RefreshCw size={16} /> Refresh Provider Pipeline
          </button>
        </Panel>
      </section>
      <Panel title="Quote Quality Snapshot" icon={<LineChart size={17} />}>
        <DataTable
          empty="No quote metadata available."
          headers={['Symbol', 'Asset', 'Provider', 'Freshness', 'Confidence', 'Fetched', 'State']}
          rows={props.state.market.slice(0, 80).map((asset) => [
            asset.symbol,
            assetClassLabel(asset.assetClass),
            assetProvider(asset),
            assetFreshness(asset),
            `${asset.metadata?.confidence ?? 50}%`,
            formatTime(asset.metadata?.fetchedAt ?? asset.updatedAt),
            asset.metadata?.error ? `Error: ${asset.metadata.error}` : asset.metadata?.fallback ? 'Fallback/Demo' : asset.metadata?.delayed ? 'Delayed' : 'Usable'
          ])}
        />
      </Panel>
    </div>
  );
}

function SettingsPageApp(props: ApplicationProps & { state: AppState }) {
  const state = props.state;
  const [layouts, setLayouts] = useState<WorkspaceLayoutRecord[]>([]);
  const [layoutStatus, setLayoutStatus] = useState('');
  const [platformHealth, setPlatformHealth] = useState<PlatformHealthSnapshot | null>(null);
  const [platformStatus, setPlatformStatus] = useState('');

  useEffect(() => {
    listLayouts()
      .then((result) => setLayouts(result.layouts))
      .catch(() => setLayouts([]));
    getPlatformHealth()
      .then((result) => setPlatformHealth(result.health))
      .catch(() => setPlatformHealth(null));
  }, [state.generatedAt]);

  async function createLayout(mode: WorkspaceLayoutRecord['mode'], name: string) {
    try {
      const result = await saveLayout({
        name,
        mode,
        config: {
          route: props.route,
          selectedSymbol: props.selectedSymbol,
          marketSort: props.marketSort,
          assetFilter: props.assetFilter
        }
      });
      setLayouts((current) => [result.layout, ...current.filter((layout) => layout.id !== result.layout.id)]);
      setLayoutStatus(`${result.layout.name} layout saved.`);
    } catch (error) {
      setLayoutStatus(error instanceof Error ? error.message : 'Could not save layout.');
    }
  }

  async function validateBackup() {
    setPlatformStatus('Validating backup snapshot...');
    try {
      const result = await validatePlatformBackup();
      setPlatformStatus(result.backup.ok
        ? `Backup validated: ${result.backup.backupPath ?? 'snapshot'}`
        : `Backup validation failed: ${result.backup.error ?? 'unknown error'}`);
      const health = await getPlatformHealth();
      setPlatformHealth(health.health);
    } catch (error) {
      setPlatformStatus(error instanceof Error ? error.message : 'Could not validate backup.');
    }
  }

  return (
    <div className="page-stack">
      <AppPageHeader title="Settings" copy="Account, providers, notifications, layouts, AI behavior, risk, appearance, storage, security, and backups." />
      {platformHealth ? (
        <Panel title="Platform Maturity" icon={<ShieldCheck size={17} />} variant="active">
          <div className="metric-strip">
            <MiniMetric label="Health" value={platformHealth.status} tone={platformHealth.status === 'healthy' ? 'good' : platformHealth.status === 'degraded' ? 'warn' : 'bad'} />
            <MiniMetric label="DB Integrity" value={platformHealth.database.integrity} tone={platformHealth.database.integrity === 'ok' ? 'good' : 'bad'} />
            <MiniMetric label="Trust" value={`${platformHealth.trust.confidence}%`} tone={platformHealth.trust.confidence >= 75 ? 'good' : platformHealth.trust.confidence >= 55 ? 'warn' : 'bad'} />
            <MiniMetric label="Safe Mode" value={platformHealth.safeMode ? 'ON' : 'Off'} tone={platformHealth.safeMode ? 'warn' : 'good'} />
          </div>
          <div className="platform-health-grid">
            <StatusLine label="Database" value={`${bytes(platformHealth.database.sizeBytes)} / ${platformHealth.database.walMode}`} />
            <StatusLine label="Continuity" value={`${platformHealth.continuity.journalEntries} notes / ${platformHealth.continuity.reports} reports / ${platformHealth.continuity.paperOrders} orders`} />
            <StatusLine label="Provider Reliability" value={`${platformHealth.providers.failed} failed / ${platformHealth.providers.degraded} degraded / ${platformHealth.providers.cacheHits} cache hits`} />
            <StatusLine label="Reduced Quality" value={`${platformHealth.trust.fallbackSymbols} fallback / ${platformHealth.trust.staleSymbols} stale / ${platformHealth.trust.failedSymbols} failed`} />
          </div>
          <div className="warning-list">
            {platformHealth.recommendations.map((recommendation) => <p key={recommendation}>{recommendation}</p>)}
          </div>
          <div className="scanner-controls-bar">
            <button className="solid-link" onClick={() => void validateBackup()} type="button">Validate Backup Snapshot</button>
            <button className="ghost-link" onClick={() => void getPlatformHealth().then((result) => setPlatformHealth(result.health))} type="button">Refresh Health</button>
          </div>
          {platformStatus ? <p className="auth-status">{platformStatus}</p> : null}
        </Panel>
      ) : null}
      <section className="two-column">
        <Panel title="Risk Controls" icon={<ShieldCheck size={17} />}>
          <RiskSlider label="Min Confidence" value={state.settings.minConfidence} min={0.1} max={0.99} step={0.01} format={pct} onChange={(value) => props.action(() => updateSettings({ minConfidence: value }), 'Confidence threshold updated.')} />
          <RiskSlider label="Max Position" value={state.settings.maxPositionPct} min={0.01} max={0.5} step={0.01} format={pct} onChange={(value) => props.action(() => updateSettings({ maxPositionPct: value }), 'Position limit updated.')} />
          <RiskSlider label="Daily Loss Limit" value={state.settings.dailyLossLimitPct} min={0.005} max={0.15} step={0.005} format={pct} onChange={(value) => props.action(() => updateSettings({ dailyLossLimitPct: value }), 'Daily loss limit updated.')} />
          <button className="ghost-link full" onClick={() => props.action(resetPaperAccount, 'Paper account reset.')} disabled={props.busy} type="button">
            <RotateCcw size={16} /> Reset Paper Account
          </button>
        </Panel>
        <Panel title="Safety State" icon={<Lock size={17} />}>
          <StatusLine label="Mode" value={state.settings.mode} />
          <StatusLine label="Live Trading Locked" value={state.settings.liveTradingLocked ? 'Yes' : 'No'} />
          <StatusLine label="Manual Confirmation" value={state.settings.manualConfirmationRequired ? 'Required' : 'Review'} />
          <StatusLine label="Provider Freshness" value={`${state.core.providerHealth.source} / ${state.core.providerHealth.freshness}`} />
          <StatusLine label="Shared Core" value={state.core.platform.sharedCore} />
        </Panel>
      </section>
      <section className="three-column">
        <Panel title="Account Sync" icon={<Database size={17} />}>
          <StatusLine label="Watchlists" value="Core contract" />
          <StatusLine label="Reports" value="Core contract" />
          <StatusLine label="Alerts" value="Core contract" />
          <StatusLine label="Portfolio" value="Core contract" />
          <StatusLine label="Journal" value="Core contract" />
          <StatusLine label="Settings" value="Saved web session" />
        </Panel>
        <Panel title="Security Hardening" icon={<ShieldCheck size={17} />}>
          <StatusLine label="2FA" value="Branded setup screen" />
          <StatusLine label="Sessions" value="Device history screen" />
          <StatusLine label="Provider Keys" value="Encryption boundary planned" />
          <StatusLine label="Audit Logs" value={`${state.audit.length} visible events`} />
          <StatusLine label="Rate Limits" value="Server policy boundary" />
          <StatusLine label="CSRF" value="Secure-cookie phase" />
        </Panel>
        <Panel title="Release Polish" icon={<FileText size={17} />}>
          <StatusLine label="Installable PWA" value="Manifest + worker" />
          <StatusLine label="Offline Shell" value={props.usingCachedState ? 'Active' : 'Ready'} />
          <StatusLine label="Docs" value="Product docs page" />
          <StatusLine label="Changelog" value="Maintained" />
          <StatusLine label="Support" value="Support page" />
          <StatusLine label="Tooltips" value="Native titles + status labels" />
        </Panel>
      </section>
      <Panel title="Workspace Layouts" icon={<PanelLeft size={17} />}>
        <div className="scanner-controls-bar">
          {[
            ['daily', 'Daily Dashboard'],
            ['market-open', 'Market Open'],
            ['research', 'Research'],
            ['portfolio-review', 'Portfolio Review'],
            ['strategy-testing', 'Strategy Testing'],
            ['mobile-compact', 'Mobile Compact']
          ].map(([mode, name]) => (
            <button className="ghost-link" key={mode} onClick={() => void createLayout(mode as WorkspaceLayoutRecord['mode'], name)} type="button">
              Save {name}
            </button>
          ))}
        </div>
        {layoutStatus ? <p className="auth-status">{layoutStatus}</p> : null}
        <DataTable
          empty="No saved layouts yet."
          headers={['Layout', 'Mode', 'Updated']}
          rows={layouts.map((layout) => [layout.name, layout.mode, formatTime(layout.updatedAt)])}
        />
      </Panel>
    </div>
  );
}

function ScannerControls(props: ApplicationProps) {
  return (
    <div className="scanner-controls-bar">
      <div className="segmented">
        {(['all', 'crypto', 'stock'] as const).map((item) => (
          <button className={props.assetFilter === item ? 'active' : ''} key={item} onClick={() => props.setAssetFilter(item)} type="button">
            {item === 'all' ? 'Both' : item}
          </button>
        ))}
      </div>
      <div className="segmented">
        {(['signals', 'movers', 'volume'] as const).map((item) => (
          <button className={props.marketSort === item ? 'active' : ''} key={item} onClick={() => props.setMarketSort(item)} type="button">
            {item}
          </button>
        ))}
      </div>
      <button className="ghost-link" onClick={props.addSymbol} disabled={props.busy || !props.marketSearch.trim()} type="button">
        <Plus size={16} /> Add Symbol
      </button>
    </div>
  );
}

function Panel({
  title,
  icon,
  children,
  variant = 'primary',
  className = ''
}: {
  title: string;
  icon: ReactNode;
  children: ReactNode;
  variant?: 'primary' | 'hero' | 'active' | 'utility';
  className?: string;
}) {
  return (
    <section className={['panel', `${variant}-panel`, className].filter(Boolean).join(' ')}>
      <div className="panel-title">
        {icon}
        <h2>{title}</h2>
      </div>
      {children}
    </section>
  );
}

function MarketRibbon({ market }: { market: MarketAsset[] }) {
  const symbols = ['SPY', 'QQQ', 'DIA', 'VIX', ...market.map((asset) => asset.symbol)].filter((value, index, array) => array.indexOf(value) === index).slice(0, 8);
  return (
    <div className="market-ribbon">
      {symbols.map((symbol) => {
        const asset = market.find((item) => item.symbol === symbol);
        return (
          <div className="ribbon-item" key={symbol}>
            <strong>{symbol}</strong>
            <span>{asset ? money(asset.price) : '--'}</span>
            <MiniSparkline asset={asset ?? null} compact />
            <small className={(asset?.changePct ?? 0) >= 0 ? 'good' : 'bad'}>{asset ? pct(asset.changePct) : 'watch'}</small>
          </div>
        );
      })}
    </div>
  );
}

function Heatmap({ assets, selected, onSelect }: { assets: MarketAsset[]; selected: string; onSelect: (symbol: string) => void }) {
  return (
    <div className="heatmap">
      {assets.map((asset) => (
        <button className={`heat-tile ${selected === asset.symbol ? 'active' : ''}`} key={asset.symbol} onClick={() => onSelect(asset.symbol)} type="button">
          <strong>{asset.symbol}</strong>
          <span className={asset.changePct >= 0 ? 'good' : 'bad'}>{pct(asset.changePct)}</span>
          <MiniSparkline asset={asset} compact />
          <small>{asset.volatility > 0.05 ? 'High vol' : 'Measured'}</small>
        </button>
      ))}
    </div>
  );
}

function ResearchFeed({ state }: { state: AppState }) {
  const topSignals = state.signals.slice(0, 4);
  return (
    <div className="research-feed">
      {topSignals.map((signal) => (
        <article className="feed-item" key={signal.symbol}>
          <span>{signal.symbol}</span>
          <strong>{signal.setupType} / {Math.round(signal.confidence * 100)}% confidence</strong>
          <p>{signal.reason}</p>
          <small>{assetClassLabel(signal.assetClass)} / {sellZoneWithPrice(signal)} / trail {money(signal.trailingStop)} / {signal.risk}</small>
        </article>
      ))}
    </div>
  );
}

function RiskOverview({ state }: { state: AppState }) {
  return (
    <div className="risk-list">
      <StatusLine label="Risk State" value={riskState(state)} />
      <StatusLine label="Drawdown" value={pct(state.account.maxDrawdownPct)} />
      <StatusLine label="Exposure" value={money(state.account.exposure)} />
      <StatusLine label="Cash" value={money(state.account.cash)} />
      <StatusLine label="Provider" value={providerFreshness(state)} />
    </div>
  );
}

function SyntheticChart({ asset, signal }: { asset: MarketAsset | null; signal?: Signal | null }) {
  const values = sparklineValues(asset, 48);
  const path = sparklinePath(values, 100, 100);
  const volumeBars = values.slice(-32).map((value, index) => 18 + Math.abs(Math.sin((value + index) * 0.2)) * 58);
  return (
    <div className="synthetic-chart">
      <div className="chart-top">
        <div>
          <span>{asset?.symbol ?? '--'}</span>
          <strong>{asset ? money(asset.price) : '--'}</strong>
        </div>
        <div className="chart-status">
          {asset ? <FreshnessBadge value={assetFreshness(asset)} /> : null}
          <small>{asset ? `${assetProvider(asset)} / ${asset.metadata?.confidence ?? 70}% confidence` : 'No symbol selected'}</small>
        </div>
      </div>
      <div className="chart-canvas" aria-label={`${asset?.symbol ?? 'Selected symbol'} research chart`}>
        <svg className="chart-line-svg" viewBox="0 0 100 100" preserveAspectRatio="none" role="img">
          <defs>
            <linearGradient id="chartLineFill" x1="0" x2="0" y1="0" y2="1">
              <stop offset="0%" stopColor="rgba(126, 167, 200, 0.26)" />
              <stop offset="100%" stopColor="rgba(126, 167, 200, 0)" />
            </linearGradient>
          </defs>
          <path className="chart-fill" d={`${path} L 100 100 L 0 100 Z`} />
          <path className="chart-line" d={path} />
          {signal ? <line className="chart-target-line" x1="0" x2="100" y1="24" y2="24" /> : null}
          {signal ? <line className="chart-stop-line" x1="0" x2="100" y1="76" y2="76" /> : null}
        </svg>
        <div className="chart-volume-bars">
          {volumeBars.map((height, index) => <span key={index} style={{ height: `${height}%` }} />)}
        </div>
        {signal ? (
          <div className="chart-plan-markers">
            <span>Target {money(signal.target1)}</span>
            <span>Stop {money(signal.stop)}</span>
          </div>
        ) : null}
      </div>
    </div>
  );
}

function MiniSparkline({ asset, compact = false }: { asset: MarketAsset | null; compact?: boolean }) {
  const values = sparklineValues(asset, compact ? 18 : 28);
  const path = sparklinePath(values, 100, 36);
  const tone = (asset?.changePct ?? 0) >= 0 ? 'good' : 'bad';
  return (
    <svg className={`mini-sparkline ${tone} ${compact ? 'compact' : ''}`} viewBox="0 0 100 36" preserveAspectRatio="none" aria-hidden="true">
      <path d={path} />
    </svg>
  );
}

function ConfidenceMeter({ label, value, compact = false }: { label: string; value: number; compact?: boolean }) {
  const normalized = clamp(value, 0, 100);
  return (
    <div className={`confidence-meter ${compact ? 'compact' : ''}`}>
      <div>
        <span>{label}</span>
        <strong>{Math.round(normalized)}%</strong>
      </div>
      <div className="confidence-track">
        <span style={{ width: `${normalized}%` }} />
      </div>
    </div>
  );
}

function FreshnessBadge({ value }: { value: string }) {
  const normalized = value.toLowerCase();
  const tone = normalized.includes('live') || normalized.includes('fresh') ? 'fresh' : normalized.includes('stale') || normalized.includes('failed') ? 'stale' : normalized.includes('fallback') ? 'fallback' : 'delayed';
  return <span className={`freshness-badge ${tone}`}><i />{value}</span>;
}

function RiskSlider(props: { label: string; value: number; min: number; max: number; step: number; format: (value: number) => string; onChange: (value: number) => void }) {
  const [draft, setDraft] = useState(props.value);
  useEffect(() => setDraft(props.value), [props.value]);
  return (
    <label className="risk-slider">
      <span>{props.label}</span>
      <strong>{props.format(draft)}</strong>
      <input
        min={props.min}
        max={props.max}
        step={props.step}
        type="range"
        value={draft}
        onChange={(event) => setDraft(Number(event.target.value))}
        onPointerUp={() => props.onChange(draft)}
      />
    </label>
  );
}

function DataTable(props: { headers: string[]; rows: string[][]; empty: string }) {
  if (!props.rows.length) return <p className="muted">{props.empty}</p>;
  return (
    <div className="data-table">
      <div className="data-row header" style={{ '--cols': props.headers.length } as CSSProperties}>
        {props.headers.map((header) => <span key={header}>{header}</span>)}
      </div>
      {props.rows.map((row, index) => (
        <div className="data-row" key={index} style={{ '--cols': row.length } as CSSProperties}>
          {row.map((cell, cellIndex) => <span key={`${index}-${cellIndex}`}>{cell}</span>)}
        </div>
      ))}
    </div>
  );
}

function AuditList({ events, empty }: { events: AppState['audit']; empty: string }) {
  if (!events.length) return <p className="muted">{empty}</p>;
  return (
    <div className="audit-list">
      {events.map((event) => (
        <article className={`audit-row ${event.level}`} key={event.id}>
          <strong>{event.message}</strong>
          <span>{event.eventType} / {formatTime(event.createdAt)}</span>
        </article>
      ))}
    </div>
  );
}

function AlertList({ alerts }: { alerts: AppState['core']['alerts'] }) {
  if (!alerts.length) return <p className="muted">No alerts, provider warnings, or risk notifications yet.</p>;
  return (
    <div className="audit-list">
      {alerts.map((alert) => (
        <article className={`audit-row ${alert.severity}`} key={alert.id}>
          <strong>{alert.title}{alert.relatedSymbol ? ` / ${alert.relatedSymbol}` : ''}</strong>
          <span>{alert.detail} / {formatTime(alert.timestamp)}</span>
        </article>
      ))}
    </div>
  );
}

function JournalList({ entries }: { entries: AppState['core']['journal'] }) {
  if (!entries.length) return <p className="muted">No journal entries linked through the Core contract yet.</p>;
  return (
    <div className="audit-list">
      {entries.slice(0, 10).map((entry) => (
        <article className="audit-row info" key={entry.id}>
          <strong>{entry.symbol ? `${entry.symbol} / ${entry.tag}` : entry.tag}</strong>
          <span>{entry.note} / {formatTime(entry.createdAt)}</span>
        </article>
      ))}
    </div>
  );
}

function TimelineList({ events }: { events: ResearchTimelineEvent[] }) {
  if (!events.length) return <p className="muted">No symbol timeline yet. Alerts, notes, reports, signals, and paper orders will appear here as research history accumulates.</p>;
  return (
    <div className="timeline-list">
      {events.slice(0, 12).map((event) => (
        <article className={`timeline-item ${event.severity}`} key={event.id}>
          <span>{event.type} / {formatTime(event.timestamp)}</span>
          <strong>{event.title}</strong>
          <p>{event.detail}</p>
          <small>{event.source}{event.freshness ? ` / ${event.freshness}` : ''}</small>
        </article>
      ))}
    </div>
  );
}

function ContentPage({ eyebrow, title, copy, sections }: { eyebrow: string; title: string; copy: string; sections: Array<[string, string]> }) {
  return (
    <section className="content-page">
      <PageHeader eyebrow={eyebrow} title={title} copy={copy} />
      <div className="content-grid">
        {sections.map(([sectionTitle, sectionCopy]) => (
          <article className="content-tile" key={sectionTitle}>
            <h2>{sectionTitle}</h2>
            <p>{sectionCopy}</p>
          </article>
        ))}
      </div>
    </section>
  );
}

function PageHeader({ eyebrow, title, copy }: { eyebrow: string; title: string; copy: string }) {
  return (
    <header className="page-header">
      <p className="eyebrow">{eyebrow}</p>
      <h1>{title}</h1>
      <p>{copy}</p>
    </header>
  );
}

function AppPageHeader({ title, copy }: { title: string; copy: string }) {
  return (
    <header className="app-page-header">
      <h1>{title}</h1>
      <p>{copy}</p>
    </header>
  );
}

function NarrativeBlock({ icon, title, copy }: { icon: ReactNode; title: string; copy: string }) {
  return (
    <article className="narrative-block">
      <div className="tile-icon">{icon}</div>
      <h2>{title}</h2>
      <p>{copy}</p>
    </article>
  );
}

function FeatureGrid() {
  const features = [
    ['Scanner Engine', Gauge],
    ['Portfolio Console', Wallet],
    ['Strategy Lab', BarChart3],
    ['Research Reports', FileText],
    ['AI Analysis', BrainCircuit],
    ['Alert Engine', Bell],
    ['Market Replay', Play],
    ['Historical Intelligence', History]
  ] as const;
  return (
    <section className="feature-grid-section">
      <PageHeader eyebrow="Capability Map" title="High-end workflows without dashboard noise." copy="Every system should earn its place by improving research quality, continuity, risk clarity, or operational trust." />
      <div className="feature-grid">
        {features.map(([label, Icon]) => (
          <article className="feature-card" key={label}>
            <Icon size={19} />
            <span>{label}</span>
          </article>
        ))}
      </div>
    </section>
  );
}

function MarketTape({ state }: { state: AppState | null }) {
  const assets = state?.market.slice(0, 5) ?? [];
  return (
    <div className="hero-tape">
      {assets.length ? assets.map((asset) => (
        <span key={asset.symbol}>
          <strong>{asset.symbol}</strong>
          <em className={asset.changePct >= 0 ? 'good' : 'bad'}>{pct(asset.changePct)}</em>
        </span>
      )) : ['SPY', 'QQQ', 'DIA', 'VIX'].map((symbol) => (
        <span key={symbol}><strong>{symbol}</strong><em>research</em></span>
      ))}
    </div>
  );
}

function DeviceCard({ icon, title, meta }: { icon: ReactNode; title: string; meta: string }) {
  return (
    <article className="device-card">
      {icon}
      <strong>{title}</strong>
      <span>{meta}</span>
    </article>
  );
}

function FinalCta() {
  return (
    <section className="final-cta">
      <p className="eyebrow">Built for disciplined market research.</p>
      <h2>Calm intelligence for long-term portfolio work.</h2>
      <a className="solid-link large" href={pathForRoute('dashboard')}>Launch Platform</a>
    </section>
  );
}

function SiteFooter() {
  return (
    <footer className="site-footer">
      <a className="brand footer-brand" href={pathForRoute('home')} aria-label="Auralith home">
        <BrandLogo />
      </a>
      <nav>
        <a href={pathForRoute('changelog')}>Changelog</a>
        <a href={pathForRoute('about')}>About</a>
        <a href={pathForRoute('support')}>Support</a>
      </nav>
    </footer>
  );
}

function Input({
  icon,
  label,
  type,
  value,
  onChange,
  autoComplete
}: {
  icon: ReactNode;
  label: string;
  type: string;
  value?: string;
  onChange?: (value: string) => void;
  autoComplete?: string;
}) {
  return (
    <label className="input-row">
      {icon}
      <input
        aria-label={label}
        autoComplete={autoComplete}
        placeholder={label}
        type={type}
        value={value}
        onChange={(event) => onChange?.(event.target.value)}
      />
    </label>
  );
}

function SessionList({ sessions }: { sessions?: AuthSession[] }) {
  if (sessions?.length) {
    return (
      <div className="session-list">
        {sessions.map((session) => (
          <article key={session.id}>
            <Fingerprint size={17} />
            <span>{session.deviceLabel}</span>
            <small>{session.revokedAt ? 'Revoked' : `Expires ${new Date(session.expiresAt).toLocaleDateString()}`}</small>
          </article>
        ))}
      </div>
    );
  }
  return (
    <div className="session-list">
      {['Desktop Terminal / Local', 'Web Cockpit / 127.0.0.1', 'Mobile Companion / Planned'].map((item) => (
        <article key={item}>
          <Fingerprint size={17} />
          <span>{item}</span>
          <small>Trusted</small>
        </article>
      ))}
    </div>
  );
}

function ReportCard({ title, copy }: { title: string; copy: string }) {
  return (
    <article className="report-card">
      <FileText size={18} />
      <h2>{title}</h2>
      <p>{copy}</p>
      <button className="ghost-link" type="button">Prepare</button>
    </article>
  );
}

function MiniMetric({ label, value, tone }: { label: string; value: string; tone: 'good' | 'bad' | 'warn' | 'neutral' }) {
  return (
    <article className={`mini-metric ${tone}`}>
      <span>{label}</span>
      <strong>{value}</strong>
    </article>
  );
}

function StatusLine({ label, value }: { label: string; value: string }) {
  return (
    <div className="status-line">
      <span>{label}</span>
      <strong>{value}</strong>
    </div>
  );
}

function LoadingState() {
  return (
    <section className="loading-state">
      <Activity className="spin" size={24} />
      <h1>Auralith Markets</h1>
      <p>Preparing the research cockpit.</p>
    </section>
  );
}

function Toast({ message }: { message: string }) {
  return (
    <div className="toast">
      <CheckCircle2 size={16} />
      {message}
    </div>
  );
}

function readSavedAppState(): AppState | null {
  try {
    const raw = window.localStorage.getItem(APP_STATE_STORAGE_KEY);
    return raw ? (JSON.parse(raw) as AppState) : null;
  } catch {
    return null;
  }
}

function saveAppState(state: AppState) {
  try {
    window.localStorage.setItem(APP_STATE_STORAGE_KEY, JSON.stringify(state));
  } catch {
    try {
      window.localStorage.setItem(APP_STATE_STORAGE_KEY, JSON.stringify(compactStateForStorage(state)));
    } catch {
      // Local storage can be unavailable in private or locked-down browser contexts.
    }
  }
}

function compactStateForStorage(state: AppState): AppState {
  const watchlistSymbols = new Set(state.core.watchlists.flatMap((group) => group.symbols));
  const importantSymbols = new Set([
    ...state.positions.map((position) => position.symbol),
    ...state.orders.slice(0, 40).map((order) => order.symbol),
    ...state.signals.slice(0, 80).map((signal) => signal.symbol),
    ...Array.from(watchlistSymbols).slice(0, 120)
  ]);
  const market = state.market.filter((asset, index) => index < 160 || importantSymbols.has(asset.symbol)).slice(0, 240);
  const availableSymbols = new Set(market.map((asset) => asset.symbol));

  return {
    ...state,
    market,
    signals: state.signals.filter((signal, index) => index < 100 || availableSymbols.has(signal.symbol)).slice(0, 140),
    orders: state.orders.slice(0, 80),
    audit: state.audit.slice(0, 80),
    strategyPerformance: state.strategyPerformance.slice(0, 40),
    core: {
      ...state.core,
      scanner: state.core.scanner.filter((signal, index) => index < 100 || availableSymbols.has(signal.symbol)).slice(0, 140),
      alerts: state.core.alerts.slice(0, 80),
      reports: state.core.reports.slice(0, 40),
      journal: state.core.journal.slice(0, 80),
      symbolResearch: state.core.symbolResearch.filter((research, index) => index < 40 || availableSymbols.has(research.symbol)).slice(0, 80),
      paperOrders: state.core.paperOrders.slice(0, 80),
      aiSummaries: state.core.aiSummaries.slice(0, 20)
    }
  };
}

function readSavedSelectedSymbol() {
  try {
    return window.localStorage.getItem(SELECTED_SYMBOL_STORAGE_KEY) || 'AAPL';
  } catch {
    return 'AAPL';
  }
}

function saveSelectedSymbol(symbol: string) {
  try {
    window.localStorage.setItem(SELECTED_SYMBOL_STORAGE_KEY, symbol);
  } catch {
    // Symbol persistence is a convenience only; the cockpit remains usable without it.
  }
}

function readRoute(): Route {
  if (window.location.hash.startsWith('#/')) {
    return routeFromString(window.location.hash.replace(/^#\/?/, '') || 'home');
  }
  return routeFromPath(window.location.pathname);
}

function isAuthRoute(route: Route) {
  return authRoutes.includes(route);
}

function isAppRoute(route: Route) {
  return appShellRoutes.includes(route);
}

function providerFreshness(state: AppState) {
  if (state.core?.providerHealth) {
    const provider = state.core.providerHealth;
    return `${provider.freshness} / ${provider.source}`;
  }
  if (!state.market.length) return 'No providers';
  const timestamps = state.market.map((asset) => Date.parse(asset.metadata?.fetchedAt ?? asset.updatedAt)).filter(Number.isFinite);
  const latest = timestamps.length ? Math.max(...timestamps) : 0;
  const fallback = state.market.filter((asset) => asset.metadata?.fallback || asset.source === 'simulated').length;
  const label = freshnessLabel(latest);
  return fallback ? `${label} / fallback ${fallback}` : label;
}

function marketSessionLabel(now = new Date()) {
  const day = now.getDay();
  const minutes = now.getHours() * 60 + now.getMinutes();
  if (day === 0 || day === 6) return 'Closed';
  if (minutes < 8 * 60 + 30) return 'Premarket';
  if (minutes < 15 * 60) return 'Open';
  if (minutes < 19 * 60) return 'After Hours';
  return 'Closed';
}

function clamp(value: number, min: number, max: number) {
  return Math.min(max, Math.max(min, Number.isFinite(value) ? value : min));
}

function sparklineValues(asset: MarketAsset | null, count: number) {
  const base = asset?.price ?? 100;
  const drift = (asset?.momentum ?? asset?.changePct ?? 0) * 42;
  const volatility = clamp((asset?.volatility ?? 0.025) * 100, 1, 9);
  return Array.from({ length: count }, (_, index) => {
    const wave = Math.sin((base * 0.07) + index * 0.52) * volatility;
    const pulse = Math.cos((base * 0.11) + index * 0.19) * (volatility * 0.52);
    return 50 + wave + pulse + drift * (index / Math.max(count - 1, 1));
  });
}

function sparklinePath(values: number[], width: number, height: number) {
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = Math.max(max - min, 1);
  return values.map((value, index) => {
    const x = (index / Math.max(values.length - 1, 1)) * width;
    const y = height - ((value - min) / range) * (height * 0.72) - height * 0.14;
    return `${index === 0 ? 'M' : 'L'} ${x.toFixed(2)} ${y.toFixed(2)}`;
  }).join(' ');
}

function riskToneClass(riskScore: number) {
  if (riskScore >= 72) return 'risk-high';
  if (riskScore >= 48) return 'risk-caution';
  return 'risk-low';
}

function assetProvider(asset: MarketAsset) {
  return asset.metadata?.provider ?? asset.source;
}

function assetFreshness(asset: MarketAsset) {
  return asset.metadata?.freshness ?? freshnessLabel(Date.parse(asset.updatedAt));
}

function assetClassLabel(assetClass?: AssetClass) {
  return assetClass === 'crypto' ? 'Crypto' : assetClass === 'stock' ? 'Stock' : 'Asset';
}

function sellZoneWithPrice(signal: Signal) {
  const currentPrice = Number.isFinite(signal.currentPrice) ? signal.currentPrice : signal.entry;
  return `Current ${money(currentPrice)} / Zone ${signal.expectedSellZone}`;
}

function averageDataConfidence(assets: MarketAsset[]) {
  if (!assets.length) return 0;
  const total = assets.reduce((sum, asset) => sum + (asset.metadata?.confidence ?? (asset.source === 'simulated' ? 55 : 70)), 0);
  return Math.round(total / assets.length);
}

function riskState(state: AppState) {
  if (state.core?.portfolio?.riskState) return state.core.portfolio.riskState;
  if (state.account.maxDrawdownPct < -0.08 || state.account.dailyPnlPct < -state.settings.dailyLossLimitPct * 0.75) return 'Elevated';
  if (state.account.exposure / Math.max(state.account.equity, 1) > 0.72) return 'Caution';
  return 'Healthy';
}

function stateWeightForSymbol(state: AppState, symbol?: string) {
  if (!symbol) return '--';
  const position = state.positions.find((item) => item.symbol === symbol);
  return position ? pct(position.marketValue / Math.max(state.account.equity, 1)) : '0.00%';
}

function watchlistMoverAssets(state: AppState) {
  const movers = state.core.watchlists.find((group) => group.id === 'watchlist-movers')?.symbols ?? [];
  const assets = movers.map((symbol) => state.market.find((asset) => asset.symbol === symbol)).filter((asset): asset is MarketAsset => Boolean(asset));
  return assets.length ? assets : state.market.slice(0, 10);
}

function money(value: number) {
  return new Intl.NumberFormat('en-US', { style: 'currency', currency: 'USD', maximumFractionDigits: value > 1000 ? 0 : 2 }).format(value);
}

function bytes(value: number) {
  if (value < 1024) return `${value} B`;
  if (value < 1024 * 1024) return `${(value / 1024).toFixed(1)} KB`;
  return `${(value / (1024 * 1024)).toFixed(1)} MB`;
}

function pct(value: number) {
  return `${(value * 100).toFixed(2)}%`;
}

function fmt(value: number, digits = 2) {
  return value.toLocaleString(undefined, { maximumFractionDigits: digits });
}

function compact(value: number) {
  return new Intl.NumberFormat('en-US', { notation: 'compact', maximumFractionDigits: 1 }).format(value);
}

function freshnessLabel(timestamp: number) {
  if (!timestamp || !Number.isFinite(timestamp)) return 'unknown';
  const ageMs = Date.now() - timestamp;
  if (ageMs < 2 * 60 * 1000) return 'fresh';
  if (ageMs < 15 * 60 * 1000) return 'delayed';
  return 'stale';
}

function formatTime(value: string) {
  const timestamp = Date.parse(value);
  return timestamp ? new Date(timestamp).toLocaleTimeString() : '--';
}

export default App;
