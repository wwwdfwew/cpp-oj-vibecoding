# cpp-oj-vibecoding

A lightweight, single-user Online Judge inspired by LeetCode. Backend in C++17
(`cpp-httplib` + MySQL), frontend in vanilla JS with CodeMirror 6.

See [SPEC.md](./SPEC.md) for the full design.

## Status (Phase 1–6 done)

| Phase | Scope | Status |
|---|---|---|
| 1 | Scaffold + schema + init script | [x] done |
| 2 | Auth, sessions, login / logout | [x] done |
| 3 | Problem CRUD (create / list / delete) | [x] done |
| 4 | Judge (compile + run + diff, AC/WA/CE/TLE/MLE/RE) | [x] done |
| 5 | Frontend SPA + CodeMirror 6 + draft persistence | [x] done |
| 6 | E2E verification, seed problems, security & perf tests | [x] done |

The service is **functional end-to-end**: it serves the static SPA, all JSON
endpoints, admin auth, and a working judge (g++ `fork`/`exec` with `rlimits`,
`mkdtemp` cleanup, mutex-serialized). See the smoke / E2E / perf results below.

## Requirements

| Component | Version | Source |
|---|---|---|
| g++ | ≥ 7 (host has 11.4) | system |
| cmake | ≥ 3.16 (host has 3.22) | system |
| MySQL Server | 8.0 | `apt install mysql-server` |
| libmysqlclient-dev | 8.0 | `apt install libmysqlclient-dev` |
| libssl-dev | 3.0 | `apt install libssl-dev` |
| cpp-httplib | 0.54.1 | `/usr/include/httplib.h` |
| pthread / libmysqlclient / OpenSSL | runtime | linked by CMake |
| (optional, for E2E) python3 | 3.10+ | `apt install python3` |
| (optional, for E2E) playwright + chromium | 1.x | `pip install playwright && playwright install chromium` |

## Quick start

```bash
# 1. Initialize database (creates oj DB, 4 tables, seeds admin user)
bash scripts/init_db.sh
# default admin credentials: admin / admin123

# 2. Build (Release by default)
cmake -S . -B build
cmake --build build -j$(nproc)

# 3. Seed three sample problems (A+B, Sum of N, Maximum of N)
python3 scripts/seed_problems.py

# 4. Run
OJ_DB_USER=oj OJ_DB_PASS=oj_dev_pw ./build/oj_server
# Open http://localhost:8088/
```

> **Port:** the SPEC's default port 8080 is taken on this host (by another
> service), so the default here is **8088**. Override with `OJ_HTTP_PORT`.

## MySQL user setup

The server does not use MySQL's `root`/`auth_socket` because the C++ client
connects via TCP and needs password auth. Create a dedicated user once:

```bash
sudo mysql <<'EOF'
CREATE USER 'oj'@'localhost' IDENTIFIED BY 'oj_dev_pw';
GRANT ALL PRIVILEGES ON oj.* TO 'oj'@'localhost';
FLUSH PRIVILEGES;
EOF
```

Then pass it to the server via env vars (`OJ_DB_USER=oj`, `OJ_DB_PASS=oj_dev_pw`).

`scripts/init_db.sh` uses `sudo mysql` to apply schema and seed the admin row,
so it works without an `oj` user being pre-created.

## Configuration

All settings come from environment variables:

| Variable | Default | Description |
|---|---|---|
| `OJ_HTTP_HOST` | `0.0.0.0` | Bind address |
| `OJ_HTTP_PORT` | `8088` | Bind port |
| `OJ_DB_HOST` | `localhost` | MySQL host |
| `OJ_DB_PORT` | `3306` | MySQL port |
| `OJ_DB_USER` | `root` | MySQL user |
| `OJ_DB_PASS` | (empty) | MySQL password |
| `OJ_DB_NAME` | `oj` | Database name |
| `OJ_WEB_ROOT` | `./web` | Static front-end directory |
| `OJ_GXX_PATH` | `/usr/bin/g++` | C++ compiler used by judge |
| `OJ_LOG` | `INFO` | One of `DEBUG`/`INFO`/`WARN`/`ERROR` |

## API surface

| Method | Path | Auth |
|---|---|---|
| `POST` | `/api/login` | — |
| `POST` | `/api/admin/login` | — (requires role=admin) |
| `POST` | `/api/logout` | session |
| `GET`  | `/api/problems` | — |
| `GET`  | `/api/problems/:id` | — |
| `POST` | `/api/submit` | — |
| `POST` | `/api/admin/problems` | admin |
| `DELETE` | `/api/admin/problems/:id` | admin |

Judge response format (`POST /api/submit`):

