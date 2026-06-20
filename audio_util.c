/* SPDX-License-Identifier: MIT */
#include "audio_util.h"
#include <stdio.h>
#include <string.h>

float audio_peak_f32(const float *src, size_t count) {
    float peak = 0.0f;
    size_t i;
    for (i = 0; i < count; i++) {
        float a = src[i] < 0.0f ? -src[i] : src[i];
        if (a > peak) peak = a;
    }
    return peak;
}

void build_output_path(const wchar_t *folder, const wchar_t *ext,
                       wchar_t *dst, size_t len) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    size_t n = wcslen(folder);
    BOOL has_sep = (n > 0 && (folder[n - 1] == L'\\' || folder[n - 1] == L'/'));
    swprintf(dst, len, L"%s%sRecording_%04u-%02u-%02u_%02u%02u%02u.%s",
             folder, has_sep ? L"" : L"\\",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, ext);
}
