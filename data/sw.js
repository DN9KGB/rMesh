// sw.js - Der Hintergrund-Wächter

// Installation
self.addEventListener('install', function(event) {
    self.skipWaiting(); // Sofort aktivieren
    console.log('SW: Installiert');
});

// Aktivierung
self.addEventListener('activate', function(event) {
    console.log('SW: Aktiviert');
});

// Auf Nachrichten von der Hauptseite reagieren
self.addEventListener('message', function(event) {
    var data = event.data;

    if (data.type === 'SHOW_LORA_NOTIFICATION') {
        var options = {
            body: data.message,
            icon: '/192.png', // Pfad zu deinem Icon auf dem ESP32
            vibrate: [200, 100, 200],
            data: { url: data.url } // Damit wir wissen, wo wir beim Klick hinmüssen
        };

        event.waitUntil(
            self.registration.showNotification(data.title, options)
        );
    }
});

// Wenn der User auf die Benachrichtigung klickt
self.addEventListener('notificationclick', function(event) {
    event.notification.close();
    event.waitUntil(
        clients.openWindow('/mobile.html') // Öffnet deine rMesh Seite
    );
});

self.addEventListener('fetch', function(event) {
    // Muss vorhanden sein für PWA
});