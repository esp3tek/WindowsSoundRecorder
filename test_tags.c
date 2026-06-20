/* SPDX-License-Identifier: MIT */
#include "tags.h"
#include "identify.h"
#include "wav.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("ok: %s\n", msg); } \
} while (0)

int main(void) {
    /* sanitize_filename */
    wchar_t out[256];
    sanitize_filename(L"AC/DC: Back\\In*Black?", out, 256);
    CHECK(wcschr(out, L'/') == NULL && wcschr(out, L'\\') == NULL &&
          wcschr(out, L':') == NULL && wcschr(out, L'*') == NULL &&
          wcschr(out, L'?') == NULL, "sanitize quita caracteres invalidos");
    sanitize_filename(L"  Trim me  ", out, 256);
    CHECK(out[0] == L'T' && out[wcslen(out)-1] == L'e', "sanitize recorta espacios");

    /* id3v2_build */
    unsigned char tag[512];
    size_t tlen = id3v2_build(L"Title", L"Artist", L"Album", tag, sizeof(tag));
    CHECK(tlen > 10, "id3v2_build devuelve bytes");
    CHECK(tag[0] == 'I' && tag[1] == 'D' && tag[2] == '3', "tag empieza con 'ID3'");
    CHECK(tag[3] == 3 && tag[4] == 0, "version ID3v2.3");
    size_t declared = ((size_t)tag[6] << 21) | ((size_t)tag[7] << 14) |
                      ((size_t)tag[8] << 7) | (size_t)tag[9];
    CHECK(declared == tlen - 10, "tamano sincsafe correcto");
    int found_tit2 = 0;
    for (size_t i = 0; i + 4 <= tlen; i++)
        if (memcmp(tag + i, "TIT2", 4) == 0) { found_tit2 = 1; break; }
    CHECK(found_tit2, "frame TIT2 presente");

    /* audd_parse_json */
    SongInfo si;
    const char *ok_json =
      "{\"status\":\"success\",\"result\":{\"artist\":\"Queen\","
      "\"title\":\"Bohemian Rhapsody\",\"album\":\"A Night at the Opera\"}}";
    wchar_t perr[256];
    CHECK(audd_parse_json(ok_json, &si, perr, 256), "parse success ok");
    CHECK(si.found, "found = TRUE");
    CHECK(wcscmp(si.artist, L"Queen") == 0, "artist = Queen");
    CHECK(wcscmp(si.title, L"Bohemian Rhapsody") == 0, "title correcto");

    const char *null_json = "{\"status\":\"success\",\"result\":null}";
    CHECK(audd_parse_json(null_json, &si, perr, 256) && !si.found, "result null -> not found");

    const char *err_json =
      "{\"status\":\"error\",\"error\":{\"error_code\":901,"
      "\"error_message\":\"no api_token\"}}";
    CHECK(!audd_parse_json(err_json, &si, perr, 256), "status error -> FALSE");

    /* wav_make_clip */
    {
        FILE *w = fopen("clip_src.wav", "wb+");
        wav_write_header(w, 2, 48000);
        DWORD frames = 48000 * 3;
        short sample[2] = {0, 0};
        for (DWORD i = 0; i < frames; i++) fwrite(sample, sizeof(short), 2, w);
        wav_finalize(w, frames * 2 * sizeof(short));
        fclose(w);

        CHECK(wav_make_clip(L"clip_src.wav", L"clip_dst.wav", 1), "wav_make_clip OK");
        FILE *d = fopen("clip_dst.wav", "rb");
        CHECK(d != NULL, "clip creado");
        if (d) {
            DWORD dataLen = 0;
            fseek(d, 40, SEEK_SET); fread(&dataLen, 4, 1, d); fclose(d);
            CHECK(dataLen >= 180000 && dataLen <= 200000, "clip dura ~1 s");
        }
        remove("clip_src.wav"); remove("clip_dst.wav");
    }

    if (failures == 0) { printf("\nALL TESTS PASSED\n"); return 0; }
    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