```json
{ "status": "AC", "message": "Accepted" }
{ "status": "WA", "message": "Wrong Answer", "failed_case": 0 }
{ "status": "WA", "message": "Compile Error", "compile_error": "..." }
{ "status": "WA", "message": "Time Limit Exceeded", "failed_case": 0 }
{ "status": "WA", "message": "Memory Limit Exceeded", "failed_case": 0 }
{ "status": "WA", "message": "Runtime Error (exit=1)", "failed_case": 0 }
```

See [SPEC.md §2.4](./SPEC.md) for the rest of the request/response payloads.

## Project layout

```
.
├── CMakeLists.txt
├── SPEC.md
├── README.md
├── sql/
│   └── schema.sql
├── scripts/
│   ├── init_db.sh                  # apply schema.sql + seed admin
│   ├── seed_admin.cpp              # C++ helper: print sha256+salt INSERT
│   ├── seed_problems.py            # Phase 6.1: create 3 seed problems
│   ├── e2e_submit_matrix.sh        # Phase 6.x: AC/WA/CE/RE/TLE/MLE smoke
│   ├── e2e_draft_persistence.py    # Phase 6.5: localStorage draft test
│   ├── e2e_security.py             # Phase 6.8: malicious code probes
│   └── perf_latency.py             # Phase 6.7: P95 latency check
├── src/
│   ├── main.cpp                    # httplib server entry, signal handlers
│   ├── util/                       # strutil, json, log, config
│   ├── auth/                       # password (sha256+salt), session
│   ├── db/                         # mysql_client (mutex-protected)
│   ├── judge/                      # tempdir, compiler, runner, diff, judge
│   └── routes/                     # auth, problems, submit, admin, static_files
└── web/
    ├── index.html
    └── static/
        ├── style.css
        ├── app.js                  # SPA router + shell
        └── pages/                  # problem-list, problem-detail, admin/*
```

## Smoke test (results)

Run after bringing the server up with the quick-start steps:

```
GET /                          -> 200
GET /api/problems              -> 200  (3 problems after seeding)
GET /api/problems/8            -> 200
POST /api/admin/login (good)   -> 200  {"ok":true,"role":"admin"}
POST /api/admin/login (bad)    -> 401  {"error":"invalid credentials"}
POST /api/submit (AC)          -> 200  {"status":"AC","message":"Accepted"}
POST /api/submit (WA)          -> 200  {"status":"WA","message":"Wrong Answer","failed_case":0}
POST /api/submit (CE)          -> 200  {"status":"WA","message":"Compile Error","compile_error":"..."}
POST /api/submit (TLE)         -> 200  {"status":"WA","message":"Time Limit Exceeded","failed_case":0}
POST /api/submit (MLE)         -> 200  {"status":"WA","message":"Memory Limit Exceeded","failed_case":0}
POST /api/submit (RE exit=1)   -> 200  {"status":"WA","message":"Runtime Error (exit=1)","failed_case":0}
POST /api/submit (empty code)  -> 400  {"error":"empty code"}
GET  /static/style.css         -> 200
GET  /problems/8 (SPA fb)      -> 200  (returns index.html)
POST /api/logout               -> 200
DELETE /api/admin/problems/8   -> 200  {"ok":true}
DELETE /api/admin/problems/8   -> 404  {"error":"not found"}  (idempotent)
POST /api/admin/problems (no auth)        -> 401  {"error":"unauthenticated"}
POST /api/admin/problems (non-admin user) -> 403  {"error":"admin only"}
```

## End-to-end / perf test results

Phase 6 ships a battery of automated tests. All pass on this host:

### `bash scripts/e2e_submit_matrix.sh` — AC/WA/CE/TLE/MLE/RE matrix

9 / 9 scenarios pass against the running server:

| # | Scenario | Problem | Expected | Got |
|---|---|---|---|---|
| 1 | correct A+B | A+B | AC | AC |
| 2 | prints `a*b` | A+B | Wrong Answer | Wrong Answer |
| 3 | missing semicolon | A+B | Compile Error | Compile Error |
| 4 | `return 1/0` | A+B | Runtime Error | Runtime Error |
| 5 | linear sum | Sum of N | AC | AC |
| 6 | streaming max | Maximum of N | AC | AC |
| 7 | O(N²) pairwise max | Maximum of N | Time Limit Exceeded | Time Limit Exceeded |
| 8 | `vector<int>(8e6)` (16 MB cap) | Sum of N | Memory Limit Exceeded | Memory Limit Exceeded |
| 9 | skips last element | Sum of N | Wrong Answer | Wrong Answer |

### `python3 scripts/perf_latency.py --runs 20` — P95 latency

`time_limit_ms = 1000`, 3 test cases per submission, 20 runs end-to-end:

