import { DASHBOARD_HTML } from './dashboard.js';

// `access-control-allow-origin: *` on every JSON reply: the signup form on the
// website is a cross-origin caller, and nothing here returns anything a browser
// couldn't already fetch server-side. The one keyed route (/score, /telemetry,
// /feedback) is protected by X-Game-Key, not by origin.
const json = (obj, status = 200) =>
  new Response(JSON.stringify(obj), {
    status,
    headers: { 'content-type': 'application/json', 'access-control-allow-origin': '*' }
  });

const html = (body, status = 200) =>
  new Response(PAGE(body), { status, headers: { 'content-type': 'text/html;charset=utf-8' } });

const UUID_RE = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const NAME_RE = /^[\x20-\x7e]+$/;
const MAX_BODY_BYTES = 2048; // these payloads are tiny; reject anything absurd before parsing
const MAX_TELEMETRY_BYTES = 16384; // one run report; ~3-5 KB typical
const OUTCOMES = new Set(['death', 'victory', 'quit', 'close']);
const MAX_FEEDBACK_BYTES = 8192;
const PLATFORMS = new Set(['win', 'linux', 'mac']);
// Deliberately loose: the only address check that actually proves anything is a
// confirmation mail, and this list is single-opt-in for now. This just catches
// typos and obvious junk before they take a row.
const EMAIL_RE = /^[^@\s,;]+@[^@\s,;]+\.[a-z]{2,}$/i;
const SOURCES = new Set(['web', 'game']);

// Unsubscribe pages. Plain, self-contained, no assets to 404.
const PAGE = (body) => `<!doctype html><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Reactor Drone mailing list</title>
<style>body{background:#0d1117;color:#c9d1d9;font:16px/1.6 system-ui,sans-serif;
display:grid;place-items:center;min-height:100vh;margin:0;text-align:center}
main{max-width:34rem;padding:2rem}h1{font-size:1.4rem;letter-spacing:.08em;color:#e6edf3}
button{background:#f0883e;color:#0d1117;border:0;border-radius:4px;padding:.7rem 1.6rem;
font:inherit;font-weight:600;cursor:pointer}p{color:#8b949e}</style>
<main>${body}</main>`;

// Reads and JSON-parses the body, refusing to buffer an oversized payload.
async function readJson(req) {
  const len = req.headers.get('content-length');
  if (len && Number(len) > MAX_BODY_BYTES) throw new Error('body_too_large');
  const text = await req.text();
  if (text.length > MAX_BODY_BYTES) throw new Error('body_too_large');
  return JSON.parse(text);
}

const DAYS = 14; // activity window on the dashboard

// SQL only returns days that had runs; the chart needs the quiet days too.
function fillDays(rows) {
  const byDay = new Map(rows.map(r => [r.d, r]));
  const out = [];
  const cursor = new Date();
  cursor.setUTCDate(cursor.getUTCDate() - (DAYS - 1));
  for (let i = 0; i < DAYS; i++) {
    const d = cursor.toISOString().slice(0, 10);
    out.push(byDay.get(d) ?? { d, n: 0, best: 0 });
    cursor.setUTCDate(cursor.getUTCDate() + 1);
  }
  return out;
}

// Constant-time compare so the game key can't be brute-forced via response timing.
function safeEqual(a, b) {
  const enc = new TextEncoder();
  const ab = enc.encode(a ?? '');
  const bb = enc.encode(b ?? '');
  if (ab.byteLength !== bb.byteLength) return false;
  return crypto.subtle.timingSafeEqual(ab, bb);
}

