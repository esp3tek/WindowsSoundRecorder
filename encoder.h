/* SPDX-License-Identifier: MIT */
#ifndef ENCODER_H
#define ENCODER_H

#include <windows.h>
#include <stddef.h>

typedef enum { FORMAT_WAV, FORMAT_MP3 } AudioFormat;
typedef struct Encoder Encoder;

/* Abre un archivo de salida en el formato dado. Devuelve NULL y escribe `err`
 * (longitud `err_len`) si falla. */
Encoder *encoder_open(const wchar_t *path, AudioFormat fmt,
                      WORD channels, DWORD rate, wchar_t *err, size_t err_len);

/* Escribe `frames` muestras (float intercalado, `channels` valores por frame). */
BOOL encoder_write(Encoder *e, const float *interleaved, UINT32 frames);

/* Finaliza y cierra. */
void encoder_close(Encoder *e);

#endif /* ENCODER_H */
