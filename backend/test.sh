#!/usr/bin/env bash
set -euo pipefail
BASE="${BASE:?}" KEY="${KEY:?}"
c() { curl -s -o /dev/null -w '%{http_code}' "$@"; }
J='content-type: application/json'
U1=11111111-aaaa-bbbb-cccc-000000000001 U2=11111111-aaaa-bbbb-cccc-000000000002
[ "$(c -XPOST "$BASE/register" -H "$J" -d "{\"player_id\":\"$U1\",\"name\":\"testconrad\"}")" = 200 ]
[ "$(c -XPOST "$BASE/register" -H "$J" -d "{\"player_id\":\"$U2\",\"name\":\"TESTCONRAD\"}")" = 409 ]  # case-insensitive unique
[ "$(c -XPOST "$BASE/score" -H "$J" -d "{\"player_id\":\"$U1\",\"score\":100}")" = 401 ]               # no key
[ "$(c -XPOST "$BASE/score" -H "$J" -H "X-Game-Key: $KEY" -d "{\"player_id\":\"$U1\",\"score\":100}")" = 200 ]
[ "$(c -XPOST "$BASE/score" -H "$J" -H "X-Game-Key: $KEY" -d "{\"player_id\":\"$U1\",\"score\":250}")" = 200 ]
curl -s "$BASE/top?mode=high"  | grep -q '"score":250'
curl -s "$BASE/top?mode=total" | grep -q '"score":350'
curl -s "$BASE/version" | grep -q '"version"'

# dashboard + stats (read-only, no game key)
[ "$(c "$BASE/dashboard")" = 200 ]
curl -s "$BASE/dashboard" | grep -q 'Reactor Drone — Live Ops'
S=$(curl -s "$BASE/stats")
echo "$S" | grep -q '"totals"'
echo "$S" | grep -q '"players"'
echo "$S" | grep -q '"recent"'
echo "$S" | grep -q '"best_name"'
# the activity series is always a full 14 days, quiet days included
[ "$(echo "$S" | grep -o '"d":"' | wc -l)" = 14 ]
# /stats must never leak a player_id
! echo "$S" | grep -q 'player_id'
echo ALL PASS
