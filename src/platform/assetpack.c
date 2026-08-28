
#include "assetpack.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rol32(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

static void sha1_block(Sha1Ctx *c, const uint8_t *p)
{
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (int i = 16; i < 80; i++)
        w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = c->state[0], b = c->state[1], d = c->state[2];
    uint32_t e = c->state[3], f = c->state[4];

    for (int i = 0; i < 80; i++) {
        uint32_t k, t;
        if (i < 20)      { k = 0x5A827999u; t = (b & d) | (~b & e); }
        else if (i < 40) { k = 0x6ED9EBA1u; t = b ^ d ^ e; }
        else if (i < 60) { k = 0x8F1BBCDCu; t = (b & d) | (b & e) | (d & e); }
        else             { k = 0xCA62C1D6u; t = b ^ d ^ e; }
        t += rol32(a, 5) + f + k + w[i];
        f = e; e = d; d = rol32(b, 30); b = a; a = t;
    }
    c->state[0] += a; c->state[1] += b; c->state[2] += d;
    c->state[3] += e; c->state[4] += f;
}

void Sha1_Init(Sha1Ctx *c)
{
    c->state[0] = 0x67452301u; c->state[1] = 0xEFCDAB89u;
    c->state[2] = 0x98BADCFEu; c->state[3] = 0x10325476u;
    c->state[4] = 0xC3D2E1F0u;
    c->bitlen = 0;
    c->buflen = 0;
}

void Sha1_Update(Sha1Ctx *c, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    c->bitlen += (uint64_t)len * 8u;
    while (len > 0) {
        size_t n = 64 - c->buflen;
        if (n > len) n = len;
        memcpy(c->buf + c->buflen, p, n);
        c->buflen += n;
        p += n;
        len -= n;
        if (c->buflen == 64) { sha1_block(c, c->buf); c->buflen = 0; }
    }
}

void Sha1_Final(Sha1Ctx *c, uint8_t out[20])
{
    uint64_t bits = c->bitlen;
    uint8_t pad = 0x80;
    Sha1_Update(c, &pad, 1);
    pad = 0x00;
    while (c->buflen != 56) Sha1_Update(c, &pad, 1);

    uint8_t len[8];
    for (int i = 0; i < 8; i++) len[i] = (uint8_t)(bits >> (56 - i * 8));
    memcpy(c->buf + 56, len, 8);
    sha1_block(c, c->buf);
    c->buflen = 0;
    for (int i = 0; i < 20; i++)
        out[i] = (uint8_t)(c->state[i / 4] >> (24 - (i % 4) * 8));
}

bool Sha1_File(const char *path, uint8_t out[20])
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    Sha1Ctx c;
    Sha1_Init(&c);
    uint8_t buf[64 * 1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) Sha1_Update(&c, buf, n);
    bool ok = (ferror(f) == 0);
    fclose(f);
    if (ok) Sha1_Final(&c, out);
    return ok;
}

void Sha1_ToHex(const uint8_t digest[20], char out[41])
{
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 20; i++) {
        out[i * 2]     = hex[digest[i] >> 4];
        out[i * 2 + 1] = hex[digest[i] & 0xF];
    }
    out[40] = '\0';
}

#pragma pack(push, 1)
typedef struct {
    char     magic[8];
    uint32_t version;
    uint32_t entry_count;
    uint8_t  rom_sha1[20];
    uint32_t index_off;
    uint32_t names_off;
    uint32_t names_size;
    uint32_t data_off;
    uint32_t data_size;
    uint32_t reserved[2];
} PakHeader;

typedef struct {
    uint64_t name_hash;
    uint32_t name_off;
    uint32_t data_off;
    uint32_t size;
    uint32_t count;
    uint32_t stride;
    uint32_t reserved;
} PakIndexEntry;
#pragma pack(pop)

typedef struct DirBlob {
    struct DirBlob *next;
    char            key[128];
    uint8_t        *data;
    uint32_t        size;
} DirBlob;

typedef struct {
    bool          is_dir;
    char          path[512];
    Pkg_Manifest  manifest;

    uint8_t             *pack;
    size_t               pack_size;
    const PakHeader     *hdr;
    const PakIndexEntry *index;
    const char          *names;
    const uint8_t       *data;

    DirBlob *blobs;
} Layer;

static Layer s_layers[PKG_MAX_LAYERS];
static int   s_layer_count;

static uint64_t fnv1a64(const char *s)
{
    uint64_t h = 1469598103934665603ull;
    for (; *s; s++) {
        h ^= (uint8_t)*s;
        h *= 1099511628211ull;
    }
    return h;
}

