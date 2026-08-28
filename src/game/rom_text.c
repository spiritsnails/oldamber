
#include "rom_text.h"
#include "../platform/game_version.h"
#include "text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct { const char *sym; const char *text; } rom_text_entry_t;
static rom_text_entry_t *s_table;
static char *s_blob;
static int s_table_n, s_tried;

#define ROM_TEXT_MAX 2048

static void load_table(void) {

    char kPaths[2][160];
    snprintf(kPaths[0], sizeof(kPaths[0]),
             "mod_runtime/generatedmaps/%s/scene_text.tbl", GameVersion_Current());
    snprintf(kPaths[1], sizeof(kPaths[1]),
             "../mod_runtime/generatedmaps/%s/scene_text.tbl", GameVersion_Current());
    FILE *f = NULL;
    long len;
    int cap = 0;

    s_tried = 1;
    for (size_t i = 0; i < sizeof kPaths / sizeof kPaths[0] && !f; i++)
        f = fopen(kPaths[i], "rb");
    if (!f) {
        printf("[rom_text] scene_text.tbl not found -- RomText() will fail. "
               "Run: tools/romimport/emit_scene_text.py\n");
        return;
    }
    fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return; }
    s_blob = (char *)malloc((size_t)len + 1);
    if (!s_blob) { fclose(f); return; }
    len = (long)fread(s_blob, 1, (size_t)len, f);
    s_blob[len] = '\0';
    fclose(f);

    for (char *p = s_blob; *p; ) {
        char *nl = strchr(p, '\n');
        char *tab = strchr(p, '\t');
        if (nl) *nl = '\0';
        if (*p != '#' && tab && (!nl || tab < nl)) {
            if (s_table_n == cap) {
                int nc = cap ? cap * 2 : 512;
                rom_text_entry_t *g = (rom_text_entry_t *)realloc(
                    s_table, (size_t)nc * sizeof *g);
                if (!g) break;
                s_table = g; cap = nc;
            }
            *tab = '\0';
            s_table[s_table_n].sym = p;
            size_t tl = strlen(tab + 1);
            if (tl && (tab + 1)[tl - 1] == '\r') (tab + 1)[tl - 1] = '\0';
            s_table[s_table_n].text = tab + 1;
            s_table_n++;
        }
        if (!nl) break;
        p = nl + 1;
    }
    printf("[rom_text] scene_text.tbl: %d ROM strings\n", s_table_n);
}

static void unescape(char *s) {
    char out[ROM_TEXT_MAX];
    int oi = 0;
    for (int i = 0; s[i] && oi + 1 < (int)sizeof(out); i++) {
        if (s[i] == '\\' && s[i + 1]) {
            char ch = s[i + 1];
            if (ch == 'n') { out[oi++] = '\n'; i++; continue; }
            if (ch == 'f') { out[oi++] = '\f'; i++; continue; }

            if (ch == 'c') { out[oi++] = TEXT_ASCII_CONT; i++; continue; }
            if (ch == 't') { out[oi++] = '\t'; i++; continue; }
            if (ch == '\\') { out[oi++] = '\\'; i++; continue; }
            if (ch == '"') { out[oi++] = '"'; i++; continue; }
        }

        if (s[i] == '#' && oi + 4 < (int)sizeof(out)) {
            out[oi++] = 'P'; out[oi++] = 'O'; out[oi++] = 'K';
            out[oi++] = (char)0xE9;
            continue;
        }

        if ((unsigned char)s[i] == 0xC3 && (unsigned char)s[i + 1] == 0xA9) {
            out[oi++] = (char)0xE9;
            i++;
            continue;
        }
        out[oi++] = s[i];
    }
    out[oi] = '\0';
    snprintf(s, ROM_TEXT_MAX, "%s", out);
}

typedef struct { const char *sym; char *text; } rom_text_cache_t;
static rom_text_cache_t *s_cache;
static int s_cache_n, s_cache_cap;

const char *RomText(const char *symbol) {
    if (!s_tried) load_table();
    for (int i = 0; i < s_cache_n; i++)
        if (strcmp(s_cache[i].sym, symbol) == 0)
            return s_cache[i].text;
    for (int i = 0; i < s_table_n; i++) {
        if (strcmp(s_table[i].sym, symbol) != 0) continue;
        char buf[ROM_TEXT_MAX];
        snprintf(buf, sizeof buf, "%s", s_table[i].text);
        unescape(buf);
        char *copy = (char *)malloc(strlen(buf) + 1);
        if (!copy) return s_table[i].text;
        memcpy(copy, buf, strlen(buf) + 1);
        if (s_cache_n == s_cache_cap) {
            int nc = s_cache_cap ? s_cache_cap * 2 : 128;
            rom_text_cache_t *g = (rom_text_cache_t *)realloc(
                s_cache, (size_t)nc * sizeof *g);
            if (!g) return copy;
            s_cache = g; s_cache_cap = nc;
        }
        s_cache[s_cache_n].sym = symbol;
        s_cache[s_cache_n].text = copy;
        s_cache_n++;
        return copy;
    }

    printf("[rom_text] RomText(%s) -- no such symbol in scene_text.tbl\n", symbol);
    return "";
}

