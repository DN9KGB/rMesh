// Digital clock - ticks locally every second, synced by server status pushes
var _clockServerTime = 0;   // last server epoch (seconds)
var _clockLocalBase  = 0;   // millis() when we received it

function drawClock(now) {
  // Called from status push with server time — resync
  _clockServerTime = Math.floor(now.getTime() / 1000);
  _clockLocalBase  = Date.now();
  _renderClock(now);
}

function _renderClock(now) {
  var el = document.getElementById('clock');
  if (!el) return;
  var h = String(now.getHours()).padStart(2, '0');
  var m = String(now.getMinutes()).padStart(2, '0');
  var s = String(now.getSeconds()).padStart(2, '0');
  el.textContent = h + ':' + m + ':' + s;

  var dateEl = document.getElementById('date');
  if (dateEl) {
    var fmt = new Intl.DateTimeFormat(currentLang === 'de' ? 'de-DE' : 'en-GB', {
      day: '2-digit', month: '2-digit', year: '2-digit'
    });
    dateEl.textContent = fmt.format(now);
  }
}

setInterval(function() {
  if (!_clockServerTime) return;
  var elapsed = Math.floor((Date.now() - _clockLocalBase) / 1000);
  _renderClock(new Date((_clockServerTime + elapsed) * 1000));
}, 1000);