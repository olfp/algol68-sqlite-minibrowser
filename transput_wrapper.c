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