export default {
  async fetch(req, env) {
    const url = new URL(req.url);
    try {
      // Preflight for the website signup form (content-type: application/json
      // makes it a non-simple request).
      if (req.method === 'OPTIONS')
        return new Response(null, { status: 204, headers: {
          'access-control-allow-origin': '*',
          'access-control-allow-methods': 'POST, GET, OPTIONS',
          'access-control-allow-headers': 'content-type',
          'access-control-max-age': '86400'
        }});

      // /dashboard and /stats are the only non-public routes here: they show
      // subscriber addresses and untrusted player-written feedback bodies.
      // Basic auth rather than a token — the browser holds the credential and
      // replays it on the page's own fetch('/stats') polls with no client code,
      // and it never lands in history or a Referer header the way ?k= would.
      // Fails closed: an unset DASH_PASS must not mean an open door.
      if (url.pathname === '/dashboard' || url.pathname === '/stats' ||
          url.pathname === '/clear') {
        const want = env.DASH_PASS ? 'Basic ' + btoa('dev:' + env.DASH_PASS) : null;
        if (!want || !safeEqual(req.headers.get('authorization'), want))
          return new Response('Authentication required\n', { status: 401, headers: {
            'www-authenticate': 'Basic realm="reactor-drone-ops", charset="UTF-8"'
          }});
      }

      // Destructive ops, behind the same Basic auth as /dashboard (see the guard
      // above — this path is listed there, and adding a route under /admin
      // WITHOUT listing it there would publish a public delete button).
      //
      // The X-Confirm header is the CSRF guard: a cross-site <form> POST can
      // carry the browser's Basic credential, but it cannot set a custom header
      // without a preflight this Worker never approves. The typed confirmation
      // in the page is for the human; this is for the browser.
      if (req.method === 'POST' && url.pathname === '/clear') {
        if (!safeEqual(req.headers.get('x-confirm'), 'CLEAR'))
          return json({ error: 'confirmation required' }, 400);
        const what = url.searchParams.get('what');
        // Fixed statement lists keyed off a literal — `what` never reaches SQL.
        // 'stats' deliberately does NOT touch `players`: pilot names are the one
        // thing an installed game holds a reference to (meta.json carries the
        // player_id), so dropping them would strand every client that ever
        // registered.
        const sets = {
          stats: ['DELETE FROM scores', 'DELETE FROM runs', 'DELETE FROM feedback'],
          subscribers: ['DELETE FROM subscribers']
        };
        const stmts = sets[what];
        if (!stmts) return json({ error: 'unknown target' }, 400);
        const res = await env.DB.batch(stmts.map(q => env.DB.prepare(q)));
        const cleared = res.reduce((n, r) => n + (r.meta ? r.meta.changes || 0 : 0), 0);
        return json({ ok: true, what, cleared });
      }

      if (req.method === 'GET' && url.pathname === '/version')
        return json({ version: env.RELEASE_VERSION, installer_url: env.INSTALLER_URL });

      if (req.method === 'GET' && url.pathname === '/top') {
        // `agg` is chosen from a fixed pair of SQL literals keyed off a boolean
        // check on the query string — the query string itself is never
        // concatenated into the SQL.
        const mode = url.searchParams.get('mode') === 'total' ? 'total' : 'high';
        const agg = mode === 'total' ? 'SUM(s.score)' : 'MAX(s.score)';
        const { results } = await env.DB.prepare(
          `SELECT p.name AS name, ${agg} AS score FROM scores s JOIN players p ON p.id = s.player_id
           GROUP BY s.player_id ORDER BY score DESC LIMIT 20`).all();
        return json({ rows: results });
      }

      if (req.method === 'GET' && url.pathname === '/dashboard')
        return new Response(DASHBOARD_HTML, {
          headers: { 'content-type': 'text/html;charset=utf-8', 'cache-control': 'public, max-age=300' }
        });

      // Read-only aggregates behind /dashboard. Exposes nothing /top doesn't
      // already make public (names + scores); no player_id ever leaves here.
      if (req.method === 'GET' && url.pathname === '/stats') {
        const [totals, daily, players, recent, feedback, subs,
               outcomes, waves, pops, db] = await env.DB.batch([
          env.DB.prepare(`SELECT
            (SELECT COUNT(*) FROM players) AS players,
            (SELECT COUNT(*) FROM scores)  AS runs,
            (SELECT COALESCE(MAX(score), 0) AS x FROM scores) AS best,
            (SELECT COALESCE(SUM(score), 0) AS x FROM scores) AS total_score,
            (SELECT COALESCE(CAST(AVG(score) AS INTEGER), 0) AS x FROM scores) AS avg,
            (SELECT COUNT(*) FROM scores WHERE ts >= unixepoch() - 86400) AS runs_24h,
            (SELECT COUNT(DISTINCT player_id) FROM scores WHERE ts >= unixepoch() - 86400) AS players_24h,
            (SELECT p.name FROM scores s JOIN players p ON p.id = s.player_id
             ORDER BY s.score DESC LIMIT 1) AS best_name,
            (SELECT COUNT(*) FROM subscribers) AS subs,
            (SELECT COUNT(*) FROM subscribers WHERE ts >= unixepoch() - 86400) AS subs_24h,
            (SELECT COUNT(*) FROM feedback)    AS fb,
            (SELECT COUNT(*) FROM feedback WHERE ts >= unixepoch() - 86400) AS fb_24h`),
          env.DB.prepare(
            `SELECT date(s.ts, 'unixepoch') AS d, COUNT(*) AS n, MAX(s.score) AS best
             FROM scores s WHERE s.ts >= unixepoch(date('now', ?1)) GROUP BY d ORDER BY d`
          ).bind(`-${DAYS - 1} days`),
          // LEFT JOIN so a registered pilot who has not banked a run still appears.
          env.DB.prepare(
            `SELECT p.name AS name, COUNT(s.player_id) AS runs,
                    COALESCE(MAX(s.score), 0) AS best, COALESCE(SUM(s.score), 0) AS total,
                    COALESCE(CAST(AVG(s.score) AS INTEGER), 0) AS avg,
                    COALESCE(MAX(s.ts), 0) AS last
             FROM players p LEFT JOIN scores s ON s.player_id = p.id
             GROUP BY p.id ORDER BY best DESC LIMIT 100`),
          env.DB.prepare(
            `SELECT p.name AS name, s.score AS score, s.ts AS ts
             FROM scores s JOIN players p ON p.id = s.player_id
             ORDER BY s.ts DESC LIMIT 25`),
          // Feedback inbox and mailing list. Both are why this route is now
          // authenticated; neither may ever be echoed by a public route.
          env.DB.prepare(
            `SELECT ts, subject, body, tags, from_name, pilot, version, platform,
                    in_run, wave, score, difficulty
             FROM feedback ORDER BY ts DESC LIMIT 30`),
          env.DB.prepare(
            `SELECT email, source, ts FROM subscribers ORDER BY ts DESC LIMIT 50`),
          // Telemetry aggregates (specs/dashboard-telemetry-and-status.md).
          // Per-run rows, so COUNT(*) is runs; indexed columns, no json_extract.
          env.DB.prepare(
            `SELECT outcome, COUNT(*) AS n FROM runs GROUP BY outcome`),
          env.DB.prepare(
            `SELECT wave, COUNT(*) AS n FROM runs GROUP BY wave ORDER BY wave`),
          // The three populations are three different numbers by design:
          // registration, banking a score, and telemetry are separate consents
          // (ESC skips name entry but not analytics), so telem may exceed reg.
          env.DB.prepare(`SELECT
            (SELECT COUNT(*) FROM players) AS reg,
            (SELECT COUNT(DISTINCT player_id) FROM scores) AS scored,
            (SELECT COUNT(DISTINCT player_id) FROM runs) AS telem`),
          // DB status: per-table rows, plus today's writes vs the D1 free
          // tier's 100K rows/day that drove the one-row-per-run design.
          env.DB.prepare(`SELECT
            (SELECT COUNT(*) FROM players)     AS players,
            (SELECT COUNT(*) FROM scores)      AS scores,
            (SELECT COUNT(*) FROM runs)        AS runs,
            (SELECT COUNT(*) FROM feedback)    AS feedback,
            (SELECT COUNT(*) FROM subscribers) AS subscribers,
              (SELECT (SELECT COUNT(*) FROM scores      WHERE ts >= unixepoch('now','start of day'))
                    + (SELECT COUNT(*) FROM runs        WHERE ts >= unixepoch('now','start of day'))
                    + (SELECT COUNT(*) FROM feedback    WHERE ts >= unixepoch('now','start of day'))
                    + (SELECT COUNT(*) FROM subscribers WHERE ts >= unixepoch('now','start of day'))) AS writes_today`)
        ]);
        return new Response(JSON.stringify({
          version: env.RELEASE_VERSION,
          totals: totals.results[0],
          daily: fillDays(daily.results),
          players: players.results,
          recent: recent.results,
          feedback: feedback.results,
          subs: subs.results,
          outcomes: outcomes.results,
          waves: waves.results,
          pops: pops.results[0],
          db: db.results[0]
        }), { headers: { 'content-type': 'application/json', 'cache-control': 'no-store' } });
      }

      if (req.method === 'POST' && url.pathname === '/register') {
        const { player_id, name } = await readJson(req);
        if (typeof player_id !== 'string' || !UUID_RE.test(player_id)) return json({ error: 'bad_request' }, 400);
        if (typeof name !== 'string') return json({ error: 'bad_request' }, 400);
        const trimmed = name.trim();
        if (!trimmed || trimmed.length > 24 || !NAME_RE.test(trimmed)) return json({ error: 'bad_request' }, 400);
        try {
          await env.DB.prepare(
            'INSERT INTO players (id, name) VALUES (?1, ?2) ON CONFLICT(id) DO UPDATE SET name = ?2')
            .bind(player_id, trimmed).run();
        } catch (e) {
          if (String(e).includes('UNIQUE')) return json({ error: 'name_taken' }, 409);
          throw e;
        }
        return json({ ok: true });
      }

      if (req.method === 'POST' && url.pathname === '/score') {
        if (!safeEqual(req.headers.get('X-Game-Key'), env.GAME_KEY)) return json({ error: 'unauthorized' }, 401);
        const { player_id, score } = await readJson(req);
        if (typeof player_id !== 'string' || !UUID_RE.test(player_id)) return json({ error: 'bad_request' }, 400);
        if (!Number.isInteger(score) || score < 0 || score > 10_000_000) return json({ error: 'bad_request' }, 400);
        const player = await env.DB.prepare('SELECT id FROM players WHERE id = ?1').bind(player_id).first();
        if (!player) return json({ error: 'unknown_player' }, 400);
        await env.DB.prepare('INSERT INTO scores (player_id, score) VALUES (?1, ?2)').bind(player_id, score).run();
        return json({ ok: true });
      }

      // One per-run summary. Deliberately does NOT require a registered player
      // row — a report from a never-registered install is still data. The raw
      // text is stored verbatim in `body`, so this reads the request itself
      // rather than going through readJson().
      if (req.method === 'POST' && url.pathname === '/telemetry') {
        if (!safeEqual(req.headers.get('X-Game-Key'), env.GAME_KEY)) return json({ error: 'unauthorized' }, 401);
        // Same refuse-before-buffering discipline as readJson().
        const len = req.headers.get('content-length');
        if (len && Number(len) > MAX_TELEMETRY_BYTES) return json({ error: 'bad_request' }, 400);
        const raw = await req.text();
        if (raw.length > MAX_TELEMETRY_BYTES) return json({ error: 'bad_request' }, 400);
        const b = JSON.parse(raw); // SyntaxError -> catch -> 400
        const str = (v, max) => typeof v === 'string' && v.length > 0 && v.length <= max;
        const int = (v, lo, hi) => Number.isInteger(v) && v >= lo && v <= hi;
        if (!int(b.v, 1, 99) || typeof b.player_id !== 'string' || !UUID_RE.test(b.player_id) ||
            !str(b.session_id, 64) || !str(b.game_version, 32) || !str(b.difficulty, 32) ||
            !int(b.prestige, 0, 99) || !int(b.ship, -1, 99) || !OUTCOMES.has(b.outcome) ||
            !int(b.wave, 0, 999) || !int(b.score, 0, 10_000_000) ||
            typeof b.dur_s !== 'number' || !Number.isFinite(b.dur_s) || b.dur_s < 0 || b.dur_s > 86400)
          return json({ error: 'bad_request' }, 400);
        await env.DB.prepare(
          `INSERT INTO runs (player_id, session, version, difficulty, prestige, ship, outcome, wave, score, dur_s, body)
           VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)`)
          .bind(b.player_id, b.session_id, b.game_version, b.difficulty, b.prestige, b.ship,
                b.outcome, b.wave, b.score, Math.round(b.dur_s), raw).run();
        return json({ ok: true });
      }

      // Explicit player action — deliberately not gated on any consent flag
      // beyond the submit itself. Flat columns: this table is the AI export.
      if (req.method === 'POST' && url.pathname === '/feedback') {
        if (!safeEqual(req.headers.get('X-Game-Key'), env.GAME_KEY)) return json({ error: 'unauthorized' }, 401);
        const len = req.headers.get('content-length');
        if (len && Number(len) > MAX_FEEDBACK_BYTES) return json({ error: 'bad_request' }, 400);
        const raw = await req.text();
        if (raw.length > MAX_FEEDBACK_BYTES) return json({ error: 'bad_request' }, 400);
        const b = JSON.parse(raw); // SyntaxError -> catch -> 400
        // Body allows \n; everything else is single-line printable ASCII.
        const line = (v, min, max) => typeof v === 'string' && v.length >= min && v.length <= max &&
                                      (v === '' || NAME_RE.test(v));
        const text = (v, min, max) => typeof v === 'string' && v.length >= min && v.length <= max &&
                                      /^[\x20-\x7e\n]*$/.test(v) && v.trim().length >= min;
        const int = (v, lo, hi) => Number.isInteger(v) && v >= lo && v <= hi;
        if (!line(b.subject, 1, 120) || b.subject.trim().length === 0 || !text(b.body, 1, 4000) ||
            !line(b.tags ?? '', 0, 200) || !line(b.from_name ?? '', 0, 60) ||
            typeof b.player_id !== 'string' || !UUID_RE.test(b.player_id) ||
            !line(b.pilot ?? '', 0, 40) || !line(b.version, 1, 32) ||
            !PLATFORMS.has(b.platform) || !line(b.session_id, 1, 64) ||
            typeof b.in_run !== 'boolean')
          return json({ error: 'bad_request' }, 400);
        if (b.in_run && (!int(b.wave, 0, 999) || !int(b.score, 0, 10_000_000) ||
                         !int(b.ship, -1, 99) || !int(b.prestige, 0, 99) || !line(b.difficulty, 1, 32)))
          return json({ error: 'bad_request' }, 400);
        await env.DB.prepare(
          `INSERT INTO feedback (subject, body, tags, from_name, player_id, pilot, version, platform,
                                 session, in_run, wave, score, ship, prestige, difficulty)
           VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15)`)
          .bind(b.subject.trim(), b.body, b.tags ?? '', b.from_name ?? '', b.player_id, b.pilot ?? '',
                b.version, b.platform, b.session_id, b.in_run ? 1 : 0,
                b.in_run ? b.wave : null, b.in_run ? b.score : null, b.in_run ? b.ship : null,
                b.in_run ? b.prestige : null, b.in_run ? b.difficulty : null).run();
        return json({ ok: true });
      }

      // Mailing list signup. Unkeyed like /register: the website form is a
      // browser caller and can't hold a secret. Always answers ok, whether the
      // row was new or already there — the response must not tell a stranger
      // whether an address is on the list.
      if (req.method === 'POST' && url.pathname === '/subscribe') {
        const { email, source } = await readJson(req);
        if (typeof email !== 'string') return json({ error: 'bad_request' }, 400);
        const addr = email.trim();
        if (addr.length > 254 || !EMAIL_RE.test(addr)) return json({ error: 'bad_email' }, 400);
        const src = SOURCES.has(source) ? source : 'web';
        await env.DB.prepare(
          `INSERT INTO subscribers (email, token, source) VALUES (?1, ?2, ?3)
           ON CONFLICT(email) DO NOTHING`).bind(addr, crypto.randomUUID(), src).run();
        return json({ ok: true });
      }

      // GET only *offers* to unsubscribe; the POST does it. Mail clients and
      // link scanners prefetch every URL in a message, so a GET that deleted
      // the row would unsubscribe people who never clicked.
      if (url.pathname === '/unsubscribe' && (req.method === 'GET' || req.method === 'POST')) {
        const token = req.method === 'POST'
          ? (await req.formData()).get('t')
          : url.searchParams.get('t');
        if (typeof token !== 'string' || !UUID_RE.test(token))
          return html('<h1>Bad unsubscribe link</h1><p>That link is not valid. ' +
                      'Reply to any of the mail and I will take you off by hand.</p>', 400);
        if (req.method === 'GET')
          return html('<h1>Leave the Reactor Drone list?</h1>' +
                      '<p>You will stop getting release and update mail.</p>' +
                      `<form method=post><input type=hidden name=t value="${token}">` +
                      '<button type=submit>Unsubscribe</button></form>');
        await env.DB.prepare('DELETE FROM subscribers WHERE token = ?1').bind(token).run();
        // Unconditional confirmation: an already-removed token must read the
        // same as a live one, or this page becomes an address oracle.
        return html('<h1>Done — you are off the list</h1>' +
                    '<p>No more mail. Thanks for playing.</p>');
      }

      return json({ error: 'not_found' }, 404);
    } catch (e) {
      // Never leak internal error detail to the client.
      if (e instanceof SyntaxError || e.message === 'body_too_large') return json({ error: 'bad_request' }, 400);
      console.error('unhandled worker error:', e);
      return json({ error: 'server_error' }, 500);
    }
  }
};
