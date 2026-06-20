/* SPDX-License-Identifier: MIT */
#ifndef MP3_ENCODER_H
#define MP3_ENCODER_H

#include <windows.h>
#include <stddef.h>

typedef struct Mp3Enc Mp3Enc;

/* Abre un MP3 vía Media Foundation. `avg_bytes_per_sec` = bitrate/8 (320 kbps = 40000).
 * Devuelve NULL y escribe `err` si falla. */
Mp3Enc *mp3_open(const wchar_t *path, WORD channels, DWORD rate,
                 DWORD avg_bytes_per_sec, wchar_t *err, size_t err_len);

/* Escribe `frames` de PCM 16-bit intercalado. */
BOOL mp3_write_pcm16(Mp3Enc *e, const INT16 *pcm, UINT32 frames);

void mp3_close(Mp3Enc *e);

#endif /* MP3_ENCODER_H */
