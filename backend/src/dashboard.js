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
  --grid:#e1e0d9; --axis:#c3c2b7; --series:#2a78d6; --good:#0ca30c; --warn:#fab219; --bad:#d03b3b;
  --border:rgba(11,11,11,.10); --wash:rgba(42,120,214,.14);
  /* categorical slots 1-4 (validated light) — outcome split only */
  --c1:#2a78d6; --c2:#eb6834; --c3:#1baf7a; --c4:#eda100;
  --meter-track:#cde2fb;
}
@media (prefers-color-scheme:dark){:root:not([data-theme=light]){
  color-scheme:dark;
  --page:#0d0d0d; --surface:#1a1a19; --ink:#fff; --ink-2:#c3c2b7; --muted:#898781;
  --grid:#2c2c2a; --axis:#383835; --series:#3987e5; --good:#0ca30c; --warn:#fab219; --bad:#d03b3b;
  --border:rgba(255,255,255,.10); --wash:rgba(57,135,229,.18);
  --c1:#3987e5; --c2:#d95926; --c3:#199e70; --c4:#c98500;
  --meter-track:#0d366b;
}}
:root[data-theme=dark]{
  color-scheme:dark;
  --page:#0d0d0d; --surface:#1a1a19; --ink:#fff; --ink-2:#c3c2b7; --muted:#898781;
  --grid:#2c2c2a; --axis:#383835; --series:#3987e5; --good:#0ca30c; --warn:#fab219; --bad:#d03b3b;
  --border:rgba(255,255,255,.10); --wash:rgba(57,135,229,.18);
  --c1:#3987e5; --c2:#d95926; --c3:#199e70; --c4:#c98500;
  --meter-track:#0d366b;
}
*{box-sizing:border-box}
body{margin:0;background:var(--page);color:var(--ink);
  font:15px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif;padding:24px 20px 64px}
/* minmax(0,1fr): an auto track sizes to max-content and would let the tile
   grid push the page wider than the viewport on narrow screens. */
.wrap{max-width:1080px;margin:0 auto;display:grid;grid-template-columns:minmax(0,1fr);gap:20px}
/* inverted pyramid: tiles up top, then paired live panels, reference last.
   Two columns only when there is room; .full spans both either way. */
@media (min-width:960px){
  .wrap{grid-template-columns:repeat(2,minmax(0,1fr))}
  header,.tiles,.full{grid-column:1 / -1}
}
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
/* populations trio — three labeled figures, not a chart */
.pops{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin:10px 0 16px}
.pops .k{color:var(--muted);font-size:12px;text-transform:uppercase;letter-spacing:.05em}
.pops .v{font-size:24px;letter-spacing:-.02em;margin-top:2px}
.pops .d{color:var(--ink-2);font-size:12px}
/* outcome split — one horizontal stacked bar, segments gapped 2px */
.stack{display:flex;height:26px;border-radius:5px;overflow:hidden;gap:2px;margin:8px 0 6px}
.stack i{min-width:3px}
.skey{display:flex;flex-wrap:wrap;gap:4px 16px;font-size:12px;color:var(--ink-2)}
.skey .sw{display:inline-block;width:9px;height:9px;border-radius:2px;margin-right:5px}
.hist{width:100%;height:150px;display:block;overflow:visible}
.hist text{fill:var(--muted);font-size:11px}
/* db meter — track is a lighter step of the fill's own ramp */
.meter{height:10px;border-radius:5px;background:var(--meter-track);overflow:hidden;margin:6px 0 4px}
.meter i{display:block;height:100%;border-radius:5px;background:var(--series);min-width:2px}
.meter.warn i{background:var(--warn)} .meter.bad i{background:var(--bad)}
#dtab{min-width:0}
/* tech center — reference typography, quieter than the live panels */
.tech h3{font-size:12px;margin:16px 0 6px;color:var(--muted);text-transform:uppercase;letter-spacing:.05em}
.tech table{font-size:13px}
.tech td,.tech th{white-space:normal;text-align:left}
.tech .flow{font-family:ui-monospace,SFMono-Regular,Menlo,monospace;font-size:12.5px;
  color:var(--ink-2);background:color-mix(in srgb,var(--ink) 4%,transparent);
  border-radius:8px;padding:10px 12px;overflow-x:auto;white-space:pre}
