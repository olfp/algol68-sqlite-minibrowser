# srv68 — a minimal HTTP server written in Algol 68

`srv68` is a small, self-contained HTTP server implemented in **Algol 68**. It exposes the tables of the [Chinook sample database](https://www.sqlitetutorial.net/sqlite-sample-database/) through a tiny JSON REST API and ships with an interactive, framework-free browser frontend that lets you explore the data in your web browser.

Everything that talks to the outside world (TCP sockets, SQLite, threads) is reached from few lines of Algol 68 via `nest C` calls into small C wrappers — the actual server logic lives in Algol 68.

## Features

- **REST API**: all 11 Chinook tables are exposed as JSON routes (`/api/albums`, `/api/tracks`, `/api/customers`, …)
- **Interactive DB browser**: served at `/`, a single-page HTML/JS client with
  - table selection,
  - live filtering across all rows (client-side),
  - sortable columns (click on the sticky table header),
  - pagination (25 rows per page),
  - proper error/status display.
- **Concurrency**: `pthread` worker threads handle requests in parallel; one shared SQLite connection is serialized with a mutex.
- **UTF-8 correct**: responses are byte-exact, including accented data (`Acústico`, `Antônio Carlos Jobim`, …).
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

`make` runs `u682a68` to convert the glyph-source `srv68.u68` into the UPPER-stropping `srv68.a68`, compiles the C wrappers, and links everything into the `srv68` binary.

## Run

```sh
./srv68 [port] [workers]
```

| Argument | Default | Meaning                    |
| -------- | ------- | -------------------------- |
| `port`   | `8080`  | TCP port to listen on      |
| `workers`| `4`     | number of worker threads   |

Open the browser frontend at <http://localhost:8080>, or query the API directly:

```sh
curl http://localhost:8080/api/albums
curl http://localhost:8080/api/tracks
```

### Endpoints

| Route                | Table           |
| -------------------- | --------------- |
| `/`                  | interactive frontend (HTML) |
| `/api/tables`        | JSON list of all table routes |
| `/api/health`        | `{ "status" : "ok" }` |
| `/api/albums`        | `Album`         |
| `/api/artists`       | `Artist`        |
| `/api/customers`     | `Customer`      |
| `/api/employees`     | `Employee`      |
| `/api/genres`        | `Genre`         |
| `/api/invoices`      | `Invoice`       |
| `/api/invoicelines`  | `InvoiceLine`   |
| `/api/mediatypes`    | `MediaType`     |
| `/api/playlists`     | `Playlist`      |
| `/api/playlisttracks`| `PlaylistTrack` |
| `/api/tracks`        | `Track`         |
| any other path       | `404 Not Found` with JSON body |

Each table route returns up to 50 rows:

```json
{ "table" : "Album", "rows" : [ { "AlbumId" : 1, "Title" : "For Those About To Rock We Salute You", "ArtistId" : 1 }, … ] }
```

## Project layout

| File                | Purpose |
| ------------------- | ------- |
| `srv68.u68`         | The whole server in Algol 68 (main program, routing, HTTP handling, inline HTML/JS frontend) — written in Unicode *glyphs* |
| `srv68.a68`         | Generated UPPER-stropping source (output of `u682a68`, do not edit) |
| `sqlite_wrapper.c`  | NEST-C glue: open DB, run `SELECT` and format the result as JSON directly in C |
| `sock_wrapper.c`    | NEST-C glue: TCP `listen`/`accept` (plus `SO_NOSIGPIPE` on accepted sockets) |
| `thread_wrapper.c`  | NEST-C glue: spawn the worker threads and register them with the Boehm GC used by the Algol 68 runtime |
| `Makefile`          | Build pipeline: `u682a68` → `ga68` link against the wrapper objects |

### Writing Algol 68 in glyphs

The source `srv68.u68` uses Unicode glyphs for Algol 68 keywords instead of the traditional reserved-word shouting:

```algol68
𝐟𝐨𝐫 𝑖 𝐟𝐫𝐨𝐦 𝐥𝐰𝐛 𝑟𝑜𝑢𝑡𝑒𝑠 𝐭𝐨 𝐮𝐩𝐛 𝑟𝑜𝑢𝑡𝑒𝑠 𝐝𝐨 … 𝐨𝐝
```

The `u682a68` converter rewrites this to `FOR i FROM LWB routes TO UPB routes DO … OD`, so the stock `ga68` compiler can compile it. Converted files carry the `.a68` extension and are regenerated on every `make`.

## Notable details

- **Byte-exact socket writes**: the Algol 68 POSIX `fputs` of the installed ga68 runtime writes only the *character count* from a longer UTF-8 buffer — for non-ASCII payloads this truncates the tail and double-encodes accented characters. `srv68` therefore writes responses through `algol68_write_all` in `sqlite_wrapper.c`, which mirrors the byte-for-byte encoding and keeps `Content-Length` consistent.
- **Database path**: the SQLite file is hard-coded in `srv68.u68` (`db_path`); adjust it there if your Chinook copy lives elsewhere.
- **No multi-line strings**: ga68 does not support multi-line string literals, and an apostrophe inside a string is an escape introducer. The embedded HTML page is therefore built from one-line string literals, uses doubled quotes (`""`) for embedded quotes, and contains no apostrophes at all.

## Related

- **sql68** — a sibling local project: an interactive SQLite shell in Algol 68 using the same Chinook database and the same NEST-C techniques. The string-returning `nest C` convention used here (`uint32_t **out, size_t *len`) was verified against it and is known to work correctly with ga68.