static void fail(char *err, size_t errsz, const char *fmt, ...)
{
    if (!err || errsz == 0) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, errsz, fmt, ap);
    va_end(ap);
}

static bool mount_archive(Layer *L, const char *path, char *err, size_t errsz)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fail(err, errsz,
             "No asset pack at '%s'.\n"
             "\n"
             "This build ships no game assets. Build a pack from your own ROM:\n"
             "    pwsh tools/py.ps1 tools/assetpack/build_pak.py --rom <your-rom.gbc>",
             path);
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); fail(err, errsz, "Cannot seek '%s'.", path); return false; }
    long sz = ftell(f);
    if (sz < (long)sizeof(PakHeader)) {
        fclose(f);
        fail(err, errsz, "Asset pack '%s' is truncated (%ld bytes).", path, sz);
        return false;
    }
    rewind(f);

    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) { fclose(f); fail(err, errsz, "Out of memory reading '%s' (%ld bytes).", path, sz); return false; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) { free(buf); fail(err, errsz, "Short read on '%s'.", path); return false; }

    const PakHeader *h = (const PakHeader *)buf;
    if (memcmp(h->magic, ASSETPACK_MAGIC, 8) != 0) {
        free(buf);
        fail(err, errsz, "'%s' is not an asset pack (bad magic).", path);
        return false;
    }
    if (h->version != ASSETPACK_VERSION) {
        free(buf);
        fail(err, errsz,
             "Asset pack '%s' is version %u, this build needs version %u.\n"
             "Rebuild it: pwsh tools/py.ps1 tools/assetpack/build_pak.py --rom <your-rom.gbc>",
             path, h->version, ASSETPACK_VERSION);
        return false;
    }

    uint64_t idx_end   = (uint64_t)h->index_off + (uint64_t)h->entry_count * sizeof(PakIndexEntry);
    uint64_t names_end = (uint64_t)h->names_off + h->names_size;
    uint64_t data_end  = (uint64_t)h->data_off  + h->data_size;
    if (idx_end > (uint64_t)sz || names_end > (uint64_t)sz || data_end > (uint64_t)sz) {
        free(buf);
        fail(err, errsz, "Asset pack '%s' is corrupt (section past end of file).", path);
        return false;
    }
    if (h->names_size > 0 && buf[h->names_off + h->names_size - 1] != '\0') {
        free(buf);
        fail(err, errsz, "Asset pack '%s' is corrupt (unterminated name blob).", path);
        return false;
    }

    const PakIndexEntry *ix = (const PakIndexEntry *)(buf + h->index_off);
    for (uint32_t i = 0; i < h->entry_count; i++) {
        if ((uint64_t)ix[i].data_off + ix[i].size > h->data_size ||
            ix[i].name_off >= h->names_size ||
            (ix[i].stride != 0 && ix[i].count * ix[i].stride != ix[i].size)) {
            free(buf);
            fail(err, errsz, "Asset pack '%s' is corrupt (bad index entry %u).", path, i);
            return false;
        }
        if (i > 0 && ix[i - 1].name_hash > ix[i].name_hash) {
            free(buf);
            fail(err, errsz, "Asset pack '%s' is corrupt (index not sorted at %u).", path, i);
            return false;
        }
    }

    L->pack      = buf;
    L->pack_size = (size_t)sz;
    L->hdr       = h;
    L->index     = ix;
    L->names     = (const char *)(buf + h->names_off);
    L->data      = buf + h->data_off;

    snprintf(L->manifest.id, sizeof L->manifest.id, "%s", path);
    L->manifest.version  = h->version;
    L->manifest.contract = h->version;
    memcpy(L->manifest.rom_sha1, h->rom_sha1, 20);
    L->manifest.has_rom_sha1 = true;
    return true;
}

static void trim(char *s)
{
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                 s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = '\0';
}

