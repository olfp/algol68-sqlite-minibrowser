# Gorgona — a minimal HTTP server written in Algol 68

`Gorgona` is a small, self-contained HTTP server implemented in **Algol 68**. It examines the schema of an arbitrary SQLite database, derives a REST route for every table, and exposes it as JSON. A separate sub-project under `web/` provides an interactive, framework-free browser frontend for exploring the data. **Several databases can be registered at once** (`db.<name> = <path>` lines in `gorgona.conf`); the frontend lets you switch between them and add new ones without restarting the server.

Everything that talks to the outside world (TCP sockets, SQLite, threads) is reached from a few lines of Algol 68 via `nest C` calls into minimal C wrappers. Config parsing, routing, and the HTTP logic live in Algol 68.

## Features

- **Dynamic REST API**: routes are derived at startup from `sqlite_master` — any SQLite database works, not just one hard-coded schema (`Order Details` becomes `/api/<db>/orderdetails`, …)
- **Multiple databases**: every `db.<name> = <path>` line in `gorgona.conf` becomes a database; the API is scoped per database (`/api/<db>/…`). A switch pull-down in the frontend changes the active database, and an **“Database hinzufügen”** dialog registers a new one — the entry is persisted to `gorgona.conf` right away.
- **Interactive DB browser**: served at `/`, a single-page HTML/JS client (a separate sub-project in `web/`) with
  - table selection,
  - live filtering across all rows (client-side),
  - sortable columns (click on the sticky table header),
  - pagination (25 rows per page),
  - proper error/status display.
- **Concurrency**: `pthread` worker threads handle requests in parallel; one shared SQLite connection is serialized with a mutex.
- **Byte-exact I/O**: reading static files and writing socket responses are byte-for-byte exact (a dedicated Unicode Transput module in `transio.u68` sits on two minimal C primitives), including accented data (`Acústico`, `aé`, …).
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

`make` builds everything **in a separate `build/run/` folder** (the repo stays clean — the `.u68`/`.c` sources are never touched). `u682a68` converts the glyph-sources `gorgona.u68`, `transio.u68`, `strings.u68`, and `url.u68` into UPPER-stropping `.a68` files, the three modules are compiled with `ga68 -c` each into their own object (`transio.o`, `strings.o`, `url.o`), the C wrappers are compiled, and the main program (`gorgona.a68`, a particular program with an `ACCESS TRANSIO, STRINGS, URL` clause) is linked together with the module objects into `build/run/gorgona`. Ga68 drops its `.o` next to the working directory, so the Makefile runs compile and link inside `build/run/`. The whole build is pure native ga68 module-units — no textual splicing. Cleans with `make clean` (removes `build/`).

## Run

Create a `gorgona.conf` in the working directory:

```ini
# Gorgona configuration - format: KEY = VALUE, comments start with '#'

# Host-/OS-specific sections: the following entries only apply where
# the section matches. Entries outside any section always apply (global).
#   [host <hostname>]           this machine only (first name, lowercase)
#   [os <macos|linux|windows>]  this OS only
[host hepti]
db.Chinook = /path/to/Chinook.sqlite
db.Northwind = /path/to/Northwind.sqlite   # one database per line
port = 8080                                # optional, default 8080
workers = 4                                # optional, default 4
```

Each `db.<name> = <path>` line registers one database under the display
name <name>. Sections scope their entries to a host (`[host hepti]`) or an
operating system (`[os macos]`) — e.g. a database that only exists on
this Mac. Outside a section entries are global. A new section header
(`[host ...]`) or `[os ...]`) ends the previous one; unknown tags are
ignored (their entries are skipped). Matching is case-sensitive, so write
the hostname as shown in the `HOST:` startup log. For compatibility the
old single-database key `db_path` is still accepted: if no `db.*` line
exists, `db_path` serves as one database named after the file's
basename. `port`/`workers` are optional with defaults, exactly as
before.

Then start the server:

```sh
make run          # == ./build/run/gorgona [port] [workers]
```

| Argument | Default  | Meaning                                     |
| -------- | -------- | ------------------------------------------- |
| `port`   | from `gorgona.conf` (default `8080`) | TCP port, CLI overrides config |
| `workers`| from `gorgona.conf` (default `4`)    | number of worker threads, CLI overrides config |

