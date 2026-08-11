const json = (obj, status = 200) =>
  new Response(JSON.stringify(obj), { status, headers: { 'content-type': 'application/json' } });

const UUID_RE = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const NAME_RE = /^[\x20-\x7e]+$/;
const MAX_BODY_BYTES = 2048; // these payloads are tiny; reject anything absurd before parsing

// Reads and JSON-parses the body, refusing to buffer an oversized payload.
async function readJson(req) {
  const len = req.headers.get('content-length');
  if (len && Number(len) > MAX_BODY_BYTES) throw new Error('body_too_large');
  const text = await req.text();
  if (text.length > MAX_BODY_BYTES) throw new Error('body_too_large');
  return JSON.parse(text);
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

      return json({ error: 'not_found' }, 404);
    } catch (e) {
      // Never leak internal error detail to the client.
      if (e instanceof SyntaxError || e.message === 'body_too_large') return json({ error: 'bad_request' }, 400);
      console.error('unhandled worker error:', e);
      return json({ error: 'server_error' }, 500);
    }
  }
};