static bool parse_manifest(const char *path, Pkg_Manifest *m,
                           char *err, size_t errsz)
{
    char file[600];
    snprintf(file, sizeof file, "%s/%s", path, PKG_MANIFEST_NAME);
    FILE *f = fopen(file, "r");
    if (!f) {
        fail(err, errsz,
             "'%s' is a directory but has no %s.\n"
             "A package directory needs a manifest naming it, so a conflict can "
             "be reported by id instead of by path.",
             path, PKG_MANIFEST_NAME);
        return false;
    }
    memset(m, 0, sizeof *m);
    m->version = 1;
    m->contract = ASSETPACK_VERSION;

    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char key[64], val[400];
        snprintf(key, sizeof key, "%s", line);
        snprintf(val, sizeof val, "%s", eq + 1);
        trim(key);
        trim(val);
        if (!strcmp(key, "id"))            snprintf(m->id, sizeof m->id, "%s", val);
        else if (!strcmp(key, "version"))  m->version  = (uint32_t)strtoul(val, NULL, 0);
        else if (!strcmp(key, "contract")) m->contract = (uint32_t)strtoul(val, NULL, 0);
        else if (!strcmp(key, "requires")) snprintf(m->requires, sizeof m->requires, "%s", val);
        else if (!strcmp(key, "rom_sha1") && strlen(val) >= 40) {
            for (int i = 0; i < 20; i++) {
                char b[3] = { val[i * 2], val[i * 2 + 1], 0 };
                m->rom_sha1[i] = (uint8_t)strtoul(b, NULL, 16);
            }
            m->has_rom_sha1 = true;
        }
    }
    fclose(f);
    if (!m->id[0]) {
        fail(err, errsz, "%s/%s has no `id`.", path, PKG_MANIFEST_NAME);
        return false;
    }
    if (m->contract != ASSETPACK_VERSION) {
        fail(err, errsz,
             "Package '%s' declares contract %u; this build speaks %u.",
             m->id, m->contract, ASSETPACK_VERSION);
        return false;
    }
    return true;
}

static bool dir_find(Layer *L, const char *key, AssetPack_Entry *out)
{
    for (DirBlob *b = L->blobs; b; b = b->next) {
        if (!strcmp(b->key, key)) {
            if (out) {
                out->data = b->data; out->size = b->size;
                out->count = b->size; out->stride = 1;
            }
            return true;
        }
    }
    if (strlen(key) >= sizeof ((DirBlob *)0)->key) return false;
    if (strstr(key, "..")) return false;

    char file[700];
    snprintf(file, sizeof file, "%s/%s", L->path, key);
    FILE *f = fopen(file, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz < 0) { fclose(f); return false; }

    DirBlob *b = (DirBlob *)calloc(1, sizeof *b);
    if (!b) { fclose(f); return false; }
    b->data = (uint8_t *)malloc((size_t)sz + 1);
    if (!b->data) { free(b); fclose(f); return false; }
    size_t got = fread(b->data, 1, (size_t)sz, f);
    fclose(f);
    b->data[got] = '\0';
    b->size = (uint32_t)got;
    snprintf(b->key, sizeof b->key, "%s", key);
    b->next = L->blobs;
    L->blobs = b;
    if (out) {
        out->data = b->data; out->size = b->size;
        out->count = b->size; out->stride = 1;
    }
    return true;
}

