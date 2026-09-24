/* Gorgona: minimaler C-Wrapper fuer den byte-exakten Transput.

 * ga68s POSIX-Transput ist fuer den Server NICHT byte-exakt: fgetc
 * dekodiert UTF-8 beim Lesen (eine Mehrbyte-Sequenz wird zu einem
 * Codepunkt), fputs kodiert beim Schreiben und schreibt zudem nur die
 * Zeichenanzahl aus dem UTF-8-Puffer (schneidet damit den Schwanz ab).
 *
 * Das Algol-68-Transput-Modul (transput.u68) legt deshalb nur diese zwei
 * minimalen Primitive zugrunde:
 *
 *  - algol68_read_file : Datei roh lesen (1 Byte -> 1 UCS-4-Zelle).
 *  - algol68_write_all : STRING byte-genau auf einen Descriptor schreiben
 *                        (pro Zelle das Niedrigwert-Byte, volle Laenge).
 *
 * Beide arbeiten bytes am Byte, unabhaengig von Locale oder Charset, und
 * halten damit das berechnete Content-Length im Server garantiert ein.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

/* Liest eine Datei komplett und liefert den Inhalt als ga68-STRING
 * (1 Byte -> 1 UCS-4-Zelle, NUL am Ende ist nicht enthalten).
 * Fehler -> leere Zeichenkette (Rueckgabe 1). Puffer ist thread-lokal,
 * weil die Worker-Threads Dateien parallel lesen. */
static __thread uint32_t *file_u32;
static __thread size_t file_cap;

int algol68_read_file(const uint32_t *path, size_t len, size_t stride,
                      uint32_t **out, size_t *out_len)
{
  char cpath[1024];
  if (len >= sizeof cpath) goto not_found;
  for (size_t i = 0; i < len; i++) {
    const uint32_t *p = (const uint32_t *)((const char *)path + i * stride);
    cpath[i] = (char)(*p & 0xFF);
  }
  cpath[len] = '\0';

  {
    FILE *f = fopen(cpath, "rb");
    if (!f) goto not_found;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); goto not_found; }
    long sz = ftell(f);
    rewind(f);
    if (sz < 0) { fclose(f); goto not_found; }

    char *buf = malloc((size_t)sz > 0 ? (size_t)sz : 1);
    if (!buf) { fclose(f); goto not_found; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    if (file_cap <= got) {
      uint32_t *nb = realloc(file_u32, (got + 1) * sizeof(uint32_t));
      if (!nb) { free(buf); goto not_found; }
      file_u32 = nb;
      file_cap = got + 1;
    }
    for (size_t i = 0; i < got; i++)
      file_u32[i] = (uint32_t)(unsigned char)buf[i];
    free(buf);

    *out = file_u32;
    *out_len = got;
    return 0;
  }

not_found:
  if (!file_u32) {
    file_u32 = malloc(sizeof(uint32_t));
    file_cap = file_u32 ? 1 : 0;
  }
  if (!file_u32) { *out = NULL; *out_len = 0; return -1; }
  file_u32[0] = 0;
  *out = file_u32;
  *out_len = 0;
  return 1;
}

/* Schreibt einen ga68-STRING ganze Dateien (Rebuild). Wird von
 * config-<Neuschreiben> fuer Add Database gebraucht: der Inhalt wird
 * zuerst in "<pfad>.tmp" geschrieben und dann atomar per rename()
 * an die Stelle des Originals gesetzt, damit die gorgona.conf bei einem
 * Absturz nicht halb fertig daliegt. Rueckgabe: 0 = ok, sonst < 0. */
int algol68_write_file(const uint32_t *path, size_t plen, size_t pstride,
                       const uint32_t *body, size_t blen, size_t bstride)
{
  char cpath[1024];
  if (plen >= sizeof cpath) return -1;
  for (size_t i = 0; i < plen; i++) {
    const uint32_t *p = (const uint32_t *)((const char *)path + i * pstride);
    cpath[i] = (char)(*p & 0xFF);
  }
  cpath[plen] = '\0';

  char tmp[1050];
  snprintf(tmp, sizeof tmp, "%s.tmp", cpath);
  FILE *f = fopen(tmp, "wb");
  if (!f) return -2;
  for (size_t i = 0; i < blen; i++) {
    const uint32_t *p = (const uint32_t *)((const char *)body + i * bstride);
    unsigned char c = (unsigned char)(*p & 0xFF);
    fwrite(&c, 1, 1, f);
  }
  if (fclose(f) != 0) return -3;
  if (rename(tmp, cpath) != 0) return -4;
  return 0;
}

/* Schreibt einen ga68-STRING byte-genau auf `fd`. Ankerpunkt der
 * byte-exakten Ausgabe: jeder Antwort-Body liegt als Zelle pro Byte vor
 * (exec_json, read_file), hier gehen die Bytes bit-genau wieder raus
 * (Laenge = Zeichenzahl = passt zum berechneten Content-Length). */
int algol68_write_all(int fd, const uint32_t *s, size_t len, size_t stride)
{
  size_t cap = len + 1;
  if (cap < 8192) cap = 8192;
  char *buf = malloc(cap);
  if (!buf) return -1;

  for (size_t i = 0; i < len; i++) {
    const uint32_t *p = (const uint32_t *)((const char *)s + i * stride);
    buf[i] = (char)(*p & 0xFF);
  }

  size_t off = 0;
  while (off < len) {
    ssize_t n = write(fd, buf + off, len - off);
    if (n < 0) {
      if (errno == EINTR) continue;
      free(buf);
      return -1;
    }
    off += (size_t)n;
  }

  free(buf);
  return 0;
}