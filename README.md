# Gorgona — a minimal HTTP server written in Algol 68

`Gorgona` is a small, self-contained HTTP server implemented in **Algol 68**. It examines the schema of an arbitrary SQLite database, derives a REST route for every table, and exposes it as JSON. A separate sub-project under `web/` provides an interactive, framework-free browser frontend for exploring the data. Which database to browse is set in a configuration file (`gorgona.conf`).

Everything that talks to the outside world (TCP sockets, SQLite, threads) is reached from a few lines of Algol 68 via `nest C` calls into minimal C wrappers. Config parsing, routing, and the HTTP logic live in Algol 68.

## Features

- **Dynamic REST API**: routes are derived at startup from `sqlite_master` — any SQLite database works, not just one hard-coded schema (`Order Details` becomes `/api/orderdetails`, …)
- **Interactive DB browser**: served at `/`, a single-page HTML/JS client (a separate sub-project in `web/`) with
  - table selection,
  - live filtering across all rows (client-side),
  - sortable columns (click on the sticky table header),
  - pagination (25 rows per page),
  - proper error/status display.
- **Concurrency**: `pthread` worker threads handle requests in parallel; one shared SQLite connection is serialized with a mutex.
- **Byte-exact I/O**: reading static files and writing socket responses are byte-for-byte exact (a dedicated Unicode Transput module in `transput.u68` sits on two minimal C primitives), including accented data (`Acústico`, `aé`, …).
- **Config in pure Algol 68**: `gorgona.conf` is read and parsed by Algol 68 code — the C wrappers only provide raw byte-exact I/O primitives.
- **No web frameworks, no dependencies beyond a C toolchain and `libsqlite3`.**

## Requirements