static bool archive_find(const Layer *L, const char *key, AssetPack_Entry *out)
{
    uint64_t want = fnv1a64(key);
    uint32_t lo = 0, hi = L->hdr->entry_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (L->index[mid].name_hash < want)      lo = mid + 1;
        else if (L->index[mid].name_hash > want) hi = mid;
        else {

            while (mid > 0 && L->index[mid - 1].name_hash == want) mid--;
            for (; mid < L->hdr->entry_count && L->index[mid].name_hash == want; mid++) {
                if (strcmp(L->names + L->index[mid].name_off, key) == 0) {
                    if (out) {
                        out->data   = L->data + L->index[mid].data_off;
                        out->size   = L->index[mid].size;
                        out->count  = L->index[mid].count;
                        out->stride = L->index[mid].stride;
                    }
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}

static bool is_directory(const char *path)
{
    char probe[600];
    snprintf(probe, sizeof probe, "%s/%s", path, PKG_MANIFEST_NAME);
    FILE *f = fopen(probe, "r");
    if (f) { fclose(f); return true; }
    return false;
}

bool Pkg_Mount(const char *path, char *err, size_t errsz)
{
    if (s_layer_count >= PKG_MAX_LAYERS) {
        fail(err, errsz, "Too many packages mounted (max %d).", PKG_MAX_LAYERS);
        return false;
    }
    Layer *L = &s_layers[s_layer_count];
    memset(L, 0, sizeof *L);
    snprintf(L->path, sizeof L->path, "%s", path);

    if (is_directory(path)) {
        L->is_dir = true;
        if (!parse_manifest(path, &L->manifest, err, errsz)) return false;
    } else {
        if (!mount_archive(L, path, err, errsz)) return false;
    }
    s_layer_count++;
    return true;
}

void Pkg_UnmountAll(void)
{
    for (int i = 0; i < s_layer_count; i++) {
        Layer *L = &s_layers[i];
        free(L->pack);
        for (DirBlob *b = L->blobs; b; ) {
            DirBlob *n = b->next;
            free(b->data);
            free(b);
            b = n;
        }
        memset(L, 0, sizeof *L);
    }
    s_layer_count = 0;
}

int Pkg_LayerCount(void) { return s_layer_count; }

const Pkg_Manifest *Pkg_LayerManifest(int i)
{
    return (i >= 0 && i < s_layer_count) ? &s_layers[i].manifest : NULL;
}

const char *Pkg_LayerPath(int i)
{
    return (i >= 0 && i < s_layer_count) ? s_layers[i].path : NULL;
}

bool Pkg_Find(const char *key, AssetPack_Entry *out, int *out_layer)
{
    if (!key) return false;

    for (int i = 0; i < s_layer_count; i++) {
        Layer *L = &s_layers[i];
        bool hit = L->is_dir ? dir_find(L, key, out) : archive_find(L, key, out);
        if (hit) {
            if (out_layer) *out_layer = i;
            return true;
        }
    }
    return false;
}

bool Pkg_CheckRequires(char *err, size_t errsz)
{
    for (int i = 0; i < s_layer_count; i++) {
        const char *req = s_layers[i].manifest.requires;
        if (!req[0]) continue;
        char buf[128];
        snprintf(buf, sizeof buf, "%s", req);
        for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
            trim(tok);
            if (!*tok) continue;
            bool found = false;
            for (int j = 0; j < s_layer_count && !found; j++)
                found = (strcmp(s_layers[j].manifest.id, tok) == 0);
            if (!found) {
                fail(err, errsz,
                     "Package '%s' requires '%s', which is not mounted.\n"
                     "An override whose base is absent does nothing at all, "
                     "silently -- so this is an error, not a warning.",
                     s_layers[i].manifest.id, tok);
                return false;
            }
        }
    }
    return true;
}

void Pkg_ReportOverrides(void (*sink)(const char *, int, int, void *), void *ctx)
{
    if (!sink) return;

    for (int i = 0; i < s_layer_count; i++) {
        const Layer *L = &s_layers[i];
        if (L->is_dir) continue;
        for (uint32_t e = 0; e < L->hdr->entry_count; e++) {
            const char *key = L->names + L->index[e].name_off;
            int winner = -1;
            if (Pkg_Find(key, NULL, &winner) && winner >= 0 && winner < i)
                sink(key, winner, i, ctx);
        }
    }
}

bool Pkg_MountList(const char *dir, char *err, size_t errsz)
{
    char listpath[512];
    snprintf(listpath, sizeof listpath, "%s/%s", dir, PKG_LIST_NAME);
    FILE *f = fopen(listpath, "r");
    if (!f) {
        fail(err, errsz, "No package list at %s.", listpath);
        return false;
    }

    int mounted = 0;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        char *end = s + strlen(s);
        while (end > s && (end[-1] == '\n' || end[-1] == '\r' ||
                           end[-1] == ' '  || end[-1] == '\t')) *--end = '\0';
        if (!*s || *s == '#') continue;

        char pak[512];
        snprintf(pak, sizeof pak, "%s/%s.pak", dir, s);
        if (!Pkg_Mount(pak, err, errsz)) {
            fclose(f);
            return false;
        }
        mounted++;
    }
    fclose(f);

    if (mounted == 0) {
        fail(err, errsz, "%s lists no packages.", listpath);
        return false;
    }
    return Pkg_CheckRequires(err, errsz);
}

bool AssetPack_Open(const char *path, char *err, size_t errsz)
{
    Pkg_UnmountAll();
    return Pkg_Mount(path ? path : ASSETPACK_DEFAULT_PATH, err, errsz);
}

void AssetPack_Close(void) { Pkg_UnmountAll(); }

bool AssetPack_IsOpen(void) { return s_layer_count > 0; }

bool AssetPack_Find(const char *name, AssetPack_Entry *out)
{
    return Pkg_Find(name, out, NULL);
}

const void *AssetPack_Require(const char *name, uint32_t *out_count)
{
    AssetPack_Entry e;
    if (!AssetPack_Find(name, &e)) {
        fprintf(stderr,
                "FATAL: asset '%s' is missing from every mounted package.\n"
                "The pack does not match this build. Rebuild it:\n"
                "    pwsh tools/py.ps1 tools/assetpack/build_pak.py --rom <your-rom.gbc>\n",
                name);
        abort();
    }
    if (out_count) *out_count = e.count;
    return e.data;
}

const uint8_t *AssetPack_RomSha1(void)
{
    for (int i = 0; i < s_layer_count; i++)
        if (s_layers[i].manifest.has_rom_sha1)
            return s_layers[i].manifest.rom_sha1;
    return NULL;
}