`db_path` and `db.<name>` may only be set in the config file. Paths in
`gorgona.conf` and the static files under `web/` are resolved relative to
the working directory, so run `make run` (i.e. `./build/run/gorgona`)
from the project directory. A
missing config file, no database at all, or an unopenable database each
abort startup with a German error message and exit code 1.

Open the browser frontend at <http://localhost:8080>, or query the API directly:

```sh
curl http://localhost:8080/api/dbs
curl http://localhost:8080/api/chinook/tables                 # Chinook tables
curl http://localhost:8080/api/chinook/album                  # Chinook: table "Album"
```

### Endpoints

| Route                | Meaning                                           |
| -------------------- | ------------------------------------------------- |
| `/`                  | interactive frontend (HTML, from `web/`)          |
| `/app.js`            | frontend JavaScript (from `web/`)                 |
| `/style.css`         | frontend stylesheet (from `web/`)                 |
| `/api/dbs`           | JSON list of all registered databases (`name`, `slug`, `path`) |
| `/api/dbadd?name=&path=` | register a new database and persist it to `gorgona.conf` |
| `/api/<db>/tables`   | JSON list of all table routes of that database   |
| `/api/<db>/health`   | `{ "status" : "ok", "database" : "<display name>" }` |
| `/api/<db>/schema`   | full schema as JSON for the ER diagram (see below) |
| `/api/<db>/<table>`  | one route per table of the database (see below)  |
| any other path       | `404 Not Found` with JSON body                    |

`<db>` is the database *slug* (the display name lower-cased and stripped to
`[a-z0-9_]`, same rule the table routes use). A database named
`My Database` answers under `/api/mydatabase/…`.

`/api/dbs` returns `{ "databases" : [ { "name" : … , "slug" : … , "path" : … } ] }`; the
frontend fills its switch pull-down from it. `/api/dbadd` takes `name` and
`path` as query parameters, opens the database, adds it to the registry and
**rewrites `gorgona.conf`** so the entry survives a restart. It returns the
new `{ "databases" : … }` list; errors reply with a JSON `"error"` and a
matching status (`400` bad name/path, `409` name already registered,
`507` registry full — at most 16 databases —, `500` open failed).

`/api/<db>/schema` returns every table of *that* database with its columns
(in declared order, `pk` marks the primary-key index, `0` = not part of the
PK) and its foreign keys (`from`/`to` column names and the referenced table
in `ref`). It is computed once per database from `sqlite_master`,
`PRAGMA table_info` and `PRAGMA foreign_key_list`:

```json
{ "tables" : [ { "name" : "Album", "cols" : [ { "name" : "AlbumId", "pk" : 1 }, … ],
                 "fks" : [ { "from" : "ArtistId", "to" : "ArtistId", "ref" : "Artist" } ] }, … ] }
```

The frontend button "ER-Diagramm" in the title line opens this schema as a diagram: every table is a draggable entity; foreign keys are drawn as arrows from a column of one entity to the referenced column of the other (entity column headers, PK columns and FK columns are highlighted). Entities are laid out automatically in layers with the fewest possible edge crossings; their positions are saved in `localStorage`, keyed per database, and a "Reset" button restores the automatic layout. The ER window (and the read-only record dialog) is nearly full-size by default and can be moved by dragging its header and resized via the corner grip.

Table routes are built from the database schema at startup and are scoped to their database. The table name is lower-cased and stripped to `[a-z0-9_]`, so Chinook's `InvoiceLine` answers on `/api/chinook/invoiceline` and a table named `Order Details` answers on `/api/orderdetails`. Identifiers are quoted properly in the generated `SELECT`, so table names with spaces (or embedded quotes) work.

Each table route supports windowed access — `offset` and `count` (query parameters, defaults `0` and `50`) select a range, and the response always carries the table's total row count:

```sh
curl 'http://localhost:8080/api/chinook/track?offset=50&count=25'
```

```json
{ "table" : "Track", "total" : 3503, "rows" : [ { "TrackId" : 51, … }, … ] }
```

The frontend uses this to implement infinite scrolling: it keeps a sliding window of at most `3 × 50 = 150` loaded rows, prefetches the next chunk as you approach the window edge, discards rows that scrolled out of view, and re-loads them as needed when you scroll back up.

A single row can be located directly by any column with `fcol` and `fval` (the value is URL-encoded; the server decodes `%XX` and `+`). This is used for **foreign-key navigation**: the response then also carries `idx`, the row's 0-based position in the default row order, so the frontend can place the referenced record exactly as the 3rd visible row:

