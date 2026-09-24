/* Gorgona: SQLite-Anbindung fuer dieselbe Datenbank wie das Projekt sql68
 * (Chinook.sqlite). Im Gegensatz zu sql68 wird das Abfrageergebnis hier
 * direkt in C zu einem JSON-Array formatiert statt ueber einen Algol-68-
 * Callback: Alle Worker-Threads teilen sich eine Verbindung, der Zugriff
 * wird mit einer pthread-Mutex serialisiert und die JSON-Puffer sind
 * thread-lokal (ga68 kopiert den STRING erst NACH der Rueckkehr aus der
 * C-Funktion in den Algol-Heap, daher darf ein Puffer nicht von einem
 * anderen Thread ueberschrieben werden).
 */

#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static pthread_mutex_t srv_sqlite_lock = PTHREAD_MUTEX_INITIALIZER;

/* ga68 reicht STRING als (uint32_t*, len, stride) in UCS-4 durch. */
static char *ucs4_to_c(const uint32_t *s, size_t len, size_t stride)
{
  char *c = malloc(len + 1);
  if (!c) return NULL;
  for (size_t i = 0; i < len; i++) {
    const uint32_t *p = (const uint32_t *)((const char *)s + i * stride);
    c[i] = (char)(*p & 0xFF);
  }
  c[len] = '\0';
  return c;
}

/* Oeffnet die Datenbank. ga68-Layout: STRING -> (u32*, len, stride),
 * REF sqlite -> sqlite3**. Rueckgabe: der sqlite3-Fehlercode (0 = OK). */
int algol68_sqlite_open(const uint32_t *s, size_t len, size_t stride,
                        sqlite3 **ppDb)
{
  char *cfn = ucs4_to_c(s, len, stride);
  if (!cfn) return SQLITE_NOMEM;
  int rc = sqlite3_open(cfn, ppDb);
  free(cfn);
  return rc;
}

/* --- thread-lokale, dynamisch wachsende JSON-Puffer --- */
static __thread char *json_cur;
static __thread size_t json_len;
static __thread size_t json_cap;
static __thread uint32_t *json_u32;
static __thread size_t u32_cap;

static void json_append(const char *s, size_t n)
{
  if (json_len + n + 1 > json_cap) {
    size_t nc = json_cap ? json_cap * 2 : 4096;
    while (json_len + n + 1 > nc) nc *= 2;
    char *nb = realloc(json_cur, nc);
    if (!nb) return;
    json_cur = nb;
    json_cap = nc;
  }
  memcpy(json_cur + json_len, s, n);
  json_len += n;
  json_cur[json_len] = '\0';
}

static void json_put(const char *s) { json_append(s, strlen(s)); }

/* Ein Wert in JSON-Anfuehrungszeichen setzen und Sonderzeichen escapen;
 * kontrolle Zeichen als \uXXXX, alles andere bleibt als UTF-8 erhalten. */
static void json_quote(const char *s)
{
  json_put("\"");
  if (s) {
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
      unsigned char ch = *p;
      switch (ch) {
      case '"':  json_put("\\\""); break;
      case '\\': json_put("\\\\"); break;
      case '\n': json_put("\\n"); break;
      case '\r': json_put("\\r"); break;
      case '\t': json_put("\\t"); break;
      default:
        if (ch < 0x20) {
          char esc[8];
          int l = snprintf(esc, sizeof esc, "\\u%04x", ch);
          json_append(esc, (size_t)l);
        } else {
          json_append((const char *)&ch, 1);
        }
      }
    }
  }
  json_put("\"");
}

/* Fuehrt die Abfrage aus und liefert das Ergebnis als JSON-Array von
 * Objekten zurueck, etwa  [ {"AlbumId":1,"Title":"...","ArtistId":1}, ... ] .
 * Spaltennamen und Textzellen werden gequotet; INTEGER/FLOAT bleiben
 * nackte JSON-Zahlen, NULL wird zu null. Rueckgabe: 0 = ok (auch leeres
 * Ergebnis), sonst Fehlercode. */
int algol68_sqlite_exec_json(sqlite3 *db,
                             const uint32_t *q, size_t qlen, size_t qstride,
                             uint32_t **out, size_t *out_len)
{
  *out = NULL;
  *out_len = 0;

  char *cq = ucs4_to_c(q, qlen, qstride);
  if (!cq) return SQLITE_NOMEM;

  pthread_mutex_lock(&srv_sqlite_lock);

  json_len = 0;
  int rc = SQLITE_ERROR;
  sqlite3_stmt *stmt = NULL;
  if (sqlite3_prepare_v2(db, cq, -1, &stmt, NULL) == SQLITE_OK) {
    json_put("[");
    int first = 1;
    int sr;
    rc = SQLITE_OK;
    while ((sr = sqlite3_step(stmt)) == SQLITE_ROW) {
      if (!first) json_put(",");
      first = 0;
      json_put("{");
      int ncols = sqlite3_column_count(stmt);
      for (int c = 0; c < ncols; c++) {
        if (c > 0) json_put(",");
        json_quote((const char *)sqlite3_column_name(stmt, c));
        json_put(":");
        int t = sqlite3_column_type(stmt, c);
        if (t == SQLITE_NULL) {
          json_put("null");
        } else if (t == SQLITE_INTEGER || t == SQLITE_FLOAT) {
          json_put((const char *)sqlite3_column_text(stmt, c));
        } else {
          json_quote((const char *)sqlite3_column_text(stmt, c));
        }
      }
      json_put("}");
    }
    json_put("]");
    if (sr != SQLITE_DONE) rc = sr;
    sqlite3_finalize(stmt);
  }

  pthread_mutex_unlock(&srv_sqlite_lock);

  free(cq);

  /* json_cur ist thread-lokal, nach dem Unlock also unveraendert:
   * jetzt nach UCS-4 fuer ga68 konvertieren. */
  size_t need = json_len + 1;
  if (u32_cap < need) {
    uint32_t *nu = realloc(json_u32, need * sizeof(uint32_t));
    if (nu) {
      json_u32 = nu;
      u32_cap = need;
    }
  }
  if (json_cur && json_u32) {
    for (size_t i = 0; i <= json_len; i++)
      json_u32[i] = (uint32_t)(unsigned char)json_cur[i];
    *out = json_u32;
    *out_len = json_len;
  }
  return rc;
}

int algol68_sqlite_close(sqlite3 *db)
{
  return sqlite3_close(db);
}