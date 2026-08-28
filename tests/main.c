
#include "test_runner.h"
#include <stdio.h>
#include "../src/platform/assetpack.h"
#include "assetpack_bind.h"
#include "../src/data/event_data.h"
#include "../src/data/dex_data.h"

int main(void) {

    char err1[256], err[256];
    if (!Pkg_MountList("packages", err1, sizeof err1) &&
        !Pkg_MountList("../packages", err1, sizeof err1)) {
        if (!AssetPack_Open(ASSETPACK_DEFAULT_PATH, err, sizeof err) &&
            !AssetPack_Open("../" ASSETPACK_DEFAULT_PATH, err, sizeof err)) {
            fprintf(stderr, "%s\n%s\n", err1, err);
            return 1;
        }
    }
    AssetPack_BindAll();
    MapEvents_LoadFromPack();
    DexEntries_LoadFromPack();

    RUN_ALL_TESTS();
}
