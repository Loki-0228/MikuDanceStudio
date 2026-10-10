#ifndef MMV_MENU_H
#define MMV_MENU_H
#include "mmv.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Optional export; leaves the existing renderer ABI unchanged. */
#define MMV_MENU_VERSION 1u
enum MmvMenuFlag { MMV_MENU_AUTO_RELOAD=1u, MMV_MENU_AUTO_SAVE=2u, MMV_MENU_RENDERER_DISABLED=4u };
enum MmvMenuCommand { MMV_MENU_SET_AUTO_RELOAD=1, MMV_MENU_SET_AUTO_SAVE=2,
    MMV_MENU_RELOAD_ALL=3, MMV_MENU_SHOW_LOG=4 };
typedef struct MmvMenuApi {
    uint32_t structureSize,version;
    uint32_t (MMV_CALL *flags)(void*);
    int (MMV_CALL *command)(void*,uint32_t,uint32_t);
} MmvMenuApi;
typedef int (MMV_CALL *MmvGetMenuApiFn)(uint32_t,MmvMenuApi*);
#ifdef __cplusplus
}
#endif
#endif
