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

/* Eine Datensatz-Zeile als JSON-Objekt ausgeben: { "Spalte" : Wert, ... }
 * Spaltennamen und Textwerte werden gequotet, INTEGER/FLOAT bleiben nackte
 * Zahlen, NULL wird zu null. */
static void emit_stmt(sqlite3_stmt *stmt)
{
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

/* Den thread-lokalen JSON-Puffer nach UCS-4 fuer ga68 konvertieren. */
static void json_finish(uint32_t **out, size_t *out_len)
{
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
}

/* --- Schema-Cache: je Tabelle Spalten und Fremdschluessel, plus die
   "Anzeige-Spalte" (NAME, sonst erste Spalte ohne "ID"). Daraus baut
   algol68_sqlite_table_rows die gorgref_*-Referenzwerte fuer die
   Badges im Frontend. --- */
typedef struct { char *name; int pk; } SchCol;
typedef struct { char *from, *to, *ref; } SchFk;
typedef struct {
  sqlite3 *db;              /* zugehoerige Connection (Cache je Datenbank) */
  char *name, *dcol;
  int ncols; SchCol *cols;
  int nfks;  SchFk  *fks;
} SchTab;
static SchTab *sch_cache = NULL;
static int sch_n = 0;

static int dlow(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int ci_eq(const char *a, const char *b)
{
  for (; *a && *b; a++, b++)
    if (dlow((unsigned char)*a) != dlow((unsigned char)*b)) return 0;
  return *a == *b;
}
static int ci_has(const char *hay, const char *needle)
{
  size_t nl = strlen(needle);
  if (!nl) return 1;
  for (const char *p = hay; *p; p++) {
    if (dlow((unsigned char)*p) == dlow((unsigned char)needle[0])) {
      const char *q = p, *n = needle;
      while (*n && *q && dlow((unsigned char)*q) == dlow((unsigned char)*n)) { q++; n++; }
      if (!*n) return 1;
    }
  }
  return 0;
}

static char *sch_dcol(SchCol *cols, int ncols)
{
  if (!ncols) return NULL;
  for (int i = 0; i < ncols; i++) if (ci_eq(cols[i].name, "name")) return cols[i].name;
  for (int i = 0; i < ncols; i++) if (ci_has(cols[i].name, "name")) return cols[i].name;
  for (int i = 0; i < ncols; i++) if (!ci_has(cols[i].name, "id")) return cols[i].name;
  return cols[0].name;
}

/* Cache-Eintraege sind je (Datenbank, Tabellenname) eindeutig: Mehrere
 * Datenbanken koennen gleichnamige Tabellen haben, darum wird der
 * sqlite3-Pointer mitgeglichen. */
static int sch_find_idx(sqlite3 *db, const char *name)
{
  for (int i = 0; i < sch_n; i++)
    if (sch_cache[i].db == db && strcmp(sch_cache[i].name, name) == 0) return i;
  return -1;
}

/* Tabelle (Spalten + FKs) laden, wenn noetig; Rueckgabe: Index im Cache,
   -1 wenn die Tabelle nicht existiert. Nur innerhalb des Mutex aufrufen! */
static int sch_load(sqlite3 *db, const char *name)
{
  int i = sch_find_idx(db, name);
  if (i >= 0) return i;
  sqlite3_stmt *chk = NULL;
  int exists = 0;
  if (sqlite3_prepare_v2(db,
      "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1",
      -1, &chk, NULL) == SQLITE_OK) {
    sqlite3_bind_text(chk, 1, name, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(chk) == SQLITE_ROW) exists = 1;
    sqlite3_finalize(chk);
  }
  if (!exists) return -1;

  int ncols = 0, nfks = 0;
  SchCol *cols = NULL; SchFk *fks = NULL;
  sqlite3_stmt *st = NULL;
  char *q = sqlite3_mprintf("PRAGMA table_info(%w);", name);
  if (q && sqlite3_prepare_v2(db, q, -1, &st, NULL) == SQLITE_OK) {
    while (sqlite3_step(st) == SQLITE_ROW) {
      cols = realloc(cols, (size_t)(ncols + 1) * sizeof(SchCol));
      cols[ncols].name = strdup((const char *)sqlite3_column_text(st, 1));
      cols[ncols].pk = sqlite3_column_int(st, 5);
      ncols++;
    }
    sqlite3_finalize(st);
  }
  sqlite3_free(q);
  q = sqlite3_mprintf("PRAGMA foreign_key_list(%w);", name);
  if (q && sqlite3_prepare_v2(db, q, -1, &st, NULL) == SQLITE_OK) {
    while (sqlite3_step(st) == SQLITE_ROW) {
      fks = realloc(fks, (size_t)(nfks + 1) * sizeof(SchFk));
      fks[nfks].from = strdup((const char *)sqlite3_column_text(st, 3));
      fks[nfks].to   = strdup((const char *)sqlite3_column_text(st, 4));
      fks[nfks].ref  = strdup((const char *)sqlite3_column_text(st, 2));
      nfks++;
    }
    sqlite3_finalize(st);
  }
  sqlite3_free(q);

  sch_cache = realloc(sch_cache, (size_t)(sch_n + 1) * sizeof(SchTab));
  i = sch_n;
  sch_cache[i].db = db;
  sch_cache[i].name = strdup(name);
  sch_cache[i].cols = cols;  sch_cache[i].ncols = ncols;
  sch_cache[i].fks  = fks;   sch_cache[i].nfks  = nfks;
  sch_cache[i].dcol = sch_dcol(cols, ncols);
  sch_n++;
  return i;
}

static void sb_ident(sqlite3_str *sb, const char *name)
{
  char *qn = sqlite3_mprintf("%w", name);
  sqlite3_str_appendall(sb, qn ? qn : "\"\"");
  sqlite3_free(qn);
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
      emit_stmt(stmt);
    }
    json_put("]");
    if (sr != SQLITE_DONE) rc = sr;
    sqlite3_finalize(stmt);
  }

  pthread_mutex_unlock(&srv_sqlite_lock);

  free(cq);

  /* json_cur ist thread-lokal, nach dem Unlock also unveraendert:
   * jetzt nach UCS-4 fuer ga68 konvertieren. */
  json_finish(out, out_len);
  return rc;
}

