/* SPDX-License-Identifier: MIT */
#include "wav.h"
#include <math.h>

BOOL wav_write_header(FILE *f, WORD channels, DWORD sample_rate) {
    WORD  bits_per_sample = 16;
    WORD  block_align = (WORD)(channels * bits_per_sample / 8);
    DWORD byte_rate   = sample_rate * block_align;
    DWORD zero = 0;
    WORD  audio_format = 1; /* PCM */
    DWORD fmt_size = 16;

    if (fseek(f, 0, SEEK_SET) != 0) return FALSE;
    if (fwrite("RIFF", 1, 4, f) != 4) return FALSE;
    if (fwrite(&zero, 4, 1, f) != 1) return FALSE;        /* RIFF size (placeholder) */
    if (fwrite("WAVE", 1, 4, f) != 4) return FALSE;
    if (fwrite("fmt ", 1, 4, f) != 4) return FALSE;
    if (fwrite(&fmt_size, 4, 1, f) != 1) return FALSE;
    if (fwrite(&audio_format, 2, 1, f) != 1) return FALSE;
    if (fwrite(&channels, 2, 1, f) != 1) return FALSE;
    if (fwrite(&sample_rate, 4, 1, f) != 1) return FALSE;
    if (fwrite(&byte_rate, 4, 1, f) != 1) return FALSE;
    if (fwrite(&block_align, 2, 1, f) != 1) return FALSE;
    if (fwrite(&bits_per_sample, 2, 1, f) != 1) return FALSE;
    if (fwrite("data", 1, 4, f) != 4) return FALSE;
    if (fwrite(&zero, 4, 1, f) != 1) return FALSE;        /* data size (placeholder) */
    return TRUE;
}

BOOL wav_finalize(FILE *f, DWORD data_bytes) {
    DWORD riff_size = 36 + data_bytes;
    if (fseek(f, 4, SEEK_SET) != 0) return FALSE;
    if (fwrite(&riff_size, 4, 1, f) != 1) return FALSE;
    if (fseek(f, 40, SEEK_SET) != 0) return FALSE;
    if (fwrite(&data_bytes, 4, 1, f) != 1) return FALSE;
    return TRUE;
}

void wav_float_to_pcm16(const float *src, INT16 *dst, size_t count) {
    size_t i;
    for (i = 0; i < count; i++) {
        float v = src[i];
        if (v > 1.0f)  v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        dst[i] = (INT16)lrintf(v * 32767.0f);
    }
}