```
=== Phase 6.7 latency over 20 runs ===
  status counts: {'AC': 20, 'WA': 0, 'ERROR': 0}
  avg = 3537.7 ms
  P50 = 3341.1 ms
  P95 = 4461.4 ms          # ≤ 5000 ms ✓
  P99 = 4488.6 ms
  max = 4495.4 ms
  -> PASS
```

### `python3 scripts/e2e_security.py` — sandbox probes

6 / 6 malicious payloads contained — server stays up:

| Payload | Result |
|---|---|
| `ifstream("/etc/passwd")` and print | WA (output ≠ expected) |
| `while(1) fork()` (RLIMIT_NPROC=1) | WA (first fork → EAGAIN) |
| `system("rm -rf ...")` | WA (exit 127, output ≠ expected) |
| `bind(22)` privileged port | WA (EACCES) |
| `while(1){}` infinite loop | TLE (CPU limit fires) |
| `vector::push_back` loop | MLE (RLIMIT_DATA fires) |

### `python3 scripts/e2e_draft_persistence.py` — frontend draft

Headless Chromium via Playwright. Boots the SPA, types a custom draft, then
verifies it survives (a) a full page reload, (b) nav to `/` and back, and
(c) is **not** leaked to other problem IDs.

## Implementation notes

### Lambda lifetime bug (fixed)

During early smoke testing, lambdas captured `[&]` to a local `auto` lambda
inside `register_*_routes` functions. When those outer registration functions
returned, the inner lambda was destroyed but the registered HTTP handlers still
held a dangling reference. The crash manifested as `MySQLClient::escape`
segfaulting because `this` was on the now-dead stack frame of the registration
function.

Fix: hoist the helper lambdas into `std::function` local variables and capture
them **by value** in the handler lambdas. Same pattern used for static file
paths and admin-guard predicate. See `src/routes/auth.cpp:30-50`,
`src/routes/admin.cpp:65-72`, `src/routes/static_files.cpp`.

### Judge safety

`Judge::run` serializes with a global mutex (`src/judge/judge.cpp:g_judge_mu`)
to keep sandbox resources predictable. Each submission runs in its own
`mkdtemp("/tmp/oj_XXXXXX")` directory; the destructor cleans up.

`Runner::run` applies:
- `RLIMIT_CPU` = `time_limit_ms / 1000 + 1` seconds
- `RLIMIT_DATA` = `memory_limit_mb` bytes (heap cap; doesn't constrain
  shared-library mappings)
- `RLIMIT_AS` = `8 × memory_limit_mb` bytes (backstop for `mmap`-based
  allocations)
- `RLIMIT_FSIZE` = 64 MB (output cap)
- `RLIMIT_NPROC` = 1 (no fork bombs)

A wall-clock kill at `time_limit_ms + 2000` ms is the final backstop. The
parent polls `wait4(WNOHANG)` every 5 ms and collects `rusage` so we can
report `time_ms` / `mem_kb`.

Memory-limit classification uses a CPU-vs-RSS heuristic: a process that ran
for less than half its CPU budget before being killed with high RSS is
classified as MLE; otherwise it's TLE. This avoids misclassifying tight TLE
kills (which fault in libraries → high RSS) as MLE.

The child's stderr is redirected to `/dev/null` so deliberately triggered
`std::bad_alloc` / SIGABRT messages from MLE probes don't leak into the
server log.

### Password hashing

`sha256(salt_hex || password)` with a 16-byte server-generated salt per user
(see `src/auth/password.cpp`, `scripts/seed_admin.cpp`). Salt and hash are
stored alongside the row.

### Frontend draft persistence

`Drafts` in `web/static/app.js` reads / writes `localStorage["draft:problem:<id>"]`.
`problem-detail.js` calls `Drafts.get(id)` to seed the editor on mount, and an
`EditorView.updateListener` calls `Drafts.set(id, ...)` on every doc change.
Verified end-to-end with `scripts/e2e_draft_persistence.py`.

### CodeMirror 6 import note

The `keymap` export lives in `@codemirror/view`, **not** in
`@codemirror/commands` (which is where `defaultKeymap`, `history`,
`historyKeymap`, and `indentWithTab` live). Pulling `keymap` from the wrong
module yields `Cannot read properties of undefined (reading 'of')` at
editor boot.

## Limitations / not yet implemented

- Submission history persistence (per SPEC §5 — out of MVP).
- HTTPS (per SPEC §5).
- Heavy sandboxing (Docker / nsjail — per SPEC §5).
- The judge runs as the same UID as the server. Within the per-submission
  `mkdtemp` (mode `0700`) and the four rlimits, a determined attacker can
  still read host files. This is acceptable for the single-user MVP per
  SPEC §6 "Risks and Trade-offs".
