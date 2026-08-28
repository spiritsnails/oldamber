#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ASSETPACK_MAGIC   "PKRDPAK"
#define ASSETPACK_VERSION 1u

#define ASSETPACK_DEFAULT_PATH "assets.pak"

typedef struct {
    const void *data;
    uint32_t    size;
    uint32_t    count;
    uint32_t    stride;
} AssetPack_Entry;

bool AssetPack_Open(const char *path, char *err, size_t errsz);

void AssetPack_Close(void);

bool AssetPack_Find(const char *name, AssetPack_Entry *out);

const void *AssetPack_Require(const char *name, uint32_t *out_count);

const uint8_t *AssetPack_RomSha1(void);

#define PKG_MANIFEST_NAME "pkg.txt"
#define PKG_LIST_NAME     "packages.txt"
#define PKG_MAX_LAYERS    8

typedef struct {
    char     id[64];
    uint32_t version;
    uint32_t contract;
    uint8_t  rom_sha1[20];
    bool     has_rom_sha1;
    char     requires[128];
} Pkg_Manifest;

bool Pkg_Mount(const char *path, char *err, size_t errsz);

bool Pkg_MountList(const char *dir, char *err, size_t errsz);

void Pkg_UnmountAll(void);

int  Pkg_LayerCount(void);
const Pkg_Manifest *Pkg_LayerManifest(int i);
const char         *Pkg_LayerPath(int i);

bool Pkg_Find(const char *key, AssetPack_Entry *out, int *out_layer);

bool Pkg_CheckRequires(char *err, size_t errsz);

void Pkg_ReportOverrides(void (*sink)(const char *key, int winner, int loser,
                                      void *ctx), void *ctx);

bool AssetPack_IsOpen(void);

typedef struct {
    uint32_t state[5];
    uint64_t bitlen;
    uint8_t  buf[64];
    size_t   buflen;
} Sha1Ctx;

void Sha1_Init(Sha1Ctx *c);
void Sha1_Update(Sha1Ctx *c, const void *data, size_t len);
void Sha1_Final(Sha1Ctx *c, uint8_t out[20]);

bool Sha1_File(const char *path, uint8_t out[20]);

void Sha1_ToHex(const uint8_t digest[20], char out[41]);
