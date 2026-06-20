/* SPDX-License-Identifier: MIT */
#ifndef IDENTIFY_H
#define IDENTIFY_H

#include <windows.h>
#include <stddef.h>

typedef struct {
    BOOL    found;
    wchar_t artist[256];
    wchar_t title[256];
    wchar_t album[256];
} SongInfo;

/* Parsea una respuesta JSON de AudD. Devuelve TRUE si la respuesta es válida
 * (status success): rellena `out`, con found=FALSE si result es null. Devuelve FALSE y
 * escribe `err` si status es error. */
BOOL audd_parse_json(const char *json, SongInfo *out, wchar_t *err, size_t err_len);

/* Crea un WAV temporal con los primeros `seconds` segundos de `src_wav` en `dst_wav`.
 * Devuelve TRUE si OK. */
BOOL wav_make_clip(const wchar_t *src_wav, const wchar_t *dst_wav, int seconds);

/* Envía el audio a AudD y rellena `out`. TRUE si la llamada se completó (out->found
 * indica si reconoció). FALSE + `err` ante fallo de red/clave/sin créditos. */
BOOL identify_song(const wchar_t *audio_path, const char *api_key,
                   SongInfo *out, wchar_t *err, size_t err_len);

#endif /* IDENTIFY_H */
