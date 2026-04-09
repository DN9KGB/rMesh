// Digital clock - replaces analog canvas clock
var _clockOffsetMs = 0;
var _clockHasServerTime = false;

function drawClock(now) {
  if (now instanceof Date) {
    _clockOffsetMs = now.getTime() - Date.now();
    _clockHasServerTime = true;
  }
  _renderClock();
}

function _renderClock() {
  var el = document.getElementById('clock');
  if (!el) return;
  var now = new Date(Date.now() + _clockOffsetMs);
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

setInterval(function () {
  if (_clockHasServerTime) _renderClock();
}, 1000);
