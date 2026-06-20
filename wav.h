/* SPDX-License-Identifier: MIT */
#ifndef WAV_H
#define WAV_H

#include <windows.h>
#include <stdio.h>
#include <stddef.h>

/* Escribe una cabecera WAV PCM 16-bit (44 bytes) con los campos de tamaño a 0.
 * Tras la llamada el puntero del archivo queda justo después de la cabecera.
 * Devuelve TRUE si la escritura fue correcta. */
BOOL wav_write_header(FILE *f, WORD channels, DWORD sample_rate);

/* Parchea los campos de tamaño RIFF (offset 4) y data (offset 40) con el número
 * real de bytes PCM escritos tras la cabecera. Devuelve TRUE si OK. */
BOOL wav_finalize(FILE *f, DWORD data_bytes);

/* Convierte `count` muestras float32 en [-1,1] a PCM 16-bit con saturación. */
void wav_float_to_pcm16(const float *src, INT16 *dst, size_t count);

#endif /* WAV_H */
