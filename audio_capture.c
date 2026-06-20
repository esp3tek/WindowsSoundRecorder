/* SPDX-License-Identifier: MIT
 * Captura WASAPI loopback -> encoder (WAV/MP3), con modo manual y autostart.
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdlib.h>
#include <string.h>
#include "audio_capture.h"
#include "encoder.h"
#include "audio_util.h"

#define REFTIMES_PER_SEC 10000000
#define SOUND_THRESHOLD  0.003f
#define SILENCE_STOP_MS  5000

struct CaptureSession {
    HANDLE        thread;
    volatile LONG running;
    HWND          notify_hwnd;
    CaptureMode   mode;
    AudioFormat   fmt;
    wchar_t       folder[MAX_PATH];
    wchar_t       cur_path[MAX_PATH];   /* path del archivo abierto actualmente */
    HANDLE        ready;
    BOOL          start_ok;
    wchar_t       err[256];
};

static void set_err(CaptureSession *s, const wchar_t *msg) {
    wcsncpy(s->err, msg, 255);
    s->err[255] = L'\0';
}

static void notify(CaptureSession *s, WPARAM state) {
    if (s->notify_hwnd) PostMessage(s->notify_hwnd, WM_APP_STATE, state, 0);
}

/* Notifica a la UI el path del archivo recién cerrado (la UI hace free del duplicado). */
static void notify_saved(CaptureSession *s) {
    if (s->notify_hwnd && s->cur_path[0]) {
        wchar_t *copy = _wcsdup(s->cur_path);
        if (copy) PostMessage(s->notify_hwnd, WM_APP_SAVED, (WPARAM)copy, 0);
    }
    s->cur_path[0] = L'\0';
}

static Encoder *open_take(CaptureSession *s, WORD ch, DWORD rate) {
    const wchar_t *ext = (s->fmt == FORMAT_MP3) ? L"mp3" : L"wav";
    build_output_path(s->folder, ext, s->cur_path, MAX_PATH);
    return encoder_open(s->cur_path, s->fmt, ch, rate, s->err, 256);
}

static DWORD WINAPI capture_proc(LPVOID param) {
    CaptureSession *s = (CaptureSession *)param;
    IMMDeviceEnumerator *enumr = NULL;
    IMMDevice           *dev   = NULL;
    IAudioClient        *client = NULL;
    IAudioCaptureClient *capture = NULL;
    WAVEFORMATEX        *wfx = NULL;
    Encoder             *enc = NULL;
    float               *zero = NULL;    /* buffer de ceros para silencio en manual */
    float               *hold = NULL;    /* búfer de retención (autostart) */
    size_t               hold_cap = 0;   /* capacidad en muestras float */
    size_t               hold_used = 0;  /* muestras float retenidas */
    ULONGLONG            last_sound = 0; /* tick del último buffer con sonido */
    int                  state = STATE_WAITING;
    HRESULT              hr;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    BOOL com_ok = SUCCEEDED(hr);

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void **)&enumr);
    if (FAILED(hr)) { set_err(s, L"Could not create the audio enumerator."); goto fail; }
    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumr, eRender, eConsole, &dev);
    if (FAILED(hr)) { set_err(s, L"No audio output device."); goto fail; }
    hr = IMMDevice_Activate(dev, &IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&client);
    if (FAILED(hr)) { set_err(s, L"Could not activate the audio client."); goto fail; }
    hr = IAudioClient_GetMixFormat(client, &wfx);
    if (FAILED(hr) || !wfx) { set_err(s, L"Could not get the audio format."); goto fail; }
    if (wfx->wBitsPerSample != 32) { set_err(s, L"Unsupported audio format (expected 32-bit float)."); goto fail; }

    hr = IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK, REFTIMES_PER_SEC, 0, wfx, NULL);
    if (FAILED(hr)) { set_err(s, L"Could not initialize loopback capture."); goto fail; }
    hr = IAudioClient_GetService(client, &IID_IAudioCaptureClient, (void **)&capture);
    if (FAILED(hr)) { set_err(s, L"Could not get the capture service."); goto fail; }

    WORD  ch   = wfx->nChannels;
    DWORD rate = wfx->nSamplesPerSec;

    /* Búfer de retención: 5 s de audio + 1 s de margen (autostart). */
    hold_cap = ((size_t)SILENCE_STOP_MS * rate / 1000 + rate) * ch;
    hold = (float *)malloc(hold_cap * sizeof(float));
    if (!hold) { set_err(s, L"Out of memory."); goto fail; }

    if (s->mode == CAPTURE_MANUAL) {
        enc = open_take(s, ch, rate);
        if (!enc) goto fail;   /* s->err ya contiene el motivo */
        state = STATE_RECORDING;
    } else {
        state = STATE_WAITING;
    }

    hr = IAudioClient_Start(client);
    if (FAILED(hr)) { set_err(s, L"Could not start capture."); goto fail; }

    s->start_ok = TRUE;
    SetEvent(s->ready);
    if (s->mode == CAPTURE_AUTOSTART) notify(s, STATE_WAITING);

    while (InterlockedCompareExchange(&s->running, 1, 1) == 1) {
        Sleep(10);

        /* Auto-stop por silencio medido en reloj de pared (robusto aunque el
         * loopback deje de entregar buffers cuando no hay reproducción). */
        if (s->mode == CAPTURE_AUTOSTART && state == STATE_RECORDING &&
            (GetTickCount64() - last_sound) >= SILENCE_STOP_MS) {
            encoder_close(enc); enc = NULL;
            hold_used = 0;
            state = STATE_WAITING;
            notify_saved(s);
            notify(s, STATE_WAITING);
        }

        UINT32 packet = 0;
        if (FAILED(IAudioCaptureClient_GetNextPacketSize(capture, &packet))) break;
        while (packet > 0) {
            BYTE  *data = NULL;
            UINT32 frames = 0;
            DWORD  flags = 0;
            if (FAILED(IAudioCaptureClient_GetBuffer(capture, &data, &frames, &flags, NULL, NULL))) break;
            size_t samples = (size_t)frames * ch;

            BOOL silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
            const float *fbuf;
            if (silent) {
                if (!zero) zero = (float *)calloc(hold_cap, sizeof(float));
                fbuf = zero;   /* ceros */
            } else {
                fbuf = (const float *)data;
            }
            float peak = (silent || !fbuf) ? 0.0f : audio_peak_f32(fbuf, samples);

            if (s->mode == CAPTURE_MANUAL) {
                if (fbuf) encoder_write(enc, fbuf, frames);
            } else if (state == STATE_WAITING) {
                if (peak >= SOUND_THRESHOLD) {
                    enc = open_take(s, ch, rate);
                    if (!enc) { IAudioCaptureClient_ReleaseBuffer(capture, frames); goto fail; }
                    encoder_write(enc, fbuf, frames);
                    state = STATE_RECORDING;
                    hold_used = 0;
                    last_sound = GetTickCount64();
                    notify(s, STATE_RECORDING);
                }
            } else { /* STATE_RECORDING (autostart) */
                if (peak >= SOUND_THRESHOLD) {
                    if (hold_used > 0) {
                        /* Volcar el silencio retenido: conserva pausas < 5 s. */
                        encoder_write(enc, hold, (UINT32)(hold_used / ch));
                        hold_used = 0;
                    }
                    encoder_write(enc, fbuf, frames);
                    last_sound = GetTickCount64();
                } else {
                    /* Silencio: retener (no escribir aún). El corte lo decide el reloj. */
                    if (fbuf && hold_used + samples <= hold_cap) {
                        memcpy(hold + hold_used, fbuf, samples * sizeof(float));
                        hold_used += samples;
                    }
                }
            }

            IAudioCaptureClient_ReleaseBuffer(capture, frames);
            if (FAILED(IAudioCaptureClient_GetNextPacketSize(capture, &packet))) break;
        }
    }

    IAudioClient_Stop(client);
    if (enc) { encoder_close(enc); enc = NULL; notify_saved(s); }
    goto cleanup;