const char *PortText(const char *literal) {
    typedef struct { const char *key; char *text; } port_cache_t;
    static port_cache_t *cache;
    static int n, cap;
    if (!literal) return "";
    for (int i = 0; i < n; i++)
        if (cache[i].key == literal) return cache[i].text;

    char buf[ROM_TEXT_MAX];
    snprintf(buf, sizeof buf, "%s", literal);
    unescape(buf);
    char *copy = (char *)malloc(strlen(buf) + 1);
    if (!copy) return literal;
    memcpy(copy, buf, strlen(buf) + 1);
    if (n == cap) {
        int nc = cap ? cap * 2 : 32;
        port_cache_t *g = (port_cache_t *)realloc(cache, (size_t)nc * sizeof *g);
        if (!g) return copy;
        cache = g; cap = nc;
    }
    cache[n].key = literal; cache[n].text = copy; n++;
    return copy;
}

void RomTextSpliceN(char *dst, size_t dst_size, const char *symbol, ...) {
    if (!dst || dst_size == 0) return;
    snprintf(dst, dst_size, "%s", RomText(symbol));

    va_list ap;
    va_start(ap, symbol);
    for (;;) {
        const char *token = va_arg(ap, const char *);
        if (!token) break;
        const char *value = va_arg(ap, const char *);
        if (!value) value = "";

        char work[ROM_TEXT_MAX];
        size_t tlen = strlen(token);
        size_t wi = 0;
        for (size_t i = 0; dst[i] && wi + 1 < sizeof work; ) {
            if (tlen && strncmp(dst + i, token, tlen) == 0) {
                for (size_t v = 0; value[v] && wi + 1 < sizeof work; v++)
                    work[wi++] = value[v];
                i += tlen;
            } else {
                work[wi++] = dst[i++];
            }
        }
        work[wi] = '\0';
        snprintf(dst, dst_size, "%s", work);
    }
    va_end(ap);
}

const char *RomTextPrefixed(const char *prefix, const char *symbol) {
    static rom_text_cache_t *cache;
    static int n, cap;
    for (int i = 0; i < n; i++)
        if (strcmp(cache[i].sym, symbol) == 0 &&
            strncmp(cache[i].text, prefix, strlen(prefix)) == 0)
            return cache[i].text;
    const char *body = RomText(symbol);
    size_t len = strlen(prefix) + strlen(body) + 1;
    char *joined = (char *)malloc(len);
    if (!joined) return body;
    snprintf(joined, len, "%s%s", prefix, body);
    if (n == cap) {
        int nc = cap ? cap * 2 : 32;
        rom_text_cache_t *g = (rom_text_cache_t *)realloc(
            cache, (size_t)nc * sizeof *g);
        if (!g) return joined;
        cache = g; cap = nc;
    }
    cache[n].sym = symbol; cache[n].text = joined; n++;
    return joined;
}

typedef struct { const char *sym; int page; char *text; } rom_text_page_cache_t;

const char *RomTextPage(const char *symbol, int page_index) {
    static rom_text_page_cache_t *cache;
    static int n, cap;
    for (int i = 0; i < n; i++)
        if (cache[i].page == page_index && strcmp(cache[i].sym, symbol) == 0)
            return cache[i].text;

    const char *full = RomText(symbol);
    const char *p = full;
    for (int page = 0; page < page_index; page++) {
        p = strchr(p, '\f');
        if (!p) {
            printf("[rom_text] RomTextPage(%s, %d) -- fewer than %d pages\n",
                   symbol, page_index, page_index + 1);
            return "";
        }
        p++;
    }
    const char *end = strchr(p, '\f');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    char *copy = (char *)malloc(len + 1);
    if (!copy) return "";
    memcpy(copy, p, len);
    copy[len] = '\0';

    if (n == cap) {
        int nc = cap ? cap * 2 : 32;
        rom_text_page_cache_t *g = (rom_text_page_cache_t *)realloc(
            cache, (size_t)nc * sizeof *g);
        if (!g) return copy;
        cache = g; cap = nc;
    }
    cache[n].sym = symbol; cache[n].page = page_index; cache[n].text = copy; n++;
    return copy;
}

void RomTextSplice(char *dst, size_t dst_size, const char *symbol,
                   const char *token, const char *value) {
    const char *src = RomText(symbol);
    size_t tlen = strlen(token);
    size_t o = 0;
    while (*src && o + 1 < dst_size) {
        if (tlen && strncmp(src, token, tlen) == 0) {
            size_t vlen = strlen(value);
            size_t room = dst_size - 1 - o;
            if (vlen > room) vlen = room;
            memcpy(dst + o, value, vlen);
            o += vlen;
            src += tlen;
        } else {
            dst[o++] = *src++;
        }
    }
    dst[o] = '\0';
}
