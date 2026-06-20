/* SPDX-License-Identifier: MIT */
#ifndef AUDIO_UTIL_H
#define AUDIO_UTIL_H

#include <windows.h>
#include <stddef.h>

/* Devuelve el pico (máximo valor absoluto) de `count` muestras float. */
float audio_peak_f32(const float *src, size_t count);

/* Construye `dst` = <folder>\Recording_YYYY-MM-DD_HHMMSS.<ext> con la hora local.
 * `ext` es L"wav" o L"mp3". Añade el separador '\\' solo si `folder` no termina ya
 * en '\\' o '/'. */
void build_output_path(const wchar_t *folder, const wchar_t *ext,
                       wchar_t *dst, size_t len);

#endif /* AUDIO_UTIL_H */