/* Datensaetze einer Tabelle als JSON-Array mit integrierten
 * Fremdschluessel-Referenzwerten: Zu jeder FK-Spalte wird die Anzeige-
 * Spalte der Ziel-Tabelle (NAME, sonst erste Spalte ohne "ID") per
 * korreliertem Subquery mitgeliefert, als Zusatzschluessel
 * "gorgref_<from>". Das Frontend zeigt diese Werte hinter den
 * Navigations-Badges. Abfrage bleibt im rowid-Format (LIMIT/OFFSET),
 * damit die idx-Positionierung der fcol/fval-Navigation stimmt. */
int algol68_sqlite_table_rows(sqlite3 *db,
                              const uint32_t *tname, size_t tlen, size_t tstride,
                              int64_t off, int64_t cnt,
                              uint32_t **out, size_t *out_len)
{
  *out = NULL;
  *out_len = 0;

  char *ct = ucs4_to_c(tname, tlen, tstride);
  if (!ct) return SQLITE_NOMEM;

  pthread_mutex_lock(&srv_sqlite_lock);
  json_len = 0;
  int rc = SQLITE_ERROR;
  int ti = sch_load(db, ct);
  if (ti >= 0) {
    SchTab *t = &sch_cache[ti];
    sqlite3_str *sb = sqlite3_str_new(NULL);
    sqlite3_str_appendall(sb, "SELECT ");
    for (int c = 0; c < t->ncols; c++) {
      if (c > 0) sqlite3_str_appendall(sb, ", ");
      sb_ident(sb, t->cols[c].name);
    }
    /* FK-Metadaten vor den Ref-Loads in den Heap kopieren: sch_load kann
     * den Cache reallozieren und damit t->fks ungueltig machen. */
    int nf = t->nfks;
    struct { char *from, *to, *ref; int ridx; } *refs =
      malloc((size_t)(nf ? nf : 1) * sizeof *refs);
    for (int k = 0; k < nf; k++) {
      refs[k].from = strdup(t->fks[k].from);
      refs[k].to   = strdup(t->fks[k].to);
      refs[k].ref  = strdup(t->fks[k].ref);
      refs[k].ridx = -1;
    }
    for (int k = 0; k < nf; k++) refs[k].ridx = sch_load(db, refs[k].ref);
    for (int k = 0; k < nf; k++) {
      if (refs[k].ridx < 0) continue;
      SchTab *rt = &sch_cache[refs[k].ridx];
      if (!rt->dcol) continue;
      sqlite3_str_appendall(sb, ", (SELECT ");
      sb_ident(sb, rt->dcol);
      sqlite3_str_appendall(sb, " FROM ");
      sb_ident(sb, refs[k].ref);
      sqlite3_str_appendall(sb, " WHERE ");
      sb_ident(sb, refs[k].to);
      sqlite3_str_appendall(sb, " = gorgrow.");
      sb_ident(sb, refs[k].from);
      sqlite3_str_appendall(sb, ") AS ");
      size_t alen = strlen("gorgref_") + strlen(refs[k].from) + 1;
      char *alias = malloc(alen);
      if (alias) {
        snprintf(alias, alen, "gorgref_%s", refs[k].from);
        sb_ident(sb, alias);
        free(alias);
      }
    }
    for (int k = 0; k < nf; k++) {
      free(refs[k].from); free(refs[k].to); free(refs[k].ref);
    }
    free(refs);
    t = &sch_cache[ti];              /* nach den Loads neu lesen */
    sqlite3_str_appendall(sb, " FROM ");
    sb_ident(sb, t->name);
    sqlite3_str_appendall(sb, " AS gorgrow LIMIT ");
    char nb[32];
    snprintf(nb, sizeof nb, "%lld", (long long)cnt);
    sqlite3_str_appendall(sb, nb);
    sqlite3_str_appendall(sb, " OFFSET ");
    snprintf(nb, sizeof nb, "%lld", (long long)off);
    sqlite3_str_appendall(sb, nb);
    sqlite3_str_appendall(sb, ";");

    char *sql = sqlite3_str_finish(sb);
    if (sql) {
      sqlite3_stmt *stmt = NULL;
      if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        json_put("[");
        int first = 1;
        int sr;
        rc = SQLITE_OK;
        while ((sr = sqlite3_step(stmt)) == SQLITE_ROW) {
          if (!first) json_put(",");
          first = 0;
          emit_stmt(stmt);
        }
        json_put("]");
        if (sr != SQLITE_DONE) rc = sr;
        sqlite3_finalize(stmt);
      }
      sqlite3_free(sql);
    }
  } else {
    json_put("[]");
  }
  pthread_mutex_unlock(&srv_sqlite_lock);

  free(ct);

  json_finish(out, out_len);
  return rc;
}

