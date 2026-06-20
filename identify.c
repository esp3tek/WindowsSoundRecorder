/* SPDX-License-Identifier: MIT */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "identify.h"
#include "wav.h"

/* Decodifica un string JSON UTF-8 (con escapes) a UTF-16 en `out`. `src` apunta tras la
 * comilla de apertura; se detiene en la comilla de cierre no escapada. */
static void json_string_to_w(const char *src, wchar_t *out, size_t out_len) {
    char utf8[1024];
    size_t j = 0;
    for (size_t i = 0; src[i] && src[i] != '"' && j + 4 < sizeof(utf8); i++) {
        if (src[i] == '\\' && src[i+1]) {
            char e = src[++i];
            if (e == 'n') utf8[j++] = '\n';
            else if (e == 't') utf8[j++] = '\t';
            else if (e == 'u') {
                unsigned v = 0; char hex[5] = {0};
                for (int k = 0; k < 4 && src[i+1]; k++) hex[k] = src[++i];
                v = (unsigned)strtoul(hex, NULL, 16);
                if (v < 0x80) utf8[j++] = (char)v;
                else if (v < 0x800) { utf8[j++] = (char)(0xC0 | (v >> 6)); utf8[j++] = (char)(0x80 | (v & 0x3F)); }
                else { utf8[j++] = (char)(0xE0 | (v >> 12)); utf8[j++] = (char)(0x80 | ((v >> 6) & 0x3F)); utf8[j++] = (char)(0x80 | (v & 0x3F)); }
            }
            else utf8[j++] = e;   /* \" \\ \/ etc. */
        } else {
            utf8[j++] = src[i];
        }
    }
    utf8[j] = '\0';
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, (int)out_len);
}

/* Busca "key": dentro de `obj` y, si el valor es un string, lo decodifica a `out`. */
static const char *extract_string(const char *obj, const char *key,
                                  wchar_t *out, size_t out_len) {
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(obj, needle);
    if (!p) { out[0] = L'\0'; return NULL; }
    p += strlen(needle);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') { out[0] = L'\0'; return p; }
    json_string_to_w(p + 1, out, out_len);
    return p;
}

BOOL audd_parse_json(const char *json, SongInfo *out, wchar_t *err, size_t err_len) {
    memset(out, 0, sizeof(*out));
    if (strstr(json, "\"status\":\"error\"")) {
        wchar_t msg[256] = L"AudD error";
        extract_string(json, "error_message", msg, 256);
        if (err) { wcsncpy(err, msg, err_len - 1); err[err_len - 1] = L'\0'; }
        return FALSE;
    }
    const char *res = strstr(json, "\"result\"");
    if (!res) { if (err) wcsncpy(err, L"Unexpected response.", err_len - 1); return FALSE; }
    const char *after = res + strlen("\"result\"");
    while (*after == ' ' || *after == ':') after++;
    if (strncmp(after, "null", 4) == 0) { out->found = FALSE; return TRUE; }

    extract_string(after, "artist", out->artist, 256);
    extract_string(after, "title",  out->title,  256);
    extract_string(after, "album",  out->album,  256);
    out->found = (out->title[0] != L'\0');
    return TRUE;
}

BOOL wav_make_clip(const wchar_t *src_wav, const wchar_t *dst_wav, int seconds) {
    FILE *in = _wfopen(src_wav, L"rb");
    if (!in) return FALSE;
    unsigned char hdr[44];
    if (fread(hdr, 1, 44, in) != 44) { fclose(in); return FALSE; }
    WORD  channels    = *(WORD *)(hdr + 22);
    DWORD sample_rate = *(DWORD *)(hdr + 24);
    WORD  bits        = *(WORD *)(hdr + 34);
    DWORD data_size   = *(DWORD *)(hdr + 40);
    DWORD byte_rate   = sample_rate * channels * (bits / 8);
    DWORD want = byte_rate * (DWORD)seconds;
    if (want > data_size) want = data_size;

    FILE *out = _wfopen(dst_wav, L"wb");
    if (!out) { fclose(in); return FALSE; }
    *(DWORD *)(hdr + 4)  = 36 + want;
    *(DWORD *)(hdr + 40) = want;
    fwrite(hdr, 1, 44, out);
    unsigned char buf[65536];
    DWORD left = want;
    while (left > 0) {
        size_t chunk = left < sizeof(buf) ? left : sizeof(buf);
        size_t rd = fread(buf, 1, chunk, in);
        if (rd == 0) break;
        fwrite(buf, 1, rd, out);
        left -= (DWORD)rd;
    }
    fclose(in); fclose(out);
    return TRUE;
}

