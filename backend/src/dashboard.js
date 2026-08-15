// Self-contained ops dashboard served at GET /dashboard. No build step, no
// dependencies, no external requests — it polls /stats and redraws.
// Written without ${} so the whole page can live in one template literal.
export const DASHBOARD_HTML = `<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Reactor Drone — Live Ops</title>
<style>
:root{
  color-scheme:light;
  --page:#f9f9f7; --surface:#fcfcfb; --ink:#0b0b0b; --ink-2:#52514e; --muted:#898781;
  --grid:#e1e0d9; --axis:#c3c2b7; --series:#2a78d6; --good:#0ca30c;
  --border:rgba(11,11,11,.10); --wash:rgba(42,120,214,.14);
}
@media (prefers-color-scheme:dark){:root:not([data-theme=light]){
  color-scheme:dark;
  --page:#0d0d0d; --surface:#1a1a19; --ink:#fff; --ink-2:#c3c2b7; --muted:#898781;
  --grid:#2c2c2a; --axis:#383835; --series:#3987e5; --good:#0ca30c;
  --border:rgba(255,255,255,.10); --wash:rgba(57,135,229,.18);
}}
:root[data-theme=dark]{
  color-scheme:dark;
  --page:#0d0d0d; --surface:#1a1a19; --ink:#fff; --ink-2:#c3c2b7; --muted:#898781;
  --grid:#2c2c2a; --axis:#383835; --series:#3987e5; --good:#0ca30c;
  --border:rgba(255,255,255,.10); --wash:rgba(57,135,229,.18);
}
*{box-sizing:border-box}
body{margin:0;background:var(--page);color:var(--ink);
  font:15px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif;padding:24px 20px 64px}
/* minmax(0,1fr): an auto track sizes to max-content and would let the tile
   grid push the page wider than the viewport on narrow screens. */
.wrap{max-width:1080px;margin:0 auto;display:grid;grid-template-columns:minmax(0,1fr);gap:20px}
header{display:flex;flex-wrap:wrap;gap:12px;align-items:baseline;justify-content:space-between}
h1{font-size:20px;margin:0;letter-spacing:-.01em}
h2{font-size:14px;margin:0 0 2px;letter-spacing:-.01em}
.sub{color:var(--muted);font-size:13px}
.live{display:flex;align-items:center;gap:8px;color:var(--ink-2);font-size:13px}
.dot{width:8px;height:8px;border-radius:50%;background:var(--good);flex:none}
.dot.stale{background:#d03b3b}
button{font:inherit;font-size:13px;color:var(--ink-2);background:var(--surface);
  border:1px solid var(--border);border-radius:8px;padding:4px 10px;cursor:pointer}
button:hover{color:var(--ink)}
.card{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:16px}
.tiles{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(150px,1fr))}
.tile .k{color:var(--muted);font-size:12px;text-transform:uppercase;letter-spacing:.05em}
.tile .v{font-size:28px;line-height:1.15;margin-top:6px;letter-spacing:-.02em}
.tile .d{color:var(--ink-2);font-size:12px;margin-top:2px}
table{width:100%;border-collapse:collapse;font-variant-numeric:tabular-nums}
th,td{text-align:right;padding:7px 8px;border-bottom:1px solid var(--grid);white-space:nowrap}
th.lead,td.lead{text-align:left;width:100%}
/* the 7-column player table scrolls rather than crushing the name column */
#ptab{min-width:620px}
thead th{color:var(--muted);font-weight:500;font-size:12px;text-transform:uppercase;
  letter-spacing:.05em;cursor:pointer;user-select:none;border-bottom-color:var(--axis)}
thead th:hover{color:var(--ink)}
tbody tr:last-child td{border-bottom:0}
tbody tr:hover{background:color-mix(in srgb,var(--ink) 4%,transparent)}
/* wide enough that the magnitude bar behind the value reads as a bar */
td.bar,th[data-k=best]{min-width:112px}
td.bar{position:relative}
td.bar i{position:absolute;left:0;top:3px;bottom:3px;background:var(--wash);
  border-radius:3px;pointer-events:none}
td.bar span{position:relative}
.rank{color:var(--muted);text-align:right;width:1%;padding-right:2px}
.scroll{overflow-x:auto}
.chart{width:100%;height:180px;display:block;overflow:visible}
.chart text{fill:var(--muted);font-size:11px}
.tip{position:fixed;pointer-events:none;opacity:0;transition:opacity .1s;
  background:var(--surface);color:var(--ink);border:1px solid var(--border);
  border-radius:8px;padding:6px 9px;font-size:12px;line-height:1.4;
  box-shadow:0 4px 16px rgba(0,0,0,.16);z-index:9;white-space:nowrap}
.empty{color:var(--muted);font-size:13px;padding:12px 0}
/* Feedback entries are prose, not figures — a list, not a table row. */
.fb{border-bottom:1px solid var(--grid);padding:12px 0}
.fb:last-child{border-bottom:0;padding-bottom:0}
.fb .h{display:flex;flex-wrap:wrap;gap:8px;align-items:baseline;justify-content:space-between}
.fb .s{font-weight:600}
.fb .m{color:var(--muted);font-size:12px}
.fb .b{white-space:pre-wrap;margin-top:6px;color:var(--ink-2);font-size:14px;
  overflow-wrap:anywhere}
.tag{display:inline-block;background:var(--wash);color:var(--ink-2);border-radius:5px;
  padding:1px 6px;font-size:11px;margin-right:4px}
#stab{min-width:420px}
.err{color:#d03b3b;font-size:13px}
</style></head><body>
<div class="wrap">
<header>
  <div><h1>Reactor Drone — Live Ops</h1><div class="sub" id="rel">&nbsp;</div></div>
  <div class="live"><span class="dot" id="dot"></span><span id="upd">connecting…</span>
    <button id="refresh">Refresh</button></div>
</header>

<div class="tiles" id="tiles"></div>

<section class="card">
  <h2>Runs per day</h2><div class="sub" id="actsub">last 14 days</div>
  <svg class="chart" id="act" role="img" aria-label="Runs per day, last 14 days"></svg>
</section>

<section class="card">
  <h2>Feedback</h2><div class="sub">newest first · last 30 reports</div>
  <div id="flist"></div>
  <div class="empty" id="fempty" hidden>No feedback yet.</div>
</section>

<section class="card">
  <h2>Players</h2><div class="sub">click a column to sort · top 100 by best score</div>
  <div class="scroll"><table id="ptab">
    <thead><tr><th></th>
      <th class="lead" data-k="name">Pilot</th><th data-k="runs">Runs</th><th data-k="best">Best</th>
      <th data-k="total">Cumulative</th><th data-k="avg">Avg</th><th data-k="last">Last seen</th>
    </tr></thead><tbody></tbody>
  </table></div>
  <div class="empty" id="pempty" hidden>No players yet.</div>
</section>

<section class="card">
  <h2>Recent runs</h2><div class="sub">newest first</div>
  <div class="scroll"><table id="rtab">
    <thead><tr><th class="lead">Pilot</th><th>Score</th><th>When</th></tr></thead><tbody></tbody>
  </table></div>
  <div class="empty" id="rempty" hidden>No runs banked yet.</div>
</section>

<section class="card">
  <h2>Mailing list</h2><div class="sub">newest first · last 50 signups</div>
  <div class="scroll"><table id="stab">
    <thead><tr><th class="lead">Address</th><th>Source</th><th>When</th></tr></thead><tbody></tbody>
  </table></div>
  <div class="empty" id="sempty" hidden>No subscribers yet.</div>
</section>

<div class="sub" id="err"></div>
</div>
<div class="tip" id="tip"></div>

<script>
// 30 s, not 15: the poll now runs six D1 queries instead of four and the
// inbox does not need second-level freshness.
var POLL_MS = 30000;
var $ = function(id){ return document.getElementById(id); };
var fmt = function(n){ return (n == null ? 0 : n).toLocaleString(); };

function ago(ts){
  if (!ts) return '—';
  var s = Math.max(0, Math.floor(Date.now()/1000) - ts);
  if (s < 60) return s + 's ago';
  if (s < 3600) return Math.floor(s/60) + 'm ago';
  if (s < 86400) return Math.floor(s/3600) + 'h ago';
  return Math.floor(s/86400) + 'd ago';
}

// Bar with rounded data-end only; the baseline end stays square on the axis.
function barPath(x, y, w, h, r){
  r = Math.max(0, Math.min(r, w/2, h));
  return 'M' + x + ',' + (y+h) + 'V' + (y+r) + 'q0,' + (-r) + ' ' + r + ',' + (-r)
       + 'h' + (w-2*r) + 'q' + r + ',0 ' + r + ',' + r + 'V' + (y+h) + 'Z';
}

var tip = $('tip');
function showTip(e, html){
  tip.innerHTML = html; tip.style.opacity = 1;
  var r = tip.getBoundingClientRect();
  var x = Math.min(e.clientX + 12, innerWidth - r.width - 8);
  tip.style.left = Math.max(8, x) + 'px';
  tip.style.top = Math.max(8, e.clientY - r.height - 12) + 'px';
}
function hideTip(){ tip.style.opacity = 0; }

var daily = [];
function drawActivity(){
  var svg = $('act');
  // Draw at the SVG's real pixel width so the viewBox maps 1:1 — stretching a
  // fixed viewBox to fit would distort the labels and the corner radii.
  var W = Math.max(280, Math.round(svg.getBoundingClientRect().width) || 720);
  var H = 180, padL = 34, padR = 8, padT = 10, padB = 26;
  svg.setAttribute('viewBox', '0 0 ' + W + ' ' + H);
  var iw = W - padL - padR, ih = H - padT - padB;
  var max = 0, i;
  for (i = 0; i < daily.length; i++) max = Math.max(max, daily[i].n);
  var top = Math.max(1, max);
  var s = '';
  // recessive gridlines + y ticks at 0, half, top
  var ticks = [0, Math.round(top/2), top].filter(function(v, j, a){ return a.indexOf(v) === j; });
  for (i = 0; i < ticks.length; i++){
    var gy = padT + ih - (ticks[i]/top) * ih;
    s += '<line x1="' + padL + '" y1="' + gy + '" x2="' + (W-padR) + '" y2="' + gy
      + '" stroke="var(--grid)" stroke-width="1"/>'
      + '<text x="' + (padL-8) + '" y="' + (gy+4) + '" text-anchor="end">' + ticks[i] + '</text>';
  }
  var n = Math.max(1, daily.length);
  var slot = iw / n, gap = 2, bw = Math.max(3, slot - gap);
  // label only as often as the width allows, so ticks never collide
  var lblStep = Math.max(1, Math.ceil(46 / slot));
  for (i = 0; i < daily.length; i++){
    var d = daily[i];
    var bh = (d.n / top) * ih;
    var x = padL + i*slot + (slot - bw)/2, y = padT + ih - bh;
    s += '<path d="' + barPath(x, y, bw, Math.max(bh, d.n ? 2 : 0), 4)
      + '" fill="var(--series)" data-i="' + i + '"/>';
    // hit target spans the whole slot, not just the bar
    s += '<rect x="' + (padL + i*slot) + '" y="' + padT + '" width="' + slot + '" height="' + ih
      + '" fill="transparent" data-i="' + i + '"/>';
    if (i % lblStep === 0)
      s += '<text x="' + (x + bw/2) + '" y="' + (H-8) + '" text-anchor="middle">'
        + d.d.slice(5) + '</text>';
  }
  s += '<line x1="' + padL + '" y1="' + (padT+ih) + '" x2="' + (W-padR) + '" y2="' + (padT+ih)
    + '" stroke="var(--axis)" stroke-width="1"/>';
  svg.innerHTML = s;
  svg.onmousemove = function(e){
    var t = e.target.getAttribute && e.target.getAttribute('data-i');
    if (t == null) return hideTip();
    var d = daily[+t];
    showTip(e, '<b>' + d.d + '</b><br>' + fmt(d.n) + (d.n === 1 ? ' run' : ' runs')
      + (d.best ? '<br>best ' + fmt(d.best) : ''));
  };
  svg.onmouseleave = hideTip;
}

var sortK = 'best', sortD = -1, players = [];
function drawPlayers(){
  var tb = $('ptab').querySelector('tbody');
  $('pempty').hidden = players.length > 0;
  var rows = players.slice().sort(function(a, b){
    var x = a[sortK], y = b[sortK];
    if (typeof x === 'string') return sortD * x.localeCompare(y);
    return sortD * ((x||0) - (y||0));
  });
  var max = 0, i;
  for (i = 0; i < rows.length; i++) max = Math.max(max, rows[i].best);
  var s = '';
  for (i = 0; i < rows.length; i++){
    var p = rows[i];
    var pct = max ? (p.best / max * 100) : 0;
    s += '<tr><td class="rank">' + (i+1) + '</td><td class="lead">' + esc(p.name) + '</td>'
      + '<td>' + fmt(p.runs) + '</td>'
      + '<td class="bar"><i style="width:' + pct.toFixed(1) + '%"></i><span>' + fmt(p.best) + '</span></td>'
      + '<td>' + fmt(p.total) + '</td><td>' + fmt(p.avg) + '</td>'
      + '<td>' + ago(p.last) + '</td></tr>';
  }
  tb.innerHTML = s;
}

function drawRecent(recent){
  var tb = $('rtab').querySelector('tbody');
  $('rempty').hidden = recent.length > 0;
  var s = '', i;
  for (i = 0; i < recent.length; i++)
    s += '<tr><td class="lead">' + esc(recent[i].name) + '</td><td>' + fmt(recent[i].score)
      + '</td><td>' + ago(recent[i].ts) + '</td></tr>';
  tb.innerHTML = s;
}

// Tags arrive comma-separated. Split on ',' rather than a regex: this whole
// page lives in a template literal, where a \s would be eaten as an escape.
function tagPills(t){
  var a = String(t == null ? '' : t).split(','), s = '', i, v;
  for (i = 0; i < a.length; i++) {
    v = a[i].trim();
    if (v) s += '<span class="tag">' + esc(v) + '</span>';
  }
  return s;
}

function drawFeedback(rows){
  $('fempty').hidden = rows.length > 0;
  var s = '', i, r, ctx;
  for (i = 0; i < rows.length; i++) {
    r = rows[i];
    ctx = [];
    if (r.pilot) ctx.push(esc(r.pilot));
    ctx.push(esc(r.platform || '?') + ' · ' + esc(r.version || '?'));
    // in_run is why the feedback table carries run columns at all — they are
    // NULL for a report sent from the main menu, so only read them when set.
    if (r.in_run) ctx.push('wave ' + fmt(r.wave) + ' · ' + fmt(r.score) + ' pts'
      + (r.difficulty ? ' · ' + esc(r.difficulty) : ''));
    if (r.from_name) ctx.push('from ' + esc(r.from_name));
    s += '<div class="fb"><div class="h"><span class="s">'
      + (esc(r.subject) || '(no subject)') + '</span><span class="m">' + ago(r.ts)
      + '</span></div><div class="m">' + tagPills(r.tags) + ctx.join(' · ')
      + '</div><div class="b">' + esc(r.body) + '</div></div>';
  }
  $('flist').innerHTML = s;
}

function drawSubs(rows){
  var tb = $('stab').querySelector('tbody');
  $('sempty').hidden = rows.length > 0;
  var s = '', i;
  for (i = 0; i < rows.length; i++)
    s += '<tr><td class="lead">' + esc(rows[i].email) + '</td><td>'
      + esc(rows[i].source) + '</td><td>' + ago(rows[i].ts) + '</td></tr>';
  tb.innerHTML = s;
}

function esc(x){
  return String(x == null ? '' : x).replace(/[&<>"]/g, function(c){
    return ({ '&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;' })[c];
  });
}

function tile(k, v, d){
  return '<div class="card tile"><div class="k">' + k + '</div><div class="v">' + v
    + '</div><div class="d">' + (d || '&nbsp;') + '</div></div>';
}

function render(data){
  var t = data.totals;
  $('tiles').innerHTML =
      tile('Pilots', fmt(t.players), fmt(t.players_24h) + ' active in 24h')
    + tile('Runs banked', fmt(t.runs), fmt(t.runs_24h) + ' in last 24h')
    + tile('Best score', fmt(t.best), t.best_name ? 'by ' + esc(t.best_name) : '')
    + tile('Runs / pilot', t.players ? (t.runs / t.players).toFixed(1) : '0', 'mean')
    + tile('Avg score', fmt(t.avg), 'across all runs')
    + tile('Score banked', fmt(t.total_score), 'cumulative')
    + tile('Subscribers', fmt(t.subs), fmt(t.subs_24h) + ' in last 24h')
    + tile('Feedback', fmt(t.fb), fmt(t.fb_24h) + ' in last 24h');
  $('rel').textContent = 'release ' + (data.version || '—')
    + ' · api ' + location.host;
  daily = data.daily;
  drawActivity();
  players = data.players;
  drawPlayers();
  drawRecent(data.recent);
  drawFeedback(data.feedback || []);
  drawSubs(data.subs || []);
}

var lastOk = 0;
function load(){
  return fetch('/stats', { cache: 'no-store' })
    .then(function(r){ if (!r.ok) throw new Error('HTTP ' + r.status); return r.json(); })
    .then(function(d){
      render(d); lastOk = Date.now(); $('err').textContent = '';
      $('dot').classList.remove('stale');
    })
    .catch(function(e){
      $('err').innerHTML = '<span class="err">' + esc(e.message) + '</span>';
      $('dot').classList.add('stale');
    });
}

$('ptab').querySelector('thead').onclick = function(e){
  var k = e.target.getAttribute('data-k');
  if (!k) return;
  if (k === sortK) sortD = -sortD; else { sortK = k; sortD = k === 'name' ? 1 : -1; }
  drawPlayers();
};
$('refresh').onclick = load;
var rz;
addEventListener('resize', function(){
  clearTimeout(rz);
  rz = setTimeout(function(){ if (daily.length) drawActivity(); }, 120);
});
setInterval(function(){
  $('upd').textContent = lastOk ? 'updated ' + ago(Math.floor(lastOk/1000)) : 'no data';
}, 1000);
setInterval(load, POLL_MS);
// Pause polling while the tab is hidden; refresh immediately on return.
document.addEventListener('visibilitychange', function(){ if (!document.hidden) load(); });
load();
</script></body></html>`;
