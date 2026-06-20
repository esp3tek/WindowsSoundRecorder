/* SPDX-License-Identifier: MIT */
#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <windows.h>
#include "encoder.h"

#define WM_APP_STATE    (WM_APP + 2)
#define STATE_WAITING   0
#define STATE_RECORDING 1
#define WM_APP_SAVED    (WM_APP + 3)   /* wParam = wchar_t* (path; el receptor hace free) */

typedef enum { CAPTURE_MANUAL, CAPTURE_AUTOSTART } CaptureMode;

typedef struct CaptureSession CaptureSession;

/* Manual: graba inmediatamente a un archivo (timestamp) en `folder` hasta capture_stop.
 * Autostart: arma; graba por tramos al detectar sonido; auto-para tras 5 s de silencio
 *   (sin registrar la cola de silencio) y re-arma; notifica el estado a `notify_hwnd`
 *   con WM_APP_STATE (wParam = STATE_*).
 * `fmt` elige WAV/MP3. Devuelve NULL y escribe `err` si el arranque falla. */
CaptureSession *capture_start(HWND notify_hwnd, CaptureMode mode, AudioFormat fmt,
                              const wchar_t *folder, wchar_t *err, size_t err_len);

void capture_stop(CaptureSession *s);

#endif /* AUDIO_CAPTURE_H */
