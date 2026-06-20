/* SPDX-License-Identifier: MIT */
#include "tags.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sanitize_filename(const wchar_t *name, wchar_t *out, size_t len) {
    static const wchar_t *bad = L"\\/:*?\"<>|";
    size_t j = 0;
    for (size_t i = 0; name[i] && j + 1 < len; i++) {
        wchar_t c = name[i];
        out[j++] = (wcschr(bad, c) || c < 32) ? L'_' : c;
    }
    out[j] = L'\0';
    /* recortar espacios al inicio y espacios/underscores al final */
    size_t start = 0;
    while (out[start] == L' ') start++;
    if (start) memmove(out, out + start, (wcslen(out + start) + 1) * sizeof(wchar_t));
    size_t n = wcslen(out);
    while (n > 0 && (out[n-1] == L' ' || out[n-1] == L'_')) out[--n] = L'\0';
}

/* Escribe un frame de texto ID3v2.3 (encoding 01 = UTF-16 con BOM). Devuelve bytes. */
static size_t write_text_frame(unsigned char *p, const char id[4], const wchar_t *text) {
    size_t chars = wcslen(text);
    size_t payload = 1 /*encoding*/ + 2 /*BOM*/ + (chars + 1) * 2 /*UTF-16 + NUL*/;
    memcpy(p, id, 4);
    p[4] = (unsigned char)((payload >> 24) & 0xFF);
    p[5] = (unsigned char)((payload >> 16) & 0xFF);
    p[6] = (unsigned char)((payload >> 8) & 0xFF);
    p[7] = (unsigned char)(payload & 0xFF);
    p[8] = 0; p[9] = 0;                 /* flags */
    p[10] = 0x01;                       /* encoding UTF-16 with BOM */
    p[11] = 0xFF; p[12] = 0xFE;         /* BOM LE */
    memcpy(p + 13, text, (chars + 1) * 2);
    return 10 + payload;
}

size_t id3v2_build(const wchar_t *title, const wchar_t *artist, const wchar_t *album,
                   unsigned char *buf, size_t cap) {
    if (cap < 10) return 0;
    unsigned char frames[4096];
    size_t fl = 0;
    fl += write_text_frame(frames + fl, "TIT2", title);
    fl += write_text_frame(frames + fl, "TPE1", artist);
    fl += write_text_frame(frames + fl, "TALB", album);
    if (10 + fl > cap) return 0;
    memcpy(buf, "ID3", 3);
    buf[3] = 3; buf[4] = 0;             /* version 2.3.0 */
    buf[5] = 0;                          /* flags */
    buf[6] = (unsigned char)((fl >> 21) & 0x7F);
    buf[7] = (unsigned char)((fl >> 14) & 0x7F);
    buf[8] = (unsigned char)((fl >> 7) & 0x7F);
    buf[9] = (unsigned char)(fl & 0x7F);
    memcpy(buf + 10, frames, fl);
    return 10 + fl;
}

BOOL write_id3v2(const wchar_t *mp3_path,
                 const wchar_t *title, const wchar_t *artist, const wchar_t *album) {
    unsigned char tag[4096];
    size_t tlen = id3v2_build(title, artist, album, tag, sizeof(tag));
    if (!tlen) return FALSE;

    FILE *f = _wfopen(mp3_path, L"rb");
    if (!f) return FALSE;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *orig = (unsigned char *)malloc(sz > 0 ? (size_t)sz : 1);
    if (!orig) { fclose(f); return FALSE; }
    size_t rd = fread(orig, 1, (size_t)sz, f);
    fclose(f);

    FILE *o = _wfopen(mp3_path, L"wb");
    if (!o) { free(orig); return FALSE; }
    fwrite(tag, 1, tlen, o);
    fwrite(orig, 1, rd, o);
    fclose(o);
    free(orig);
    return TRUE;
}

BOOL rename_with_song(const wchar_t *path, const wchar_t *artist, const wchar_t *title,
                      wchar_t *new_path, size_t len) {
    wchar_t folder[MAX_PATH];
    wcsncpy(folder, path, MAX_PATH - 1); folder[MAX_PATH-1] = 0;
    wchar_t *slash = wcsrchr(folder, L'\\');
    const wchar_t *ext = wcsrchr(path, L'.');
    if (!ext) ext = L"";
    if (slash) *slash = L'\0'; else wcscpy(folder, L".");

    wchar_t raw[512];
    swprintf(raw, 512, L"%s - %s", artist, title);
    wchar_t clean[512];
    sanitize_filename(raw, clean, 512);

    for (int n = 1; n < 1000; n++) {
        if (n == 1) swprintf(new_path, len, L"%s\\%s%s", folder, clean, ext);
        else        swprintf(new_path, len, L"%s\\%s (%d)%s", folder, clean, n, ext);
        if (GetFileAttributesW(new_path) == INVALID_FILE_ATTRIBUTES)
            return MoveFileW(path, new_path);
    }
    return FALSE;
}