/* Lee un archivo entero a memoria. Devuelve el buffer (free por el llamante) y su tamaño. */
static unsigned char *read_all(const wchar_t *path, DWORD *size) {
    FILE *f = _wfopen(path, L"rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *buf = (unsigned char *)malloc(sz > 0 ? (size_t)sz : 1);
    if (!buf) { fclose(f); return NULL; }
    *size = (DWORD)fread(buf, 1, (size_t)sz, f);
    fclose(f);
    return buf;
}

BOOL identify_song(const wchar_t *audio_path, const char *api_key,
                   SongInfo *out, wchar_t *err, size_t err_len) {
    memset(out, 0, sizeof(*out));

    wchar_t temp[MAX_PATH];
    const wchar_t *send_path = audio_path;
    BOOL is_wav = (wcslen(audio_path) > 4 &&
                   _wcsicmp(audio_path + wcslen(audio_path) - 4, L".wav") == 0);
    BOOL made_temp = FALSE;
    if (is_wav) {
        wchar_t dir[MAX_PATH]; GetTempPathW(MAX_PATH, dir);
        swprintf(temp, MAX_PATH, L"%swsr_clip.wav", dir);
        if (wav_make_clip(audio_path, temp, 20)) { send_path = temp; made_temp = TRUE; }
    }

    DWORD fsize = 0;
    unsigned char *fdata = read_all(send_path, &fsize);
    if (made_temp) _wremove(temp);
    if (!fdata) { if (err) wcsncpy(err, L"Could not read the audio file.", err_len - 1); return FALSE; }

    const char *bnd = "----wsrAudDBoundary7f3a";
    char head[1024];
    int hl = snprintf(head, sizeof(head),
        "--%s\r\nContent-Disposition: form-data; name=\"api_token\"\r\n\r\n%s\r\n"
        "--%s\r\nContent-Disposition: form-data; name=\"return\"\r\n\r\nspotify,apple_music\r\n"
        "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"audio\"\r\n"
        "Content-Type: application/octet-stream\r\n\r\n",
        bnd, api_key, bnd, bnd);
    char tail[64];
    int tl = snprintf(tail, sizeof(tail), "\r\n--%s--\r\n", bnd);

    DWORD body_len = (DWORD)hl + fsize + (DWORD)tl;
    unsigned char *body = (unsigned char *)malloc(body_len);
    if (!body) { free(fdata); if (err) wcsncpy(err, L"Out of memory.", err_len - 1); return FALSE; }
    memcpy(body, head, hl);
    memcpy(body + hl, fdata, fsize);
    memcpy(body + hl + fsize, tail, tl);
    free(fdata);

    BOOL ok = FALSE;
    HINTERNET hS = WinHttpOpen(L"WindowsSoundRecorder/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET hC = hS ? WinHttpConnect(hS, L"api.audd.io", INTERNET_DEFAULT_HTTPS_PORT, 0) : NULL;
    HINTERNET hR = hC ? WinHttpOpenRequest(hC, L"POST", L"/", NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;

    if (hR) {
        wchar_t ctype[128];
        swprintf(ctype, 128, L"Content-Type: multipart/form-data; boundary=%S", bnd);
        WinHttpAddRequestHeaders(hR, ctype, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
        if (WinHttpSendRequest(hR, WINHTTP_NO_ADDITIONAL_HEADERS, 0, body, body_len, body_len, 0) &&
            WinHttpReceiveResponse(hR, NULL)) {
            char *resp = NULL; DWORD total = 0, avail = 0;
            do {
                if (!WinHttpQueryDataAvailable(hR, &avail) || avail == 0) break;
                char *nr = (char *)realloc(resp, total + avail + 1);
                if (!nr) break;
                resp = nr;
                DWORD got = 0;
                WinHttpReadData(hR, resp + total, avail, &got);
                total += got;
            } while (avail > 0);
            if (resp) {
                resp[total] = '\0';
                ok = audd_parse_json(resp, out, err, err_len);
                free(resp);
            } else if (err) {
                wcsncpy(err, L"Empty response from AudD.", err_len - 1);
            }
        } else if (err) {
            wcsncpy(err, L"Network error contacting AudD.", err_len - 1);
        }
    } else if (err) {
        wcsncpy(err, L"Could not open HTTPS connection.", err_len - 1);
    }

    if (hR) WinHttpCloseHandle(hR);
    if (hC) WinHttpCloseHandle(hC);
    if (hS) WinHttpCloseHandle(hS);
    free(body);
    return ok;
}
