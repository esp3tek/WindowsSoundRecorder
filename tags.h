/* SPDX-License-Identifier: MIT */
#ifndef TAGS_H
#define TAGS_H

#include <windows.h>
#include <stddef.h>

/* Sustituye los caracteres no válidos de nombre de archivo (\ / : * ? " < > |) por '_'
 * y recorta espacios al inicio/fin. `out` recibe el resultado (longitud `len`). */
void sanitize_filename(const wchar_t *name, wchar_t *out, size_t len);

/* Construye un tag ID3v2.3 (frames TIT2/TPE1/TALB, texto UTF-16) en `buf` (capacidad
 * `cap`). Devuelve el número de bytes escritos, o 0 si no cabe. */
size_t id3v2_build(const wchar_t *title, const wchar_t *artist, const wchar_t *album,
                   unsigned char *buf, size_t cap);

/* Reescribe el MP3 con un tag ID3v2.3 al principio. Devuelve TRUE si OK. */
BOOL write_id3v2(const wchar_t *mp3_path,
                 const wchar_t *title, const wchar_t *artist, const wchar_t *album);

/* Renombra `path` a "<carpeta>\Artista - Título.<ext>" (saneado). Si ya existe añade
 * " (2)", " (3)"... Escribe el nuevo path en `new_path` (longitud `len`). TRUE si OK. */
BOOL rename_with_song(const wchar_t *path, const wchar_t *artist, const wchar_t *title,
                      wchar_t *new_path, size_t len);

#endif /* TAGS_H */
