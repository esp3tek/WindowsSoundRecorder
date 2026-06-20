/* SPDX-License-Identifier: MIT
 * Codificador MP3 con Windows Media Foundation (IMFSinkWriter).
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mftransform.h>
#include <stdlib.h>
#include "mp3_encoder.h"

struct Mp3Enc {
    IMFSinkWriter *writer;
    DWORD     stream;
    WORD      channels;
    DWORD     rate;
    LONGLONG  sample_time;  /* en unidades de 100 ns */
    BOOL      mf_started;
};

/* Pide a Media Foundation los tipos de salida MP3 disponibles y devuelve el que
 * coincide en canales/frecuencia y se acerca más a `avg_bytes_per_sec`. El llamante
 * debe liberar el IMFMediaType devuelto. */
static IMFMediaType *get_mp3_output_type(WORD channels, DWORD rate, DWORD avg_bytes_per_sec) {
    IMFCollection *types = NULL;
    if (FAILED(MFTranscodeGetAudioOutputAvailableTypes(&MFAudioFormat_MP3,
            MFT_ENUM_FLAG_ALL, NULL, &types)) || !types)
        return NULL;

    DWORD count = 0;
    IMFCollection_GetElementCount(types, &count);

    IMFMediaType *best = NULL;
    DWORD best_diff = 0xFFFFFFFF;
    DWORD i;
    for (i = 0; i < count; i++) {
        IUnknown *unk = NULL;
        if (FAILED(IMFCollection_GetElement(types, i, &unk)) || !unk) continue;
        IMFMediaType *t = NULL;
        IUnknown_QueryInterface(unk, &IID_IMFMediaType, (void **)&t);
        IUnknown_Release(unk);
        if (!t) continue;

        UINT32 ch = 0, sr = 0, abps = 0;
        IMFMediaType_GetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, &ch);
        IMFMediaType_GetUINT32(t, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &sr);
        IMFMediaType_GetUINT32(t, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &abps);
        if (ch == channels && sr == rate) {
            DWORD diff = (abps > avg_bytes_per_sec) ? (abps - avg_bytes_per_sec)
                                                    : (avg_bytes_per_sec - abps);
            if (diff < best_diff) {
                if (best) IMFMediaType_Release(best);
                best = t;
                IMFMediaType_AddRef(best);
                best_diff = diff;
            }
        }
        IMFMediaType_Release(t);
    }
    IMFCollection_Release(types);
    return best;
}

Mp3Enc *mp3_open(const wchar_t *path, WORD channels, DWORD rate,
                 DWORD avg_bytes_per_sec, wchar_t *err, size_t err_len) {
    Mp3Enc *e = (Mp3Enc *)calloc(1, sizeof(Mp3Enc));
    if (!e) { if (err) wcsncpy(err, L"Out of memory.", err_len - 1); return NULL; }
    e->channels = channels;
    e->rate = rate;

    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(hr)) { if (err) wcsncpy(err, L"Media Foundation is not available.", err_len - 1); free(e); return NULL; }
    e->mf_started = TRUE;

    IMFMediaType *outType = NULL;
    IMFMediaType *inType  = NULL;

    hr = MFCreateSinkWriterFromURL(path, NULL, NULL, &e->writer);
    if (FAILED(hr)) { if (err) wcsncpy(err, L"Could not create the MP3 file.", err_len - 1); goto fail; }

    /* Output: MP3 — usar un tipo válido que el codificador anuncie. */
    outType = get_mp3_output_type(channels, rate, avg_bytes_per_sec);
    if (!outType) { if (err) wcsncpy(err, L"MP3 encoder not available.", err_len - 1); goto fail; }
    hr = IMFSinkWriter_AddStream(e->writer, outType, &e->stream);
    if (FAILED(hr)) { if (err) wcsncpy(err, L"MP3 encoder not available.", err_len - 1); goto fail; }

    /* Input: PCM 16-bit */
    if (FAILED(MFCreateMediaType(&inType))) goto fail_generic;
    IMFMediaType_SetGUID(inType, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(inType, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
    IMFMediaType_SetUINT32(inType, &MF_MT_AUDIO_NUM_CHANNELS, channels);
    IMFMediaType_SetUINT32(inType, &MF_MT_AUDIO_SAMPLES_PER_SECOND, rate);
    IMFMediaType_SetUINT32(inType, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    IMFMediaType_SetUINT32(inType, &MF_MT_AUDIO_BLOCK_ALIGNMENT, (UINT32)(channels * 2));
    IMFMediaType_SetUINT32(inType, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, rate * channels * 2);
    IMFMediaType_SetUINT32(inType, &MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    hr = IMFSinkWriter_SetInputMediaType(e->writer, e->stream, inType, NULL);
    if (FAILED(hr)) { if (err) wcsncpy(err, L"MP3 input format rejected.", err_len - 1); goto fail; }

    hr = IMFSinkWriter_BeginWriting(e->writer);
    if (FAILED(hr)) { if (err) wcsncpy(err, L"Could not start the MP3 writer.", err_len - 1); goto fail; }

    IMFMediaType_Release(outType);
    IMFMediaType_Release(inType);
    return e;

fail_generic:
    if (err) wcsncpy(err, L"Media Foundation error.", err_len - 1);
fail:
    if (outType) IMFMediaType_Release(outType);
    if (inType)  IMFMediaType_Release(inType);
    if (e->writer) IMFSinkWriter_Release(e->writer);
    if (e->mf_started) MFShutdown();
    free(e);
    return NULL;
}

BOOL mp3_write_pcm16(Mp3Enc *e, const INT16 *pcm, UINT32 frames) {
    if (!e || frames == 0) return TRUE;
    DWORD bytes = frames * e->channels * (DWORD)sizeof(INT16);
    IMFMediaBuffer *buf = NULL;
    if (FAILED(MFCreateMemoryBuffer(bytes, &buf))) return FALSE;

    BYTE *p = NULL;
    if (FAILED(IMFMediaBuffer_Lock(buf, &p, NULL, NULL))) { IMFMediaBuffer_Release(buf); return FALSE; }
    memcpy(p, pcm, bytes);
    IMFMediaBuffer_Unlock(buf);
    IMFMediaBuffer_SetCurrentLength(buf, bytes);

    IMFSample *sample = NULL;
    if (FAILED(MFCreateSample(&sample))) { IMFMediaBuffer_Release(buf); return FALSE; }
    IMFSample_AddBuffer(sample, buf);

    LONGLONG dur = (LONGLONG)frames * 10000000LL / (LONGLONG)e->rate;
    IMFSample_SetSampleTime(sample, e->sample_time);
    IMFSample_SetSampleDuration(sample, dur);
    e->sample_time += dur;

    HRESULT hr = IMFSinkWriter_WriteSample(e->writer, e->stream, sample);

    IMFSample_Release(sample);
    IMFMediaBuffer_Release(buf);
    return SUCCEEDED(hr);
}

void mp3_close(Mp3Enc *e) {
    if (!e) return;
    if (e->writer) {
        IMFSinkWriter_Finalize(e->writer);
        IMFSinkWriter_Release(e->writer);
    }
    if (e->mf_started) MFShutdown();
    free(e);
}