int algol68_sqlite_close(sqlite3 *db)
{
  return sqlite3_close(db);
}

/* Liefert das komplette Schema als JSON-Array, damit das Frontend ein
 * ER-Diagramm zeichnen kann:
 *   [ {"name":"Album","cols":[{"name":"AlbumId","pk":1},...],
 *      "fks":[{"from":"ArtistId","to":"ArtistId","ref":"Artist"},...]}, ... ]
 * Pro Tabelle: Spalten in der deklarierten Reihenfolge (PRAGMA table_info,
 * pk ist der Primärschlüssel-Index, 0 = kein PK) und die Fremdschlüssel
 * (PRAGMA foreign_key_list: from/to-Spalte und ref = Referenztabelle). */
int algol68_sqlite_schema_json(sqlite3 *db,
                               uint32_t **out, size_t *out_len)
{
  *out = NULL;
  *out_len = 0;

  pthread_mutex_lock(&srv_sqlite_lock);
  json_len = 0;
  int rc = SQLITE_OK;

  static const char *qt =
    "SELECT name FROM sqlite_master WHERE type='table' "
    "AND name NOT LIKE 'sqlite_%' ORDER BY name;";
  sqlite3_stmt *st = NULL;
  if (sqlite3_prepare_v2(db, qt, -1, &st, NULL) != SQLITE_OK)
    rc = sqlite3_errcode(db);

  if (rc == SQLITE_OK) {
    json_put("[");
    int first_t = 1;
    while (sqlite3_step(st) == SQLITE_ROW) {
      const char *tname = (const char *)sqlite3_column_text(st, 0);
      if (!tname) continue;

      /* Tabellennamen als SQL-String-Literal quoten (Quote verdoppeln) */
      size_t need = strlen(tname) * 2 + 3;
      char *lit = malloc(need);
      if (!lit) { rc = SQLITE_NOMEM; break; }
      char *w = lit;
      *w++ = '\'';
      for (const unsigned char *p = (const unsigned char *)tname; *p; p++) {
        if (*p == '\'') *w++ = '\'';
        *w++ = (char)*p;
      }
      *w++ = '\'';
      *w = '\0';

      char pti[160], pfk[160];
      snprintf(pti, sizeof pti, "PRAGMA table_info(%s)", lit);
      snprintf(pfk, sizeof pfk, "PRAGMA foreign_key_list(%s)", lit);

      if (!first_t) json_put(",");
      first_t = 0;
      json_put("{\"name\":");
      json_quote(tname);
      json_put(",\"cols\":[");

      sqlite3_stmt *s2 = NULL;
      int cfirst = 1;
      if (sqlite3_prepare_v2(db, pti, -1, &s2, NULL) == SQLITE_OK) {
        while (sqlite3_step(s2) == SQLITE_ROW) {
          if (!cfirst) json_put(",");
          cfirst = 0;
          json_put("{\"name\":");
          json_quote((const char *)sqlite3_column_text(s2, 1));
          json_put(",\"pk\":");
          char nb[16];
          snprintf(nb, sizeof nb, "%d", sqlite3_column_int(s2, 5));
          json_put(nb);
          json_put("}");
        }
        sqlite3_finalize(s2);
      }

      json_put("],\"fks\":[");
      sqlite3_stmt *s3 = NULL;
      int ffirst = 1;
      if (sqlite3_prepare_v2(db, pfk, -1, &s3, NULL) == SQLITE_OK) {
        while (sqlite3_step(s3) == SQLITE_ROW) {
          if (!ffirst) json_put(",");
          ffirst = 0;
          json_put("{\"from\":");
          json_quote((const char *)sqlite3_column_text(s3, 3));
          json_put(",\"to\":");
          json_quote((const char *)sqlite3_column_text(s3, 4));
          json_put(",\"ref\":");
          json_quote((const char *)sqlite3_column_text(s3, 2));
          json_put("}");
        }
        sqlite3_finalize(s3);
      }
      json_put("]}");
      free(lit);
    }
    json_put("]");
    sqlite3_finalize(st);
  }

  pthread_mutex_unlock(&srv_sqlite_lock);

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