fail:
    s->start_ok = FALSE;
    if (enc) { encoder_close(enc); enc = NULL; }
    SetEvent(s->ready);

cleanup:
    free(zero);
    free(hold);
    if (wfx)     CoTaskMemFree(wfx);
    if (capture) IAudioCaptureClient_Release(capture);
    if (client)  IAudioClient_Release(client);
    if (dev)     IMMDevice_Release(dev);
    if (enumr)   IMMDeviceEnumerator_Release(enumr);
    if (com_ok)  CoUninitialize();
    return 0;
}

CaptureSession *capture_start(HWND notify_hwnd, CaptureMode mode, AudioFormat fmt,
                              const wchar_t *folder, wchar_t *err, size_t err_len) {
    CaptureSession *s = (CaptureSession *)calloc(1, sizeof(CaptureSession));
    if (!s) { if (err) wcsncpy(err, L"Out of memory.", err_len - 1); return NULL; }
    s->notify_hwnd = notify_hwnd;
    s->mode = mode;
    s->fmt = fmt;
    wcsncpy(s->folder, folder, MAX_PATH - 1);
    s->running = 1;
    s->ready = CreateEvent(NULL, TRUE, FALSE, NULL);

    s->thread = CreateThread(NULL, 0, capture_proc, s, 0, NULL);
    if (!s->thread) {
        if (err) wcsncpy(err, L"Could not create the capture thread.", err_len - 1);
        CloseHandle(s->ready); free(s); return NULL;
    }
    WaitForSingleObject(s->ready, INFINITE);
    if (!s->start_ok) {
        if (err) { wcsncpy(err, s->err, err_len - 1); err[err_len - 1] = L'\0'; }
        WaitForSingleObject(s->thread, INFINITE);
        CloseHandle(s->thread); CloseHandle(s->ready); free(s);
        return NULL;
    }
    return s;
}

void capture_stop(CaptureSession *s) {
    if (!s) return;
    InterlockedExchange(&s->running, 0);
    WaitForSingleObject(s->thread, INFINITE);
    CloseHandle(s->thread);
    CloseHandle(s->ready);
    free(s);
}