.tech code{font-family:ui-monospace,SFMono-Regular,Menlo,monospace;font-size:12.5px}
.err{color:#d03b3b;font-size:13px}
</style></head><body>
<div class="wrap">
<header>
  <div><h1>Reactor Drone — Live Ops</h1><div class="sub" id="rel">&nbsp;</div></div>
  <div class="live"><span class="dot" id="dot"></span><span id="upd">connecting…</span>
    <button id="refresh">Refresh</button></div>
</header>

<div class="tiles" id="tiles"></div>

<section class="card full">
  <h2>Runs per day</h2><div class="sub" id="actsub">last 14 days</div>
  <svg class="chart" id="act" role="img" aria-label="Runs per day, last 14 days"></svg>
</section>

<section class="card full">
  <h2>Telemetry</h2><div class="sub">per-run reports · ANALYTICS-on players only</div>
  <div class="pops" id="pops"></div>
  <h3 class="sub" style="margin:8px 0 0">How runs end</h3>
  <div class="stack" id="ostack"></div><div class="skey" id="okey"></div>
  <h3 class="sub" style="margin:16px 0 0">Waves reached</h3>
  <svg class="hist" id="whist" role="img" aria-label="Runs by wave reached"></svg>
  <div class="empty" id="tempty" hidden>No telemetry yet.</div>
</section>

<section class="card full">
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

<section class="card">
  <h2>Database</h2><div class="sub">D1 reactor-drone-db · free tier</div>
  <div class="scroll"><table id="dtab">
    <thead><tr><th class="lead">Table</th><th>Rows</th></tr></thead><tbody></tbody>
  </table></div>
  <div class="sub" style="margin-top:12px">Writes today <span id="wpct"></span></div>
  <div class="meter" id="wmeter"><i style="width:0"></i></div>
  <div class="sub">of 100,000 rows/day (free tier); one row per run by design</div>
</section>

<section class="card full tech">
  <h2>Tech center</h2><div class="sub">the full stack on one screen · updated with the code it describes</div>
  <h3>Request flow</h3>
  <div class="flow">game (main.cpp) --net::post_json + X-Game-Key--> Worker reactor-drone-api --D1 binding--> reactor-drone-db
website signup form --fetch, CORS--> Worker /subscribe ----------------------------^
this page ----------Basic auth----> Worker /stats (6-query batch) ------------------^</div>
  <h3>Routes <span style="text-transform:none;letter-spacing:0">(checked against worker.js by verify_branch.sh)</span></h3>
  <div class="scroll"><table id="routes">
    <thead><tr><th>Route</th><th>Auth</th><th>Writes</th><th>Notes</th></tr></thead>
    <tbody>
    <tr><td><code>GET /version</code></td><td>public</td><td>—</td><td>update check; version + installer URL from Worker vars</td></tr>
    <tr><td><code>POST /register</code></td><td>public</td><td>players</td><td>UUID + pilot name; 409 on a taken name (NOCASE)</td></tr>
    <tr><td><code>POST /score</code></td><td>X-Game-Key</td><td>scores</td><td>score 0–10M; player must exist</td></tr>
    <tr><td><code>GET /top</code></td><td>public</td><td>—</td><td>leaderboard, high|total, top 20, names only</td></tr>
    <tr><td><code>POST /telemetry</code></td><td>X-Game-Key</td><td>runs</td><td>one run report, 16KB cap; consent = ANALYTICS toggle</td></tr>
    <tr><td><code>POST /feedback</code></td><td>X-Game-Key</td><td>feedback</td><td>player-written report + auto context, 8KB cap</td></tr>
    <tr><td><code>POST /subscribe</code></td><td>public</td><td>subscribers</td><td>dedupes by address; reply never reveals membership</td></tr>
    <tr><td><code>GET|POST /unsubscribe</code></td><td>token</td><td>subscribers</td><td>GET offers, POST deletes (prefetch-safe)</td></tr>
    <tr><td><code>GET /dashboard</code></td><td>Basic</td><td>—</td><td>this page</td></tr>
    <tr><td><code>GET /stats</code></td><td>Basic</td><td>—</td><td>every panel here; never emits player_id</td></tr>
    </tbody>
  </table></div>
  <h3>Where things live</h3>
  <div class="scroll"><table>
    <tbody>
    <tr><td class="lead">Client net calls</td><td><code>CPP/game/main.cpp</code> — register, score, telemetry, feedback, subscribe; <code>net_config.hpp</code> pins NET_BASE (compiled into every shipped binary — do not move the Worker)</td></tr>
    <tr><td class="lead">Worker + page</td><td><code>backend/src/worker.js</code>, <code>dashboard.js</code>; config <code>wrangler.jsonc</code>; secrets GAME_KEY, DASH_PASS</td></tr>
    <tr><td class="lead">Schema</td><td><code>backend/schema.sql</code> — players, scores, runs, feedback, subscribers (all CREATE IF NOT EXISTS; migrate with d1 execute --remote)</td></tr>
    <tr><td class="lead">Releases</td><td><code>.github/workflows/release.yml</code> on tag v* — Windows installer + portable zip, Linux tarball, mac .app, itch via butler</td></tr>
    <tr><td class="lead">Decisions</td><td><code>agentProjectDocs/decisions.md</code> D195–D203 cover this subsystem; specs in <code>agentProjectDocs/specs/</code></td></tr>
    </tbody>
  </table></div>
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

// Outcome slots are assigned once, in canonical order — color follows the
// entity, never its rank, so a quiet outcome keeps its hue forever.
// Segment order IS the palette's validated adjacency (blue-orange-aqua-yellow):
// death,quit,victory,close. Reordering these repaints nothing (color follows
// the outcome) but changes which hues sit side by side — re-run the validator.
var OUTCOMES = [
  { k: 'death',   label: 'Death',   c: 'var(--c1)' },
  { k: 'quit',    label: 'Quit',    c: 'var(--c2)' },
  { k: 'victory', label: 'Victory', c: 'var(--c3)' },
  { k: 'close',   label: 'Closed',  c: 'var(--c4)' }
];

function drawOutcomes(rows){
  var by = {}, total = 0, i;
  for (i = 0; i < rows.length; i++){ by[rows[i].outcome] = rows[i].n; total += rows[i].n; }
  var bar = '', key = '';
  for (i = 0; i < OUTCOMES.length; i++){
    var o = OUTCOMES[i], n = by[o.k] || 0;
    var pct = total ? (n / total * 100) : 0;
    if (n) bar += '<i style="flex:' + n + ' 0 0;background:' + o.c
      + '" data-o="' + i + '"></i>';
    // the key doubles as the direct label: name + count, ink not series color
    key += '<span><span class="sw" style="background:' + o.c + '"></span>'
      + o.label + ' <b>' + fmt(n) + '</b>'
      + (total ? ' <span style="color:var(--muted)">' + (n/total*100).toFixed(0) + '%</span>' : '')
      + '</span>';
  }
  $('ostack').innerHTML = bar;
  $('okey').innerHTML = key;
  $('ostack').onmousemove = function(e){
    var t = e.target.getAttribute && e.target.getAttribute('data-o');
    if (t == null) return hideTip();
    var o = OUTCOMES[+t], n = by[o.k] || 0;
    showTip(e, '<b>' + o.label + '</b><br>' + fmt(n) + ' of ' + fmt(total) + ' runs');
  };
  $('ostack').onmouseleave = hideTip;
}

var waves = [];
function drawWaves(){
  var svg = $('whist');
  var W = Math.max(280, Math.round(svg.getBoundingClientRect().width) || 720);
  var H = 150, padL = 30, padR = 8, padT = 8, padB = 24;
  svg.setAttribute('viewBox', '0 0 ' + W + ' ' + H);
  var iw = W - padL - padR, ih = H - padT - padB;
  // continuous 0..maxWave axis so gaps read as zero, not as missing bars
  var maxW = 0, by = {}, max = 0, i;
  for (i = 0; i < waves.length; i++){
    maxW = Math.max(maxW, waves[i].wave); by[waves[i].wave] = waves[i].n;
    max = Math.max(max, waves[i].n);
  }
  var top = Math.max(1, max), n = maxW + 1;
  var s = '', ticks = [0, Math.round(top/2), top].filter(function(v,j,a){ return a.indexOf(v)===j; });
  for (i = 0; i < ticks.length; i++){
    var gy = padT + ih - (ticks[i]/top)*ih;
    s += '<line x1="' + padL + '" y1="' + gy + '" x2="' + (W-padR) + '" y2="' + gy
      + '" stroke="var(--grid)" stroke-width="1"/>'
      + '<text x="' + (padL-8) + '" y="' + (gy+4) + '" text-anchor="end">' + ticks[i] + '</text>';
  }
  var slot = iw / n, gap = 2, bw = Math.max(2, slot - gap);
  var lblStep = Math.max(1, Math.ceil(26 / slot));
  for (i = 0; i < n; i++){
    var v = by[i] || 0;
    var bh = (v/top)*ih;
    var x = padL + i*slot + (slot-bw)/2, y = padT + ih - bh;
    if (v) s += '<path d="' + barPath(x, y, bw, Math.max(bh, 2), Math.min(4, bw/2))
      + '" fill="var(--series)" data-w="' + i + '"/>';
    s += '<rect x="' + (padL + i*slot) + '" y="' + padT + '" width="' + slot + '" height="' + ih
      + '" fill="transparent" data-w="' + i + '"/>';
    if (i % lblStep === 0)
      s += '<text x="' + (x + bw/2) + '" y="' + (H-6) + '" text-anchor="middle">' + i + '</text>';
  }
  s += '<line x1="' + padL + '" y1="' + (padT+ih) + '" x2="' + (W-padR) + '" y2="' + (padT+ih)
    + '" stroke="var(--axis)" stroke-width="1"/>';
  svg.innerHTML = s;
  svg.onmousemove = function(e){
    var t = e.target.getAttribute && e.target.getAttribute('data-w');
    if (t == null) return hideTip();
    var v = by[+t] || 0;
    showTip(e, '<b>Wave ' + t + '</b><br>' + fmt(v) + (v === 1 ? ' run ended here' : ' runs ended here'));
  };
  svg.onmouseleave = hideTip;
}

function drawPops(p){
  $('pops').innerHTML =
      '<div><div class="k">Registered</div><div class="v">' + fmt(p.reg)
    + '</div><div class="d">completed name entry</div></div>'
    + '<div><div class="k">Banked a score</div><div class="v">' + fmt(p.scored)
    + '</div><div class="d">registered and finished a run</div></div>'
    + '<div><div class="k">Sending telemetry</div><div class="v">' + fmt(p.telem)
    + '</div><div class="d">ANALYTICS on · can exceed registered (ESC skips name entry, not stats)</div></div>';
}

function drawDb(db){
  var tables = ['players','scores','runs','feedback','subscribers'];
  var s = '', i;
  for (i = 0; i < tables.length; i++)
    s += '<tr><td class="lead"><code>' + tables[i] + '</code></td><td>'
      + fmt(db[tables[i]]) + '</td></tr>';
  $('dtab').querySelector('tbody').innerHTML = s;
  var LIMIT = 100000;
  var pct = Math.min(100, db.writes_today / LIMIT * 100);
  $('wpct').textContent = fmt(db.writes_today) + ' · ' + (pct < 1 ? '<1' : pct.toFixed(0)) + '%';
  var m = $('wmeter');
  m.className = 'meter' + (pct >= 80 ? ' bad' : pct >= 50 ? ' warn' : '');
  m.firstElementChild.style.width = Math.max(pct, db.writes_today ? 2 : 0) + '%';
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
  var hasTelem = (data.waves || []).length > 0;
  $('tempty').hidden = hasTelem;
  drawOutcomes(data.outcomes || []);
  waves = data.waves || [];
  drawWaves();
  if (data.pops) drawPops(data.pops);
  if (data.db) drawDb(data.db);
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
  rz = setTimeout(function(){
    if (daily.length) drawActivity();
    if (waves.length) drawWaves();
  }, 120);
});
setInterval(function(){
  $('upd').textContent = lastOk ? 'updated ' + ago(Math.floor(lastOk/1000)) : 'no data';
}, 1000);
setInterval(load, POLL_MS);
// Pause polling while the tab is hidden; refresh immediately on return.
document.addEventListener('visibilitychange', function(){ if (!document.hidden) load(); });
load();
</script></body></html>`;
