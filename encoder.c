/* SPDX-License-Identifier: MIT */
#include "encoder.h"
#include "wav.h"
#include "mp3_encoder.h"
#include <stdio.h>
#include <stdlib.h>

struct Encoder {
    AudioFormat fmt;
    /* WAV */
    FILE  *f;
    DWORD  data_bytes;
    WORD   channels;
    INT16 *pcm;
    size_t pcm_cap;   /* en muestras INT16 */
    wchar_t path[MAX_PATH];
    /* MP3 */
    Mp3Enc *mp3;
};

static BOOL ensure_pcm(Encoder *e, size_t samples) {
    if (e->pcm_cap >= samples) return TRUE;
    INT16 *p = (INT16 *)realloc(e->pcm, samples * sizeof(INT16));
    if (!p) return FALSE;
    e->pcm = p;
    e->pcm_cap = samples;
    return TRUE;
}

Encoder *encoder_open(const wchar_t *path, AudioFormat fmt,
                      WORD channels, DWORD rate, wchar_t *err, size_t err_len) {
    Encoder *e = (Encoder *)calloc(1, sizeof(Encoder));
    if (!e) { if (err) wcsncpy(err, L"Out of memory.", err_len - 1); return NULL; }
    e->fmt = fmt;
    e->channels = channels;
    wcsncpy(e->path, path, MAX_PATH - 1);

    if (fmt == FORMAT_WAV) {
        e->f = _wfopen(path, L"wb+");
        if (!e->f) { if (err) wcsncpy(err, L"Could not create the recording file.", err_len - 1); free(e); return NULL; }
        if (!wav_write_header(e->f, channels, rate)) {
            if (err) wcsncpy(err, L"Could not write the WAV header.", err_len - 1);
            fclose(e->f); free(e); return NULL;
        }
        return e;
    }

    /* FORMAT_MP3: 320 kbps CBR = 40000 bytes/s */
    e->mp3 = mp3_open(path, channels, rate, 40000, err, err_len);
    if (!e->mp3) { free(e); return NULL; }
    return e;
}

BOOL encoder_write(Encoder *e, const float *interleaved, UINT32 frames) {
    if (!e) return FALSE;
    size_t samples = (size_t)frames * e->channels;
    if (e->fmt == FORMAT_WAV) {
        if (!ensure_pcm(e, samples)) return FALSE;
        wav_float_to_pcm16(interleaved, e->pcm, samples);
        if (fwrite(e->pcm, sizeof(INT16), samples, e->f) != samples) return FALSE;
        e->data_bytes += (DWORD)(samples * sizeof(INT16));
        return TRUE;
    }
    if (e->fmt == FORMAT_MP3) {
        if (!ensure_pcm(e, samples)) return FALSE;
        wav_float_to_pcm16(interleaved, e->pcm, samples);
        return mp3_write_pcm16(e->mp3, e->pcm, frames);
    }
    return FALSE;
}

void encoder_close(Encoder *e) {
    if (!e) return;
    if (e->fmt == FORMAT_WAV && e->f) {
        wav_finalize(e->f, e->data_bytes);
        fclose(e->f);
    }
    if (e->fmt == FORMAT_MP3 && e->mp3) mp3_close(e->mp3);
    free(e->pcm);
    free(e);
}
