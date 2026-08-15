#!/usr/bin/env bash
set -euo pipefail
BASE="${BASE:?}" KEY="${KEY:?}" DASH="${DASH:?}"
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

# telemetry
T="{\"v\":1,\"player_id\":\"$U1\",\"session_id\":\"s1\",\"game_version\":\"2.0.0\",\"difficulty\":\"Normal\",\"prestige\":0,\"ship\":0,\"outcome\":\"death\",\"wave\":7,\"score\":1200,\"dur_s\":301.5,\"heat\":{},\"waves\":[]}"
[ "$(c -XPOST "$BASE/telemetry" -H "$J" -d "$T")" = 401 ]                          # no key
[ "$(c -XPOST "$BASE/telemetry" -H "$J" -H "X-Game-Key: $KEY" -d "$T")" = 200 ]
[ "$(c -XPOST "$BASE/telemetry" -H "$J" -H "X-Game-Key: $KEY" -d '{"v":1}')" = 400 ]  # missing fields
[ "$(c -XPOST "$BASE/telemetry" -H "$J" -H "X-Game-Key: $KEY" -d "{\"v\":1,\"player_id\":\"$U1\",\"session_id\":\"s1\",\"game_version\":\"2.0.0\",\"difficulty\":\"Normal\",\"prestige\":0,\"ship\":0,\"outcome\":\"rage\",\"wave\":7,\"score\":1,\"dur_s\":1}")" = 400 ]  # bad outcome
[ "$(c -XPOST "$BASE/telemetry" -H "$J" -H "X-Game-Key: $KEY" -d "$(python3 -c 'print("{\"pad\":\""+"x"*17000+"\"}")')")" = 400 ]  # oversized
[ "$(c -XPOST "$BASE/telemetry" -H "$J" -H "X-Game-Key: $KEY" -d '{"v":1,"player_id":"nope","session_id":"s1","game_version":"2.0.0","difficulty":"Normal","prestige":0,"ship":0,"outcome":"death","wave":7,"score":1,"dur_s":1}')" = 400 ]  # bad uuid
[ "$(c -XPOST "$BASE/telemetry" -H "$J" -H "X-Game-Key: $KEY" -d "{\"v\":1,\"player_id\":\"$U1\",\"session_id\":\"s1\",\"game_version\":\"2.0.0\",\"difficulty\":\"Normal\",\"prestige\":0,\"ship\":0,\"outcome\":\"death\",\"wave\":7,\"score\":1,\"dur_s\":1,\"trailing\":}")" = 400 ]  # malformed json

# feedback
FB="{\"subject\":\"Boss too hard\",\"body\":\"wave 9 boss\\nmelts me\",\"tags\":\"balance,boss\",\"from_name\":\"conrad\",\"player_id\":\"$U1\",\"pilot\":\"testconrad\",\"version\":\"2.0.0\",\"platform\":\"linux\",\"session_id\":\"s1\",\"in_run\":true,\"wave\":9,\"score\":4200,\"ship\":1,\"prestige\":0,\"difficulty\":\"Normal\"}"
[ "$(c -XPOST "$BASE/feedback" -H "$J" -d "$FB")" = 401 ]                                # no key
[ "$(c -XPOST "$BASE/feedback" -H "$J" -H "X-Game-Key: $KEY" -d "$FB")" = 200 ]
[ "$(c -XPOST "$BASE/feedback" -H "$J" -H "X-Game-Key: $KEY" -d "{\"subject\":\"menu note\",\"body\":\"more ships pls\",\"player_id\":\"$U1\",\"version\":\"2.0.0\",\"platform\":\"win\",\"session_id\":\"s2\",\"in_run\":false}")" = 200 ]   # optionals absent
[ "$(c -XPOST "$BASE/feedback" -H "$J" -H "X-Game-Key: $KEY" -d "{\"subject\":\"\",\"body\":\"x\",\"player_id\":\"$U1\",\"version\":\"2.0.0\",\"platform\":\"win\",\"session_id\":\"s\",\"in_run\":false}")" = 400 ]  # empty subject
[ "$(c -XPOST "$BASE/feedback" -H "$J" -H "X-Game-Key: $KEY" -d "{\"subject\":\"s\",\"body\":\"x\",\"player_id\":\"$U1\",\"version\":\"2.0.0\",\"platform\":\"amiga\",\"session_id\":\"s\",\"in_run\":false}")" = 400 ]  # bad platform
[ "$(c -XPOST "$BASE/feedback" -H "$J" -H "X-Game-Key: $KEY" -d "{\"subject\":\"s\",\"body\":\"x\",\"player_id\":\"$U1\",\"version\":\"2.0.0\",\"platform\":\"win\",\"session_id\":\"s\",\"in_run\":true}")" = 400 ]  # in_run without run state

# dashboard + stats (read-only, no game key)
# the ops page is the one authenticated surface here: it shows subscriber
# addresses and player-written feedback bodies
[ "$(c "$BASE/dashboard")" = 401 ]
[ "$(c "$BASE/stats")" = 401 ]
curl -s -D- -o /dev/null "$BASE/stats" | grep -qi 'www-authenticate: Basic'
[ "$(c -u "dev:wrong$DASH" "$BASE/stats")" = 401 ]
! curl -s "$BASE/stats" | grep -q '"totals"'
[ "$(c -u "dev:$DASH" "$BASE/dashboard")" = 200 ]
curl -s -u "dev:$DASH" "$BASE/dashboard" | grep -q 'Reactor Drone — Live Ops'
S=$(curl -s -u "dev:$DASH" "$BASE/stats")
echo "$S" | grep -q '"totals"'
echo "$S" | grep -q '"players"'
echo "$S" | grep -q '"recent"'
echo "$S" | grep -q '"best_name"'
echo "$S" | grep -q '"feedback"'
echo "$S" | grep -q '"subs"'
echo "$S" | grep -q '"subs_24h"'
echo "$S" | grep -q '"fb_24h"'
echo "$S" | grep -q '"outcomes"'
echo "$S" | grep -q '"waves"'
echo "$S" | grep -q '"reg"'
echo "$S" | grep -q '"writes_today"'
# the telemetry POSTs earlier in this file must be visible in the aggregates
echo "$S" | grep -q '"outcome":"death"'
# the activity series is always a full 14 days, quiet days included
[ "$(echo "$S" | grep -o '"d":"' | wc -l)" = 14 ]
# /stats must never leak a player_id
! echo "$S" | grep -q 'player_id'

# mailing list
E1="drone.tester+$$@example.com"
[ "$(c -XPOST "$BASE/subscribe" -H "$J" -d "{\"email\":\"$E1\",\"source\":\"web\"}")" = 200 ]
[ "$(c -XPOST "$BASE/subscribe" -H "$J" -d "{\"email\":\"$E1\",\"source\":\"game\"}")" = 200 ]   # duplicate is still ok
[ "$(c -XPOST "$BASE/subscribe" -H "$J" -d '{"email":"nope"}')" = 400 ]
[ "$(c -XPOST "$BASE/subscribe" -H "$J" -d '{"email":"a b@example.com"}')" = 400 ]
[ "$(c -XPOST "$BASE/subscribe" -H "$J" -d '{"nope":1}')" = 400 ]
[ "$(c -XOPTIONS "$BASE/subscribe")" = 204 ]
curl -s -D- -o /dev/null -XPOST "$BASE/subscribe" -H "$J" -d "{\"email\":\"$E1\"}" | grep -qi 'access-control-allow-origin: \*'
# unsubscribe: GET only offers, POST performs; a bad token is a page, not a 500
TOK=00000000-1111-2222-3333-444444444444
curl -s "$BASE/unsubscribe?t=$TOK" | grep -q '<form method=post'
[ "$(c "$BASE/unsubscribe?t=garbage")" = 400 ]
curl -s -XPOST "$BASE/unsubscribe" -d "t=$TOK" | grep -q 'off the list'
echo ALL PASS
