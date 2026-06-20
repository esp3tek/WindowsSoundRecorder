/* SPDX-License-Identifier: MIT */
#include "wav.h"
#include "audio_util.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("ok: %s\n", msg); } \
} while (0)

static DWORD read_u32(FILE *f, long off) {
    DWORD v = 0;
    fseek(f, off, SEEK_SET);
    fread(&v, sizeof(v), 1, f);
    return v;
}

int main(void) {
    /* Conversión float -> PCM16 */
    float in[5]  = { 0.0f, 1.0f, -1.0f, 0.5f, 2.0f /* clamp */ };
    INT16 out[5] = { 0 };
    wav_float_to_pcm16(in, out, 5);
    CHECK(out[0] == 0,      "0.0 -> 0");
    CHECK(out[1] == 32767,  "1.0 -> 32767");
    CHECK(out[2] == -32767, "-1.0 -> -32767");
    CHECK(out[3] >= 16000 && out[3] <= 16500, "0.5 -> ~16383");
    CHECK(out[4] == 32767,  "2.0 clamp -> 32767");

    /* Cabecera + finalize */
    FILE *f = fopen("test_out.wav", "wb+");
    CHECK(f != NULL, "abre archivo temporal");
    if (f) {
        BOOL h = wav_write_header(f, 2, 48000);
        CHECK(h, "escribe cabecera");
        CHECK(ftell(f) == 44, "puntero tras cabecera = 44");
        /* Simula 1000 bytes de datos PCM */
        char dummy[1000];
        memset(dummy, 0, sizeof(dummy));
        fwrite(dummy, 1, sizeof(dummy), f);
        BOOL fin = wav_finalize(f, 1000);
        CHECK(fin, "finalize OK");
        CHECK(read_u32(f, 4)  == 36 + 1000, "RIFF size = 36 + data");
        CHECK(read_u32(f, 40) == 1000,       "data size = 1000");
        /* sampleRate en offset 24, byteRate en 28 */
        CHECK(read_u32(f, 24) == 48000,        "sampleRate = 48000");
        CHECK(read_u32(f, 28) == 48000 * 2 * 2, "byteRate = sr*ch*2");
        fclose(f);
        remove("test_out.wav");
    }

    /* audio_peak_f32 */
    float silent[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    CHECK(audio_peak_f32(silent, 4) == 0.0f, "peak de silencio = 0");
    float mixed[4] = { 0.1f, -0.5f, 0.2f, -0.3f };
    CHECK(audio_peak_f32(mixed, 4) == 0.5f, "peak usa valor absoluto = 0.5");
    float low[2] = { 0.001f, -0.002f };
    CHECK(audio_peak_f32(low, 2) < 0.003f, "ruido bajo < umbral 0.003");
    float hi[2] = { 0.004f, 0.0f };
    CHECK(audio_peak_f32(hi, 2) >= 0.003f, "señal >= umbral 0.003");

    /* build_output_path */
    wchar_t p1[MAX_PATH];
    build_output_path(L"C:\\Temp", L"wav", p1, MAX_PATH);
    CHECK(wcsstr(p1, L"C:\\Temp\\Recording_") == p1, "ruta con separador añadido");
    CHECK(wcsstr(p1, L".wav") != NULL, "extensión .wav");
    wchar_t p2[MAX_PATH];
    build_output_path(L"C:\\Temp\\", L"mp3", p2, MAX_PATH);
    CHECK(wcsstr(p2, L"C:\\Temp\\Recording_") == p2, "no duplica separador");
    CHECK(wcsstr(p2, L".mp3") != NULL, "extensión .mp3");

    if (failures == 0) { printf("\nALL TESTS PASSED\n"); return 0; }
    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
