const CACHE_NAME = 'auralith-markets-v2';
const CORE = [
  '/',
  '/manifest.webmanifest',
  '/icon.svg',
  '/auralith-icon-192.png',
  '/auralith-icon-512.png',
  '/auralith-mark.png',
  '/auralith-wordmark.png',
  '/terminal-preview.png'
];

self.addEventListener('install', (event) => {
  self.skipWaiting();
  event.waitUntil(caches.open(CACHE_NAME).then((cache) => cache.addAll(CORE)));
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys().then((keys) => Promise.all(keys.filter((key) => key !== CACHE_NAME).map((key) => caches.delete(key))))
  );
  self.clients.claim();
});

self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url);
  if (url.pathname.startsWith('/api')) return;
  if (event.request.method !== 'GET') return;

  if (event.request.mode === 'navigate') {
    event.respondWith(fetch(event.request).catch(() => caches.match('/')));
    return;
  }

  event.respondWith(
    caches.match(event.request).then((cached) => {
      const network = fetch(event.request)
        .then((response) => {
          if (response.ok) {
            const copy = response.clone();
            caches.open(CACHE_NAME).then((cache) => cache.put(event.request, copy));
          }
          return response;
        })
        .catch(() => cached || caches.match('/'));
      return network;
    })
  );
});

self.addEventListener('push', (event) => {
  let payload = {
    title: 'Auralith update',
    body: 'A new Auralith notification is ready.'
  };
  try {
    payload = event.data ? event.data.json() : payload;
  } catch {
    payload = {
      title: 'Auralith update',
      body: event.data?.text?.() ?? payload.body
    };
  }
  event.waitUntil(
    self.registration.showNotification(payload.title ?? 'Auralith update', {
      body: payload.body ?? 'Open Auralith to review the latest intelligence.',
      icon: '/auralith-icon-192.png',
      badge: '/auralith-icon-192.png',
      tag: payload.tag ?? 'auralith-intelligence',
      data: { url: payload.url ?? '/#/alerts' }
    })
  );
});

self.addEventListener('notificationclick', (event) => {
  event.notification.close();
  const target = event.notification.data?.url ?? '/#/alerts';
  event.waitUntil(
    self.clients.matchAll({ type: 'window', includeUncontrolled: true }).then((clients) => {
      const existing = clients.find((client) => 'focus' in client);
      if (existing) {
        existing.navigate?.(target);
        return existing.focus();
      }
      return self.clients.openWindow(target);
    })
  );
});