- [GNU Algol 68](https://gcc.gnu.org/onlinedocs/ga68.pdf) — the Algol 68 front-end for GCC (`ga68`)
- `u682a68` — the glyph→stropping converter used for the build (see below)
- a C compiler (`gcc`) and SQLite3 development headers (`-lsqlite3`)
- POSIX threads (`-pthread`, standard on macOS/Linux)

## Build

```sh
make
```

`make` runs `u682a68` to convert the glyph-sources `gorgona.u68` and `transput.u68` into UPPER-stropping `.a68` files, splices the Transput module straight behind the opening `BEGIN` of the main program (`gorgona-joined.a68`), compiles the C wrappers, and links everything into the `gorgona` binary. The splice is needed because the stock `ga68` installed here does not support Algol 68 module-units yet.

## Run

Create a `gorgona.conf` in the working directory:

```ini
# Gorgona configuration - format: KEY = VALUE, comments start with '#'
db_path = /path/to/Chinook.sqlite   # required
port = 8080                         # optional, default 8080
workers = 4                         # optional, default 4
```

Then start the server:

```sh
./gorgona [port] [workers]
```

| Argument | Default  | Meaning                                     |
| -------- | -------- | ------------------------------------------- |
| `port`   | from `gorgona.conf` (default `8080`) | TCP port, CLI overrides config |
| `workers`| from `gorgona.conf` (default `4`)    | number of worker threads, CLI overrides config |

`db_path` may only be set in the config file. Paths in `gorgona.conf` and
the static files under `web/` are resolved relative to the working
directory, so run `./gorgona` from the project directory. A missing config
file, a missing `db_path`, or an unopenable database each abort startup
with a German error message and exit code 1.

Open the browser frontend at <http://localhost:8080>, or query the API directly:

```sh
curl http://localhost:8080/api/tables
curl http://localhost:8080/api/album      # Chinook: table "Album"
```

### Endpoints

| Route                | Meaning                                           |
| -------------------- | ------------------------------------------------- |
| `/`                  | interactive frontend (HTML, from `web/`)          |
| `/app.js`            | frontend JavaScript (from `web/`)                 |
| `/style.css`         | frontend stylesheet (from `web/`)                 |
| `/api/tables`        | JSON list of all table routes                     |
| `/api/health`        | `{ "status" : "ok", "database" : "<basename of the active DB>" }` |
| `/api/<table>`       | one route per table of the database (see below)   |
| any other path       | `404 Not Found` with JSON body                    |

Table routes are built from the database schema at startup. The table name is lower-cased and stripped to `[a-z0-9_]`, so Chinook's `InvoiceLine` answers on `/api/invoiceline` and a table named `Order Details` answers on `/api/orderdetails`. Identifiers are quoted properly in the generated `SELECT`, so table names with spaces (or embedded quotes) work.

Each table route returns up to 50 rows:

```json
{ "table" : "Album", "rows" : [ { "AlbumId" : 1, "Title" : "For Those About To Rock We Salute You", "ArtistId" : 1 }, … ] }
```

## Project layout

| File                | Purpose |
| ------------------- | ------- |
| `gorgona.u68`         | The server in Algol 68 (main program, dynamic routing, HTTP handling) — written in Unicode *glyphs* |
| `transput.u68`      | The **Unicode Transput module**: byte-exact `read_file`/`write_all` plus the pure AlGol 68 config parser for `gorgona.conf` — also written in glyphs |
| `gorgona.a68`         | Generated UPPER-stropping main source (output of `u682a68`, do not edit) |
| `gorgona-joined.a68`  | `transput.a68` spliced behind the opening `BEGIN` of `gorgona.a68` (do not edit) |
| `transput_wrapper.c`| Minimal NEST-C glue: raw file read and raw descriptor write, one byte per cell — the only byte-exact transput primitives |
| `sqlite_wrapper.c`  | NEST-C glue: open DB, run `SELECT` and format the result as JSON directly in C |
| `sock_wrapper.c`    | NEST-C glue: TCP `listen`/`accept` (plus `SO_NOSIGPIPE` on accepted sockets) |
| `thread_wrapper.c`  | NEST-C glue: spawn the worker threads and register them with the Boehm GC used by the Algol 68 runtime |
| `gorgona.conf`        | Configuration file (see Run): database to browse, port, workers |
| `web/`              | Frontend sub-project: `index.html`, `app.js`, `style.css` — served as static files |
| `Makefile`          | Build pipeline: `u682a68` → splice module → `ga68` link against the wrapper objects |

### Writing Algol 68 in glyphs

The source `gorgona.u68` uses Unicode glyphs for Algol 68 keywords instead of the traditional reserved-word shouting:

```algol68
𝐟𝐨𝐫 𝑖 𝐟𝐫𝐨𝐦 𝐥𝐰𝐛 𝑟𝑜𝑢𝑡𝑒𝑠 𝐭𝐨 𝐮𝐩𝐛 𝑟𝑜𝑢𝑡𝑒𝑠 𝐝𝐨 … 𝐨𝐝
```

The `u682a68` converter rewrites this to `FOR i FROM LWB routes TO UPB routes DO … OD`, so the stock `ga68` compiler can compile it. Converted files carry the `.a68` extension and are regenerated on every `make`.

## Notable details

- **Byte-exact transput**: the Algol 68 POSIX prelude is not byte-exact — `fgetc` *decodes* UTF-8 while reading (a multi-byte sequence becomes one code point) and `fputs` writes only the *character count* from a longer UTF-8 buffer, truncating the tail and double-encoding accents. The Unicode Transput module (`transput.u68`) therefore provides `read_file` and `write_all` on top of two minimal C primitives in `transput_wrapper.c` that do raw, one-byte-per-cell I/O. Socket responses and file reads are byte-for-byte, so `Content-Length` always matches.
- **Config in Algol 68**: `gorgona.conf` is parsed by the Transput module (`config_load`/`config_string`/`config_int` in pure Algol 68, using the web68-style `ABS s[i] - ABS "0"` idiom for numbers). `db_path` is required; `port`/`workers` default to config values and can be overridden on the command line. Startup fails with an explicit German message and exit code 1 if the config, `db_path`, or the database is missing or unopenable.
- **Dynamic routes**: on startup the server queries `sqlite_master`, derives a route per table, and quotes the table name as an SQL identifier in the generated `SELECT` — so any SQLite database works, including tables with spaces or accented names.
- **Frontend as a sub-project**: the browser lives under `web/` as plain, normal HTML/CSS/JS files (framework-free) and is served from disk at `/`, `/app.js` and `/style.css`. Because they are real files, they are freed from the ga68 string restrictions that would apply to embedded code — an embedded page must avoid apostrophes and multi-line literals, see next bullet.
- **ga68 string and boolean quirks**: ga68 does not support multi-line string literals; an apostrophe inside a string is an escape introducer; and a string containing a quote needs the quote *doubled* (`""""` is a one-character `"`). Strings in `gorgona.u68` stay on one line and embedded quotes are avoided using `REPR 34`. One more trap: **`AND`/`OR` are not short-circuiting**, so a bound check and a subscript may not share a conjunction — the code guards bounds with explicit `IF`s.
- **Module splicing**: the stock `ga68` (GCC 16) installed here does not support Algol 68 module-units yet, so the Transput module is spliced textually behind the opening `BEGIN` by the Makefile instead. This keeps the module self-contained and reviewable while the main program stays a single `ga68` source.

## Related

- **sql68** — a sibling local project: an interactive SQLite shell in Algol 68 using the same Chinook database and the same NEST-C techniques. The string-returning `nest C` convention used here (`uint32_t **out, size_t *len`) was verified against it and is known to work correctly with ga68.