```sh
curl 'http://localhost:8080/api/chinook/track?fcol=AlbumId&fval=5'
```

```json
{ "table" : "Track", "total" : 3503, "idx" : 22, "rows" : [ { "gorgidx" : 22, "TrackId" : 23, … } ] }
```

In the table view, every foreign-key value (detected from the schema) is preceded by a small **→** badge (the ID itself sits in a right-aligned slot with reserved width, so longer IDs — up to `MaxInt` — don't shift the badge); clicking the badge switches to the referenced table and shows the referenced record as the **3rd data row** (two rows of context above it), highlighted in the table. The position lookup keeps the record in place even for huge tables, because the window fetch starts at `max(0, idx − 2)`.

Navigation history is remembered: the browser view keeps a stack of `{ table, row }` steps and offers **`<` / `>` buttons at the far right of the title line** to step back and forward through it (forward steps are discarded once you navigate somewhere new). Following a foreign key, stepping **back** marks the record you followed (its badge row) again; stepping forward restores the referenced record. Table columns carry an estimated minimal width (from the visible values plus the badge/id basket for foreign-key columns) so FK columns always fit their content.

The badge also shows the referenced record's **display value** — the `NAME` column of the target table, or the first non-ID column if there is no `NAME`. Each windowed table response carries these per row as extra keys `gorgref_<fromColumn>` (a correlated subquery on the target table; `null` when no row matches), so the badge needs no separate request:

```sh
curl 'http://localhost:8080/api/chinook/album?offset=0&count=1'
```

```json
{ "table" : "Album", "total" : 347, "rows" : [ { "AlbumId" : 1, "Title" : "For Those About To Rock We Salute You", "ArtistId" : 1, "gorgref_ArtistId" : "AC/DC" } ] }
```

The client strips these synthetic keys before building the column headers and only uses them as badge text.

## Project layout

| File                | Purpose |
| ------------------- | ------- |
| `gorgona.u68`         | The server in Algol 68 (main program, multi-database registry, dynamic routing, HTTP handling) — written in Unicode *glyphs* |
| `strings.u68`        | The **string-helper module**: `s2i`, `findc`, `find_sub`, `find_count`, and `jq` (JSON string escaping) |
| `url.u68`            | The **URL/request module**: `extract_path`, `split_db`, `wsug`, `path_of`, `pct_decode`, `query_int`, `query_string` (mode `PATHDEF`) |
| `transio.u68`      | The **Unicode Transput module** (`MODULE TRANSIO`): byte-exact `read_file`/`write_all` plus the pure Algol 68 config parser for `gorgona.conf` (with `[host …]`/`[os …]` sections) — also written in glyphs |
| `build/run/gorgona.a68` | Generated in `build/run/` (output of `u682a68`, do not edit) |
| `build/run/transio.a68`, `strings.a68`, `url.a68` | Generated in `build/run/` UPPER-stropping **definition-module** sources; each is compiled to the object whose name matches its module indicant (`transio.o`, `strings.o`, `url.o`) so the `ACCESS` clauses resolve the exports |
| `build/run/*.o`, `build/run/gorgona` | All objects and the linked **executable** live in `build/run/` — the repo itself stays free of build products |
| `transput_wrapper.c`| Minimal NEST-C glue: raw file read/write and raw descriptor write, one byte per cell — incl. the atomic `algol68_write_file` used to rewrite `gorgona.conf`, plus `algol68_hostname`/`algol68_osname` for the host/OS-aware config sections |
| `sqlite_wrapper.c`  | NEST-C glue: open DB, run `SELECT` and format the result as JSON directly in C; schema cache per database (keyed by connection+table) for FK display values (`gorgref_*`) and the ER diagram |
| `sock_wrapper.c`    | NEST-C glue: TCP `listen`/`accept` (plus `SO_NOSIGPIPE` on accepted sockets) |
| `thread_wrapper.c`  | NEST-C glue: spawn the worker threads and register them with the Boehm GC used by the Algol 68 runtime |
| `gorgona.conf`        | Configuration file (see Run): database registry (`db.<name> = <path>`), port, workers |
| `web/`              | Frontend sub-project: `index.html`, `app.js`, `style.css` — served as static files |
| `Makefile`          | Build pipeline into `build/run/`: `u682a68` → per-module `ga68 -c` → link main + module objects against the wrapper objects |

### Writing Algol 68 in glyphs

The source `gorgona.u68` uses Unicode glyphs for Algol 68 keywords instead of the traditional reserved-word shouting:

```algol68
𝐟𝐨𝐫 𝑖 𝐟𝐫𝐨𝐦 𝐥𝐰𝐛 𝑟𝑜𝑢𝑡𝑒𝑠 𝐭𝐨 𝐮𝐩𝐛 𝑟𝑜𝑢𝑡𝑒𝑠 𝐝𝐨 … 𝐨𝐝
```

The `u682a68` converter rewrites this to `FOR i FROM LWB routes TO UPB routes DO … OD`, so the stock `ga68` compiler can compile it. Converted files carry the `.a68` extension and are regenerated on every `make`.

## Notable details

- **Native modules**: the source is split into three ga68 **definition modules** (`MODULE TRANSIO`, `MODULE STRINGS`, `MODULE URL = ACCESS STRINGS`) plus a particular program (`gorgona.a68`) that begins with `ACCESS TRANSIO, STRINGS, URL`. Each module exports its helpers with `PUB`, and the main program calls them directly across the module boundary. The modules are compiled separately (`ga68 -c`), and the exported declarations are resolved from the module objects at compile time.
- **Byte-exact transput**: the Algol 68 POSIX prelude is not byte-exact — `fgetc` *decodes* UTF-8 while reading (a multi-byte sequence becomes one code point) and `fputs` writes only the *character count* from a longer UTF-8 buffer, truncating the tail and double-encoding accents. The Unicode Transput module (`transio.u68`) therefore provides `read_file` and `write_all` on top of two minimal C primitives in `transput_wrapper.c` that do raw, one-byte-per-cell I/O. Socket responses and file reads are byte-for-byte, so `Content-Length` always matches.
- **Config in Algol 68**: `gorgona.conf` is parsed by the Transput module (`config_load`/`config_string`/`config_int` in pure Algol 68, using the web68-style `ABS s[i] - ABS "0"` idiom for numbers). Since ga68 lacks `hostname`/`os`, two tiny NEST-C primitives (`algol68_hostname`, `algol68_osname`) provide the machine name and OS; `[host …]`/`[os …]` sections scope entries to a specific host/OS, entries outside sections are global. At least one database must be present (as a `db.<name> = <path>` line, or via the legacy `db_path`); `port`/`workers` default to config values and can be overridden on the command line. Startup fails with an explicit German message and exit code 1 if the config, all databases, or any database is missing or unopenable.
- **Persistent registry**: `dbadd` rewrites `gorgona.conf` through `algol68_write_file` (write to a `*.tmp` file, then atomic `rename`), keeping the non-database keys and replacing all `db.*`/`db_path` lines with one `db.<name> = <path>` line per registered database.
- **Dynamic routes**: on startup the server queries `sqlite_master`, derives a route per table, and quotes the table name as an SQL identifier in the generated `SELECT` — so any SQLite database works, including tables with spaces or accented names.
- **Frontend as a sub-project**: the browser lives under `web/` as plain, normal HTML/CSS/JS files (framework-free) and is served from disk at `/`, `/app.js` and `/style.css`. Because they are real files, they are freed from the ga68 string restrictions that would apply to embedded code — an embedded page must avoid apostrophes and multi-line literals, see next bullet.
- **ga68 string and boolean quirks**: ga68 does not support multi-line string literals; an apostrophe inside a string is an escape introducer; and a string containing a quote needs the quote *doubled* (`""""` is a one-character `"`). Strings in `gorgona.u68` stay on one line and embedded quotes are avoided using `REPR 34`. One more trap: **`AND`/`OR` are not short-circuiting**, so a bound check and a subscript may not share a conjunction — the code guards bounds with explicit `IF`s.
- **Module naming**: the module objects must be named after the lowercase module indicant — `ga68 -c transio.a68` produces `transio.o` for `MODULE TRANSIO` — because `ACCESS` looks the exports up by that name. `url.a68` shows the nested case: its own module accesses the `STRINGS` module (`MODULE URL = ACCESS STRINGS`) the same way the main program accesses all three.

## Related

- **sql68** — a sibling local project: an interactive SQLite shell in Algol 68 using the same Chinook database and the same NEST-C techniques. The string-returning `nest C` convention used here (`uint32_t **out, size_t *len`) was verified against it and is known to work correctly with ga68.