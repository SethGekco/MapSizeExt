#define _WIN32_WINNT 0x0600

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <windows.h>
#include <psapi.h>

#include "yr_map_512_patch_table.h"

struct HookEntry {
    uint32_t address;
    uint32_t size;
    const char* name;
} __attribute__((aligned(16)));

__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512PlaneActivate = {
    0x00565812,
    0x5,
    "Map512PlaneActivate",
};

__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512BoundaryProbe = {
    0x005657a5,
    0x7,
    "Map512BoundaryProbe",
};

__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512SubzoneIdCeilingGuard = {
    0x0058215b,
    0x5,
    "Map512SubzoneIdCeilingGuard",
};

__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512RegularPathExitProbe = {
    0x0042a43e,
    0x7,
    "Map512RegularPathExitProbe",
};

__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512HierarchicalFailureProbe = {
    0x0042cb7e,
    0x5,
    "Map512HierarchicalFailureProbe",
};

__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512RegularFailureProbe = {
    0x0042cc65,
    0x5,
    "Map512RegularFailureProbe",
};

__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512OverlayReadSuccessProbe = {
    0x005fd38c, 0x8, "Map512OverlayReadSuccessProbe",
};

__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512OverlayReadFailureProbe = {
    0x005fd546, 0x8, "Map512OverlayReadFailureProbe",
};

__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512OverlayDataReadSuccessProbe = {
    0x005fd58f, 0x8, "Map512OverlayDataReadSuccessProbe",
};

__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512OverlayDataReadFailureProbe = {
    0x005fd67c, 0x8, "Map512OverlayDataReadFailureProbe",
};

__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512LoadBeforePostLoadReinit = {
    0x0068512b, 0x5, "Map512LoadBeforePostLoadReinit",
};

__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512LoadAfterTabInit = {
    0x0067e694, 0x5, "Map512LoadAfterTabInit",
};

__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512CellSlotGuard = {
    0x005663bc, 0x5, "Map512CellSlotGuard",
};

__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512CellIteratorGuard = {
    0x00578290, 0x6, "Map512CellIteratorGuard",
};

__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512CoordTransformGuard = {
    0x00660540, 0x5, "Map512CoordTransformGuard",
};
/* DIAGNOSTIC probes (read-only, return 0 to continue). Identify the wall /
 * drag-select / infantry-exit regressions at 1024. Remove once resolved. */
__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512WallDrawProbe = {
    0x0047f96a, 0x6, "Map512WallDrawProbe",
};
__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512CellLookupCallerProbe = {
    0x005657bb, 0x6, "Map512CellLookupCallerProbe",
};
__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512CellMissProbe = {
    0x0056577a, 0x5, "Map512CellMissProbe",
};
__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512KickoutEnterProbe = {
    0x00443c60, 0x6, "Map512KickoutEnterProbe",
};
__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512KickoutFailProbe = {
    0x00445696, 0x5, "Map512KickoutFailProbe",
};
__attribute__((section(".syhks00"), used, aligned(16)))
static struct HookEntry hook_Map512ScatterProbe = {
    0x0051d0dd, 0x6, "Map512ScatterProbe",
};

typedef struct SyringeRegisters {
    uint32_t origin;
    uint32_t flags;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
} SyringeRegisters;

typedef struct SyringeHandshakeInfo {
    int32_t cb_size;
    int32_t num_hooks;
    uint32_t checksum;
    uint32_t exe_filesize;
    uint32_t exe_timestamp;
    uint32_t exe_crc;
    int32_t message_capacity;
    char* message;
} SyringeHandshakeInfo;

enum {
    GAME_FRAME_ADDRESS = 0x00a8ed84,
    MAP_INSTANCE_ADDRESS = 0x0087f7e8,
    MAP_CELLS_ITEMS_OFFSET = 0x13c,
    MAP_CELLS_CAPACITY_OFFSET = 0x140,
    MAP_MAX_WIDTH_OFFSET = 0x14c,
    MAP_MAX_HEIGHT_OFFSET = 0x150,
    MAP_MAX_CELLS_OFFSET = 0x154,
    CELL_AXIS_LIMIT = 1024,
    CELL_COUNT_LIMIT = 0x100000,
    EXPECTED_TIMESTAMP = 0x3bdf544e,
    EXPECTED_ENTRYPOINT_RVA = 0x003cd80f,
    STEAM_IMAGE_SIZE = 0x00804000,
    CNCNET_SPAWNER_IMAGE_SIZE = 0x00793000,
    STEAM_FILE_SIZE = 0x0050a940,
    STEAM_SYRINGE_CRC = 0xa3f19485,
    CNCNET_SPAWNER_FILE_SIZE = 0x00497110,
    CNCNET_SPAWNER_SYRINGE_CRC = 0x098465b3,
    TESTER_CNCNET_SYRINGE_CRC = 0xd114b054,
    ACTIVATION_PATCH_ADDRESS = 0x00565812,
    SUBZONE_ID_RESERVED = 0xffff,
    SUBZONE_ID_SATURATED = 0xfffe,
    RECALCULATE_SUBZONES_OVERFLOW_CONTINUE = 0x005822d0,
};


static HANDLE g_log = INVALID_HANDLE_VALUE;
static int g_host_checked;
static int g_host_supported;
static int g_patch_status;
static uint32_t g_host_timestamp;
static uint32_t g_host_entrypoint;
static uint32_t g_host_image_size;
static uint32_t g_getcell_calls;
static uint32_t g_axis_invalid;
static uint32_t g_flat_invalid;
static uint32_t g_axis_row_carry;
static uint32_t g_invalid_samples;
static uint32_t g_max_x;
static uint32_t g_max_y;
static uint32_t g_last_frame = 0xffffffffu;
static uint32_t g_probe_base;
static uint32_t g_subzone_max_id;
static uint32_t g_subzone_ceiling_entries;
static uint32_t g_subzone_opcode_patches;
static uint32_t g_extension_patches;
static uint32_t g_regular_path_exits;
static uint32_t g_regular_path_cap_hits;
static uint32_t g_hierarchical_failures;
static uint32_t g_regular_failures;

typedef struct SubzoneOpcodePatch {
    uint32_t address;
    uint8_t size;
    uint8_t expected[5];
} SubzoneOpcodePatch;

typedef struct ModuleOpcodePatch {
    const char* module_name;
    uint32_t rva;
    uint8_t size;
    uint8_t expected[6];
    uint8_t replacement[6];
    const void* redirect_target;
} ModuleOpcodePatch;

static const SubzoneOpcodePatch g_subzone_opcode_patch_table[] = {
    {0x00429e9a, 3, {0x0f, 0xbf, 0x08, 0x00, 0x00}},
    {0x0042c34a, 4, {0x0f, 0xbf, 0x1c, 0x70, 0x00}},
    {0x0042c36a, 4, {0x0f, 0xbf, 0x04, 0x70, 0x00}},
    {0x00582575, 4, {0x0f, 0xbf, 0x04, 0x43, 0x00}},
    {0x005826fb, 4, {0x0f, 0xbf, 0x04, 0x56, 0x00}},
    {0x00582892, 5, {0x0f, 0xbf, 0x4c, 0x45, 0x00}},
    {0x00582ae6, 4, {0x0f, 0xbf, 0x0c, 0x4e, 0x00}},
    {0x00582fdd, 4, {0x0f, 0xbf, 0x34, 0x41, 0x00}},
    {0x00583006, 4, {0x0f, 0xbf, 0x0c, 0x42, 0x00}},
    {0x0058307b, 4, {0x0f, 0xbf, 0x34, 0x41, 0x00}},
    {0x005830a0, 4, {0x0f, 0xbf, 0x0c, 0x42, 0x00}},
    {0x00583115, 4, {0x0f, 0xbf, 0x34, 0x41, 0x00}},
    {0x0058313a, 4, {0x0f, 0xbf, 0x0c, 0x42, 0x00}},
    {0x00584de4, 3, {0x0f, 0xbf, 0x08, 0x00, 0x00}},
};

static const ModuleOpcodePatch g_module_opcode_patch_table[] = {
    {
        "Phobos.dll", 0x0003ea34, 3,
        {0xc1, 0xe3, 0x09, 0x00, 0x00, 0x00},
        {0xc1, 0xe3, 0x0a, 0x00, 0x00, 0x00},
        NULL
    },
    {
        "Phobos.dll", 0x0003ea3e, 6,
        {0x81, 0xfb, 0xff, 0xff, 0x03, 0x00},
        {0x81, 0xfb, 0xff, 0xff, 0x0f, 0x00},
        NULL
    },

    {"Antares.dll", 0x0000d34a, 3, {0xc1, 0xe7, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe7, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000160d2, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0001725d, 3, {0xc1, 0xe2, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe2, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00018480, 3, {0xc1, 0xe3, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe3, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000185f3, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00019e6a, 3, {0xc1, 0xe0, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe0, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0001a347, 3, {0xc1, 0xe2, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe2, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0001a3d7, 3, {0xc1, 0xe2, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe2, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0001c876, 3, {0xc1, 0xe7, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe7, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0001c8c1, 3, {0xc1, 0xe5, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe5, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000202c8, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000205c9, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00020cc7, 3, {0xc1, 0xe2, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe2, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0002d036, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0002dd33, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0003249e, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0003262e, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00032f7d, 3, {0xc1, 0xe2, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe2, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0003e9de, 3, {0xc1, 0xe6, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe6, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0003ee51, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0003f8c9, 3, {0xc1, 0xe6, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe6, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00042b5b, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00042c26, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00043071, 3, {0xc1, 0xe0, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe0, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00043547, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00043b48, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00043d86, 3, {0xc1, 0xe3, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe3, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00043f84, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00044696, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00044861, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00044a76, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000450cb, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000452bb, 3, {0xc1, 0xe7, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe7, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000532a0, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000576ae, 3, {0xc1, 0xe2, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe2, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00058b44, 3, {0xc1, 0xe2, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe2, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0005ca1c, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00060958, 3, {0xc1, 0xe3, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe3, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000632c1, 3, {0xc1, 0xe2, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe2, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00064621, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0006520d, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0006880b, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000688e5, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00068f25, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0007c882, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0007ce9f, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0007d2ff, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0007d4f2, 3, {0xc1, 0xe6, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe6, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0007d8a8, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0007db21, 3, {0xc1, 0xe3, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe3, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000838a0, 3, {0xc1, 0xe3, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe3, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00083923, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00083ac4, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00083b74, 3, {0xc1, 0xe7, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe7, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000844af, 3, {0xc1, 0xe6, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe6, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000848e6, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00084a45, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00084d59, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00085d07, 3, {0xc1, 0xe7, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe7, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0008693d, 3, {0xc1, 0xe3, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe3, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00086c47, 3, {0xc1, 0xe2, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe2, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000870a8, 3, {0xc1, 0xe3, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe3, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00087323, 3, {0xc1, 0xe6, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe6, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00087475, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00087565, 3, {0xc1, 0xe2, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe2, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000885e0, 3, {0xc1, 0xe2, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe2, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x00089c00, 3, {0xc1, 0xe6, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe6, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0008a83b, 3, {0xc1, 0xe2, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe2, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0008a9b6, 3, {0xc1, 0xe2, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe2, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0008af35, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0008b10a, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0008b995, 3, {0xc1, 0xe6, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe6, 0x0a, 0x00, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x0008beaf, 3, {0xc1, 0xe1, 0x09, 0x00, 0x00, 0x00}, {0xc1, 0xe1, 0x0a, 0x00, 0x00, 0x00}, NULL},
    /* Antares.dll cell-index bounds: cmp reg,MaxCells-1 (0x3FFFF -> 0xFFFFF)
       and cmp reg/eax,MaxCells (0x40000 -> 0x100000). Missing half of the
       Antares coverage; the broad build has these and survives the bottom-left
       radar-order crash (dangling 0x880A04 tactical singleton). 75 sites. */
    {"Antares.dll", 0x0000d357, 6, {0x81, 0xff, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xff, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000160df, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00017265, 6, {0x81, 0xfa, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfa, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0001848d, 6, {0x81, 0xfb, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfb, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000185f9, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00019e73, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0001a34c, 6, {0x81, 0xfa, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfa, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0001a3dc, 6, {0x81, 0xfa, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfa, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0001c882, 6, {0x81, 0xff, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xff, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0001c8c9, 6, {0x81, 0xfd, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfd, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000202d0, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000205d6, 6, {0x81, 0xff, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xff, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00020cd0, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0002d043, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0002dd41, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000324ab, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00032646, 6, {0x81, 0xfe, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfe, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00032f82, 6, {0x81, 0xfe, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfe, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0003e9e9, 6, {0x81, 0xfa, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfa, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0003ee59, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0003f8ce, 6, {0x81, 0xfe, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfe, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00042b61, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00042c2c, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0004307f, 6, {0x81, 0xfe, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfe, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0004354c, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00043b50, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00043d8e, 6, {0x81, 0xfb, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfb, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00043f8c, 6, {0x81, 0xff, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xff, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0004469c, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0004486c, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00044a7e, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000450d3, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000452c3, 6, {0x81, 0xff, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xff, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000532a9, 6, {0x81, 0xfe, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfe, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000576b3, 6, {0x81, 0xfa, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfa, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00058b4c, 6, {0x81, 0xfa, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfa, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0005ca24, 6, {0x81, 0xfe, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfe, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0006095d, 6, {0x81, 0xfb, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfb, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000632c9, 6, {0x81, 0xfa, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfa, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0006462d, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0006521a, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00068811, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000688eb, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00068f32, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0007c88a, 6, {0x81, 0xfe, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfe, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0007cea4, 6, {0x81, 0xfe, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfe, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0007d304, 6, {0x81, 0xfe, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfe, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0007d4f7, 6, {0x81, 0xfe, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfe, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0007d8b3, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0007db29, 6, {0x81, 0xfb, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfb, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000838ad, 6, {0x81, 0xfb, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfb, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0008392b, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00083ad1, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00083b81, 6, {0x81, 0xff, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xff, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000844b7, 6, {0x81, 0xfe, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfe, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000848ee, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00084a4d, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00084d66, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00085d0c, 6, {0x81, 0xff, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xff, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00086945, 6, {0x81, 0xfb, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfb, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00086c59, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000870b0, 6, {0x81, 0xfb, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfb, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0008732b, 6, {0x81, 0xfe, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfe, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0008747d, 6, {0x81, 0xfe, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfe, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00087571, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000885eb, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x00089c06, 6, {0x81, 0xfe, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfe, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0008a840, 6, {0x81, 0xfa, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfa, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0008a9be, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0008af3d, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0008b112, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0008b99e, 6, {0x81, 0xfa, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xfa, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x0008beb7, 6, {0x81, 0xf9, 0xff, 0xff, 0x03, 0x00}, {0x81, 0xf9, 0xff, 0xff, 0x0f, 0x00}, NULL},
    {"Antares.dll", 0x000a97dc, 5, {0x3d, 0x00, 0x00, 0x04, 0x00, 0x00}, {0x3d, 0x00, 0x00, 0x10, 0x00, 0x00}, NULL},
    {"Antares.dll", 0x000ab661, 6, {0x81, 0xf9, 0x00, 0x00, 0x04, 0x00}, {0x81, 0xf9, 0x00, 0x00, 0x10, 0x00}, NULL},
};

static uint32_t read_u32(uint32_t address) {
    return *(volatile uint32_t*)address;
}

static void ensure_log(void) {
    if (g_log != INVALID_HANDLE_VALUE) {
        return;
    }
    g_log = CreateFileA(
        "yr_map_512_plane_probe.csv",
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
}

static void log_text(const char* text) {
    if (g_log == INVALID_HANDLE_VALUE || !text) {
        return;
    }
    DWORD written = 0;
    WriteFile(g_log, text, (DWORD)lstrlenA(text), &written, NULL);
}

static void logf(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    _vsnprintf(buffer, sizeof(buffer) - 1, format, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = '\0';
    log_text(buffer);
}

static void flush_log(void) {
    if (g_log != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(g_log);
    }
}

static int bytes_match(const volatile uint8_t* actual, const uint8_t* expected, uint8_t size) {
    for (uint8_t i = 0; i < size; ++i) {
        if (actual[i] != expected[i]) {
            return 0;
        }
    }
    return 1;
}

static void copy_bytes(volatile uint8_t* destination, const uint8_t* source, uint8_t size) {
    for (uint8_t i = 0; i < size; ++i) {
        destination[i] = source[i];
    }
}

static int module_replacement_bytes(const ModuleOpcodePatch* patch,
                                    uint32_t address,
                                    uint8_t replacement[6]) {
    for (uint8_t i = 0; i < patch->size; ++i) {
        replacement[i] = patch->replacement[i];
    }
    if (!patch->redirect_target) {
        return 1;
    }
    if (patch->size < 5u) {
        return 0;
    }
    const intptr_t displacement =
        (const uint8_t*)patch->redirect_target - (const uint8_t*)(uintptr_t)(address + 5u);
    if (displacement < INT32_MIN || displacement > INT32_MAX) {
        return 0;
    }
    replacement[0] = 0xe9;
    const int32_t relative = (int32_t)displacement;
    replacement[1] = (uint8_t)(relative & 0xff);
    replacement[2] = (uint8_t)((relative >> 8) & 0xff);
    replacement[3] = (uint8_t)((relative >> 16) & 0xff);
    replacement[4] = (uint8_t)((relative >> 24) & 0xff);
    for (uint8_t i = 5u; i < patch->size; ++i) {
        replacement[i] = 0x90;
    }
    return 1;
}

static void check_host_profile(void) {
    if (g_host_checked) {
        return;
    }
    g_host_checked = 1;
    HMODULE module = GetModuleHandleA(NULL);
    if (!module) {
        return;
    }
    const uint8_t* base = (const uint8_t*)module;
    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        return;
    }
    const IMAGE_NT_HEADERS32* nt = (const IMAGE_NT_HEADERS32*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        return;
    }
    g_host_timestamp = nt->FileHeader.TimeDateStamp;
    g_host_entrypoint = nt->OptionalHeader.AddressOfEntryPoint;
    g_host_image_size = nt->OptionalHeader.SizeOfImage;
    g_host_supported =
        g_host_timestamp == EXPECTED_TIMESTAMP &&
        g_host_entrypoint == EXPECTED_ENTRYPOINT_RVA &&
        (g_host_image_size == STEAM_IMAGE_SIZE ||
         g_host_image_size == CNCNET_SPAWNER_IMAGE_SIZE);
}

static void rollback_patches(uint32_t count) {
    while (count > 0) {
        const Map512PatchEntry* patch = &g_map512_patch_table[--count];
        if (patch->address == ACTIVATION_PATCH_ADDRESS) {
            continue;
        }
        volatile uint8_t* code = (volatile uint8_t*)patch->address;
        DWORD old_protect = 0;
        if (VirtualProtect((LPVOID)code, patch->size, PAGE_EXECUTE_READWRITE, &old_protect)) {
            copy_bytes(code, patch->expected, patch->size);
            DWORD ignored = 0;
            VirtualProtect((LPVOID)code, patch->size, old_protect, &ignored);
        }
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0u);
}

static int write_code_bytes(uint32_t address, const uint8_t* bytes, uint8_t size) {
    volatile uint8_t* code = (volatile uint8_t*)address;
    DWORD old_protect = 0;
    if (!VirtualProtect((LPVOID)code, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return 0;
    }
    copy_bytes(code, bytes, size);
    DWORD ignored = 0;
    const int restored = VirtualProtect((LPVOID)code, size, old_protect, &ignored) != 0;
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)code, size);
    return restored;
}

static int set_reload_sensitive_iterators(int widened) {
    const Map512PatchEntry* selected[5] = {0};
    uint32_t selected_count = 0u;
    for (uint32_t i = 0u; i < MAP512_PATCH_COUNT; ++i) {
        const Map512PatchEntry* patch = &g_map512_patch_table[i];
        if (patch->address < 0x005782bdu || patch->address > 0x00578482u) {
            continue;
        }
        if (selected_count >= 5u) {
            return 0;
        }
        selected[selected_count++] = patch;
    }
    if (selected_count != 5u) {
        return 0;
    }
    for (uint32_t i = 0u; i < selected_count; ++i) {
        const Map512PatchEntry* patch = selected[i];
        const uint8_t* current = widened ? patch->expected : patch->replacement;
        if (!bytes_match((const volatile uint8_t*)patch->address,
                         current,
                         patch->size)) {
            logf("reload_iterator_transition_preflight_failed,%u,0x%08x,%u\n",
                 (unsigned)i,
                 (unsigned)patch->address,
                 (unsigned)widened);
            flush_log();
            return 0;
        }
    }
    for (uint32_t i = 0u; i < selected_count; ++i) {
        const Map512PatchEntry* patch = selected[i];
        const uint8_t* target = widened ? patch->replacement : patch->expected;
        if (!write_code_bytes(patch->address, target, patch->size)) {
            logf("reload_iterator_transition_write_failed,%u,0x%08x,%u\n",
                 (unsigned)i,
                 (unsigned)patch->address,
                 (unsigned)widened);
            while (i > 0u) {
                const Map512PatchEntry* rollback = selected[--i];
                const uint8_t* original_state =
                    widened ? rollback->expected : rollback->replacement;
                if (!write_code_bytes(rollback->address,
                                      original_state,
                                      rollback->size)) {
                    logf("reload_iterator_transition_rollback_failed,0x%08x,%u\n",
                         (unsigned)rollback->address,
                         (unsigned)widened);
                }
            }
            flush_log();
            return 0;
        }
    }
    logf("reload_iterator_transition,%s,%u\n",
         widened ? "widened" : "stock",
         (unsigned)selected_count);
    flush_log();
    return 1;
}

static void rollback_subzone_opcode_selected(const uint8_t* selected, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        if (selected[i]) {
            const SubzoneOpcodePatch* patch = &g_subzone_opcode_patch_table[i];
            write_code_bytes(patch->address, patch->expected, patch->size);
        }
    }
    g_subzone_opcode_patches = 0;
}

static void rollback_module_opcode_selected(const uint8_t* selected, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        if (selected[i]) {
            const ModuleOpcodePatch* patch = &g_module_opcode_patch_table[i];
            HMODULE module = GetModuleHandleA(patch->module_name);
            if (module) {
                write_code_bytes((uint32_t)(uintptr_t)module + patch->rva,
                                 patch->expected,
                                 patch->size);
            }
        }
    }
    g_extension_patches = 0;
}

/* WALL LINE-FILL BISECTION (2026-08-09): the 73 Antares.dll shl-9 patches
 * (size 3) are the prime suspect for breaking GuardRange wall line-fill:
 *   - broad build patches Antares fully  -> breaks walls (BUG-ATLAS 2.1);
 *   - his base patches NO Antares         -> clean walls;
 *   - we added 148 Antares patches        -> walls broke.
 * The shl half is NOT needed for the corner fix (that was the Antares cmp
 * patches), so removing it keeps corners while testing the wall hypothesis.
 * Set to 0 to restore them. Phobos shl (also size 3) is excluded by the 'A'. */
static int g_skip_antares_shl = 0;   /* RULED OUT as wall cause: with shl off,
    walls stayed broken AND shroud striped + movement/AIBaseSpacing broke, so the
    Antares shl are load-bearing (shroud/tactical/AI cell math) and NOT the wall
    line-fill FP. Kept ON. Wall line-fill is a separate, still-unpinned issue. */
static int module_patch_disabled(const ModuleOpcodePatch* p) {
    return g_skip_antares_shl && p->size == 3u && p->module_name[0] == 'A';
}

static int apply_map512_patches(void) {
    if (g_patch_status != 0) {
        return g_patch_status > 0;
    }
    const uint32_t subzone_patch_count =
        (uint32_t)(sizeof(g_subzone_opcode_patch_table) /
                   sizeof(g_subzone_opcode_patch_table[0]));
    uint8_t subzone_needs_patch[32] = {0};
    uint8_t subzone_applied[32] = {0};
    const uint32_t module_patch_count =
        (uint32_t)(sizeof(g_module_opcode_patch_table) /
                   sizeof(g_module_opcode_patch_table[0]));
    /* Sized to the module patch table (Phobos + Antares shl + Antares cmp).
       These are indexed by patch index, so they MUST be >= the number of
       ModuleOpcodePatch entries or the per-patch flag writes overflow the
       stack. Kept comfortably above the current count (~150). */
    uint8_t module_present[256] = {0};
    uint8_t module_applied[256] = {0};
    uint32_t subzone_prepatched_count = 0;
    for (uint32_t i = 0; i < subzone_patch_count; ++i) {
        const SubzoneOpcodePatch* patch = &g_subzone_opcode_patch_table[i];
        uint8_t replacement[5];
        for (uint8_t j = 0; j < patch->size; ++j) {
            replacement[j] = patch->expected[j];
        }
        replacement[1] = 0xb7;
        const int is_original = bytes_match((const volatile uint8_t*)patch->address,
                                            patch->expected,
                                            patch->size);
        const int is_replacement = bytes_match((const volatile uint8_t*)patch->address,
                                               replacement,
                                               patch->size);
        if (!is_original && !is_replacement) {
            const volatile uint8_t* actual = (const volatile uint8_t*)patch->address;
            logf("subzone_opcode_activation_mismatch,%u,0x%08x,%02x%02x%02x%02x%02x\n",
                 (unsigned)i,
                 (unsigned)patch->address,
                 (unsigned)actual[0],
                 (unsigned)actual[1],
                 (unsigned)actual[2],
                 (unsigned)actual[3],
                 (unsigned)actual[4]);
            g_patch_status = -1;
            return 0;
        }
        subzone_needs_patch[i] = is_original ? 1u : 0u;
        if (is_replacement) {
            ++subzone_prepatched_count;
        }
    }
    for (uint32_t i = 0; i < MAP512_PATCH_COUNT; ++i) {
        const Map512PatchEntry* patch = &g_map512_patch_table[i];
        if (patch->address == ACTIVATION_PATCH_ADDRESS) {
            continue;
        }
        if (!bytes_match((const volatile uint8_t*)patch->address, patch->expected, patch->size)) {
            logf("patch_preflight_mismatch,%u,0x%08x\n", (unsigned)i, (unsigned)patch->address);
            g_patch_status = -1;
            return 0;
        }
    }
    for (uint32_t i = 0; i < module_patch_count; ++i) {
        const ModuleOpcodePatch* patch = &g_module_opcode_patch_table[i];
        HMODULE module = GetModuleHandleA(patch->module_name);
        if (!module) {
            continue;
        }
        if (module_patch_disabled(patch)) {
            continue;   /* Antares shl bisection: leave module_present[i]=0 */
        }
        module_present[i] = 1u;
        const uint32_t address = (uint32_t)(uintptr_t)module + patch->rva;
        uint8_t replacement[6];
        if (!module_replacement_bytes(patch, address, replacement)) {
            logf("extension_patch_redirect_invalid,%u,%s,0x%08x\n",
                 (unsigned)i,
                 patch->module_name,
                 (unsigned)address);
            g_patch_status = -1;
            return 0;
        }
        if (!bytes_match((const volatile uint8_t*)address,
                         patch->expected,
                         patch->size)) {
            /* A module patch whose expected bytes no longer match (module was
               updated to a different version, or another DLL already patched
               the site) is SKIPPED, not fatal. Optional inline-stride patches
               on Antares/Phobos must never disable the whole plane-widening:
               a single stale entry used to set g_patch_status=-1 and abort ALL
               patches (gamemd core included), silently reverting the game to a
               512 no-op. Log it and move on so the matching sites still apply. */
            logf("extension_patch_preflight_mismatch,%u,%s,0x%08x\n",
                 (unsigned)i,
                 patch->module_name,
                 (unsigned)address);
            module_present[i] = 0u;
        }
    }
    uint32_t applied = 0;
    for (uint32_t i = 0; i < MAP512_PATCH_COUNT; ++i) {
        const Map512PatchEntry* patch = &g_map512_patch_table[i];
        if (patch->address == ACTIVATION_PATCH_ADDRESS) {
            continue;
        }
        volatile uint8_t* code = (volatile uint8_t*)patch->address;
        DWORD old_protect = 0;
        if (!VirtualProtect((LPVOID)code, patch->size, PAGE_EXECUTE_READWRITE, &old_protect)) {
            rollback_patches(i);
            logf("patch_protect_failed,%u,0x%08x\n", (unsigned)i, (unsigned)patch->address);
            g_patch_status = -1;
            return 0;
        }
        copy_bytes(code, patch->replacement, patch->size);
        DWORD ignored = 0;
        if (!VirtualProtect((LPVOID)code, patch->size, old_protect, &ignored)) {
            rollback_patches(i + 1u);
            logf("patch_restore_protect_failed,%u,0x%08x\n", (unsigned)i, (unsigned)patch->address);
            g_patch_status = -1;
            return 0;
        }
        ++applied;
    }
    uint32_t subzone_newly_patched_count = 0;
    for (uint32_t i = 0; i < subzone_patch_count; ++i) {
        if (subzone_needs_patch[i]) {
            const SubzoneOpcodePatch* patch = &g_subzone_opcode_patch_table[i];
            uint8_t replacement[5];
            for (uint8_t j = 0; j < patch->size; ++j) {
                replacement[j] = patch->expected[j];
            }
            replacement[1] = 0xb7;
            if (!write_code_bytes(patch->address, replacement, patch->size)) {
                rollback_subzone_opcode_selected(subzone_applied, subzone_patch_count);
                rollback_patches(MAP512_PATCH_COUNT);
                logf("subzone_opcode_patch_failed,%u,0x%08x\n",
                     (unsigned)i,
                     (unsigned)patch->address);
                g_patch_status = -1;
                return 0;
            }
            subzone_applied[i] = 1u;
            ++subzone_newly_patched_count;
        }
    }
    uint32_t extension_newly_patched_count = 0;
    for (uint32_t i = 0; i < module_patch_count; ++i) {
        if (!module_present[i]) {
            continue;
        }
        const ModuleOpcodePatch* patch = &g_module_opcode_patch_table[i];
        HMODULE module = GetModuleHandleA(patch->module_name);
        const uint32_t address = (uint32_t)(uintptr_t)module + patch->rva;
        uint8_t replacement[6];
        if (!module_replacement_bytes(patch, address, replacement) ||
            !write_code_bytes(address, replacement, patch->size)) {
            rollback_module_opcode_selected(module_applied, module_patch_count);
            rollback_subzone_opcode_selected(subzone_applied, subzone_patch_count);
            rollback_patches(MAP512_PATCH_COUNT);
            logf("extension_patch_failed,%u,%s,0x%08x\n",
                 (unsigned)i,
                 patch->module_name,
                 (unsigned)address);
            g_patch_status = -1;
            return 0;
        }
        module_applied[i] = 1u;
        ++extension_newly_patched_count;
    }
    g_extension_patches = extension_newly_patched_count;
    g_subzone_opcode_patches = subzone_patch_count;
    FlushInstructionCache(GetCurrentProcess(), NULL, 0u);
    for (uint32_t i = 0; i < MAP512_PATCH_COUNT; ++i) {
        const Map512PatchEntry* patch = &g_map512_patch_table[i];
        if (patch->address == ACTIVATION_PATCH_ADDRESS) {
            continue;
        }
        if (!bytes_match((const volatile uint8_t*)patch->address, patch->replacement, patch->size)) {
            rollback_module_opcode_selected(module_applied, module_patch_count);
            rollback_subzone_opcode_selected(subzone_applied, subzone_patch_count);
            rollback_patches(MAP512_PATCH_COUNT);
            logf("patch_postflight_mismatch,%u,0x%08x\n", (unsigned)i, (unsigned)patch->address);
            g_patch_status = -1;
            return 0;
        }
    }
    for (uint32_t i = 0; i < subzone_patch_count; ++i) {
        const SubzoneOpcodePatch* patch = &g_subzone_opcode_patch_table[i];
        uint8_t replacement[5];
        for (uint8_t j = 0; j < patch->size; ++j) {
            replacement[j] = patch->expected[j];
        }
        replacement[1] = 0xb7;
        if (!bytes_match((const volatile uint8_t*)patch->address,
                         replacement,
                         patch->size)) {
            rollback_module_opcode_selected(module_applied, module_patch_count);
            rollback_subzone_opcode_selected(subzone_applied, subzone_patch_count);
            rollback_patches(MAP512_PATCH_COUNT);
            logf("subzone_opcode_postflight_mismatch,%u,0x%08x\n",
                 (unsigned)i,
                 (unsigned)patch->address);
            g_patch_status = -1;
            return 0;
        }
    }
    for (uint32_t i = 0; i < module_patch_count; ++i) {
        if (!module_present[i]) {
            continue;
        }
        const ModuleOpcodePatch* patch = &g_module_opcode_patch_table[i];
        HMODULE module = GetModuleHandleA(patch->module_name);
        const uint32_t address = (uint32_t)(uintptr_t)module + patch->rva;
        uint8_t replacement[6];
        if (!module_replacement_bytes(patch, address, replacement)) {
            rollback_module_opcode_selected(module_applied, module_patch_count);
            rollback_subzone_opcode_selected(subzone_applied, subzone_patch_count);
            rollback_patches(MAP512_PATCH_COUNT);
            logf("extension_patch_postflight_redirect_invalid,%u,%s,0x%08x\n",
                 (unsigned)i,
                 patch->module_name,
                 (unsigned)address);
            g_patch_status = -1;
            return 0;
        }
        if (!bytes_match((const volatile uint8_t*)address,
                         replacement,
                         patch->size)) {
            rollback_module_opcode_selected(module_applied, module_patch_count);
            rollback_subzone_opcode_selected(subzone_applied, subzone_patch_count);
            rollback_patches(MAP512_PATCH_COUNT);
            logf("extension_patch_postflight_mismatch,%u,%s,0x%08x\n",
                 (unsigned)i,
                 patch->module_name,
                 (unsigned)address);
            g_patch_status = -1;
            return 0;
        }
    }
    g_patch_status = 1;
    logf("patch_applied,%u,hooked_activation,0x%08x,subzone_opcode_patches,%u,new,%u,prepatched,%u,extension_patches,%u\n",
         (unsigned)applied,
         (unsigned)ACTIVATION_PATCH_ADDRESS,
         (unsigned)subzone_patch_count,
         (unsigned)subzone_newly_patched_count,
         (unsigned)subzone_prepatched_count,
         (unsigned)extension_newly_patched_count);
    flush_log();
    return 1;
}

static void log_process_memory(uint32_t frame) {
    PROCESS_MEMORY_COUNTERS counters;
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        logf("process_memory,%u,%u,%u,%u\n",
             (unsigned)frame,
             (unsigned)counters.WorkingSetSize,
             (unsigned)counters.PagefileUsage,
             (unsigned)counters.PeakPagefileUsage);
    }
}

static void log_radar_state(const char* event) {
    logf("%s,0x%08x,0x%08x,0x%08x,0x%08x,%u,%u,%u,%u\n",
         event,
         (unsigned)read_u32(MAP_INSTANCE_ADDRESS + 0x121c),
         (unsigned)read_u32(MAP_INSTANCE_ADDRESS + 0x1220),
         (unsigned)read_u32(MAP_INSTANCE_ADDRESS + 0x123c),
         (unsigned)read_u32(MAP_INSTANCE_ADDRESS + 0x1274),
         (unsigned)read_u32(MAP_INSTANCE_ADDRESS + 0x149c),
         (unsigned)read_u32(MAP_INSTANCE_ADDRESS + 0x14a0),
         (unsigned)read_u32(MAP_INSTANCE_ADDRESS + 0x14a4),
         (unsigned)read_u32(MAP_INSTANCE_ADDRESS + 0x14a8));
    log_process_memory(read_u32(GAME_FRAME_ADDRESS));
    flush_log();
}

static uint32_t read_cell_slot_bounded(uint32_t items,
                                       uint32_t capacity,
                                       uint32_t x,
                                       uint32_t y) {
    const uint64_t index = (uint64_t)y * CELL_AXIS_LIMIT + x;
    if (items < 0x10000u || index >= capacity) {
        return 0u;
    }
    return read_u32(items + (uint32_t)(index * 4u));
}

static void log_periodic_state(uint32_t frame) {
    if (frame == g_last_frame) {
        return;
    }
    g_last_frame = frame;
    if (frame >= 5u && frame % 60u != 0u) {
        return;
    }
    logf("map_state,%u,0x%08x,%u,%u,%u,%u,%d,%d,%d,%d,%d,%u,%u,%u,%u,%u,%u,%u\n",
         (unsigned)frame,
         (unsigned)read_u32(MAP_INSTANCE_ADDRESS + MAP_CELLS_ITEMS_OFFSET),
         (unsigned)read_u32(MAP_INSTANCE_ADDRESS + MAP_CELLS_CAPACITY_OFFSET),
         (unsigned)read_u32(MAP_INSTANCE_ADDRESS + MAP_MAX_WIDTH_OFFSET),
         (unsigned)read_u32(MAP_INSTANCE_ADDRESS + MAP_MAX_HEIGHT_OFFSET),
         (unsigned)read_u32(MAP_INSTANCE_ADDRESS + MAP_MAX_CELLS_OFFSET),
         (int32_t)read_u32(MAP_INSTANCE_ADDRESS + 0x0f4),
         (int32_t)read_u32(MAP_INSTANCE_ADDRESS + 0x0fc),
         (int32_t)read_u32(MAP_INSTANCE_ADDRESS + 0x100),
         (int32_t)read_u32(MAP_INSTANCE_ADDRESS + 0x104),
         (int32_t)read_u32(MAP_INSTANCE_ADDRESS + 0x108),
         (unsigned)g_getcell_calls,
         (unsigned)g_axis_invalid,
         (unsigned)g_flat_invalid,
         (unsigned)g_axis_row_carry,
         (unsigned)g_invalid_samples,
         (unsigned)g_max_x,
         (unsigned)g_max_y);
    logf("radar_surface_state,%u,0x%08x,0x%08x,0x%08x,0x%08x\n",
         (unsigned)frame,
         (unsigned)read_u32(MAP_INSTANCE_ADDRESS + 0x121cu),
         (unsigned)read_u32(MAP_INSTANCE_ADDRESS + 0x1220u),
         (unsigned)read_u32(0x00880a04u),
         (unsigned)read_u32(0x00880a08u));
    log_process_memory(frame);
    if (frame == 1u) {
        const uint32_t items = read_u32(MAP_INSTANCE_ADDRESS + MAP_CELLS_ITEMS_OFFSET);
        const uint32_t capacity =
            read_u32(MAP_INSTANCE_ADDRESS + MAP_CELLS_CAPACITY_OFFSET);
        if (items >= 0x10000u) {
            for (uint32_t x = 527u; x <= 532u; ++x) {
                const uint32_t cell =
                    read_cell_slot_bounded(items, capacity, x, 512u);
                if (cell >= 0x10000u) {
                    logf("cell_passability_sample,%u,%u,%u,0x%08x,%u,%u,%u,%d,%d,%u,0x%08x\n",
                         (unsigned)frame,
                         (unsigned)x,
                         512u,
                         (unsigned)cell,
                         (unsigned)read_u32(cell + 0x38u),
                         (unsigned)read_u32(cell + 0x4cu),
                         (unsigned)read_u32(cell + 0xecu),
                         (int)*(volatile int8_t*)(cell + 0x11bu),
                         (int32_t)read_u32(cell + 0x44u),
                         (unsigned)*(volatile uint8_t*)(cell + 0x11eu),
                         (unsigned)read_u32(cell + 0x124u));
                }
            }
            for (uint32_t y = 527u; y <= 532u; ++y) {
                const uint32_t cell =
                    read_cell_slot_bounded(items, capacity, 512u, y);
                if (cell >= 0x10000u) {
                    logf("cell_passability_transpose,%u,%u,%u,0x%08x,%u,%u,%u,%d,%d,%u,0x%08x\n",
                         (unsigned)frame, 512u, (unsigned)y,
                         (unsigned)cell,
                         (unsigned)read_u32(cell + 0x38u),
                         (unsigned)read_u32(cell + 0x4cu),
                         (unsigned)read_u32(cell + 0xecu),
                         (int)*(volatile int8_t*)(cell + 0x11bu),
                         (int32_t)read_u32(cell + 0x44u),
                         (unsigned)*(volatile uint8_t*)(cell + 0x11eu),
                         (unsigned)read_u32(cell + 0x124u));
                }
            }
            static const uint16_t wall_cross_candidates[][2] = {
                {511, 499}, {512, 499}, {513, 499},
                {543, 499}, {544, 499}, {545, 499},
                {499, 511}, {499, 512}, {499, 513},
                {512, 400}, {400, 512}, {512, 600}, {600, 512}
            };
            for (uint32_t i = 0u; i < sizeof(wall_cross_candidates) / sizeof(wall_cross_candidates[0]); ++i) {
                const uint32_t x = wall_cross_candidates[i][0];
                const uint32_t y = wall_cross_candidates[i][1];
                const uint32_t cell =
                    read_cell_slot_bounded(items, capacity, x, y);
                if (cell >= 0x10000u) {
                    logf("wall_cross_cell_sample,%u,%u,%u,0x%08x,%u,%u,%u,%d,%d,%u,0x%08x\n",
                         (unsigned)frame, (unsigned)x, (unsigned)y,
                         (unsigned)cell,
                         (unsigned)read_u32(cell + 0x38u),
                         (unsigned)read_u32(cell + 0x4cu),
                         (unsigned)read_u32(cell + 0xecu),
                         (int)*(volatile int8_t*)(cell + 0x11bu),
                         (int32_t)read_u32(cell + 0x44u),
                         (unsigned)*(volatile uint8_t*)(cell + 0x11eu),
                         (unsigned)read_u32(cell + 0x124u));
                }
            }
            static const uint16_t stock_water_candidates[][2] = {
                {217, 41}, {218, 41}, {220, 41}, {216, 41}, {226, 41},
                {202, 72}, {231, 72}, {190, 102}, {347, 149}
            };
            for (uint32_t i = 0u; i < sizeof(stock_water_candidates) / sizeof(stock_water_candidates[0]); ++i) {
                const uint32_t x = stock_water_candidates[i][0];
                const uint32_t y = stock_water_candidates[i][1];
                const uint32_t cell =
                    read_cell_slot_bounded(items, capacity, x, y);
                if (cell >= 0x10000u) {
                    logf("stock_cell_passability_sample,%u,%u,%u,0x%08x,%u,%u,%u,%d\n",
                         (unsigned)frame, (unsigned)x, (unsigned)y,
                         (unsigned)cell,
                         (unsigned)read_u32(cell + 0x38u),
                         (unsigned)read_u32(cell + 0x4cu),
                         (unsigned)read_u32(cell + 0xecu),
                         (int)*(volatile int8_t*)(cell + 0x11bu));
                }
            }
        }
    }
    flush_log();
}

extern "C" __declspec(dllexport) HRESULT __cdecl SyringeHandshake(SyringeHandshakeInfo* info) {
    if (!info || info->cb_size != (int32_t)sizeof(SyringeHandshakeInfo)) {
        return E_INVALIDARG;
    }
    const int steam =
        info->exe_filesize == STEAM_FILE_SIZE &&
        info->exe_timestamp == EXPECTED_TIMESTAMP &&
        info->exe_crc == STEAM_SYRINGE_CRC;
    const int cncnet_spawner =
        info->exe_filesize == CNCNET_SPAWNER_FILE_SIZE &&
        info->exe_timestamp == EXPECTED_TIMESTAMP &&
        (info->exe_crc == CNCNET_SPAWNER_SYRINGE_CRC ||
         info->exe_crc == TESTER_CNCNET_SYRINGE_CRC);
    const int accepted = steam || cncnet_spawner;
    if (info->message && info->message_capacity > 0) {
        lstrcpynA(info->message,
                  accepted ? "YR 1.001 map-512 plane probe: host accepted"
                           : "YR 1.001 map-512 plane probe: host rejected",
                  info->message_capacity);
    }
    return accepted ? S_OK : S_FALSE;
}

extern "C" __declspec(dllexport) DWORD __cdecl Map512PlaneActivate(void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    if (!r) {
        return 0;
    }
    if (g_host_supported && g_patch_status == 0 && !apply_map512_patches()) {
        g_host_supported = 0;
        flush_log();
    }
    /* Replay the hooked mov eax,512, widened only after complete activation. */
    r->eax = g_host_supported && g_patch_status > 0 ? 1024u : 512u;
    return 0;
}

/* MapClass cell-construction loop @0x5663BC reads plane[edi] and treats any
 * non-zero slot as an existing cell. On a fresh 1024 plane the diamond corners
 * beyond the ~250x250 init hold garbage (-1), so it constructs onto a wild
 * pointer -> AV 0x410174 (the 300x300 fatal). Replay the stolen
 *   mov ecx,[edx+edi*4]; test ecx,ecx
 * but neutralise any slot that is not a plausible heap cell pointer to 0, so
 * the loop allocates a fresh cell instead. */
extern "C" __declspec(dllexport) DWORD __cdecl Map512CellSlotGuard(void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    if (!r) {
        return 0;
    }
    uint32_t* slot = (uint32_t*)(r->edx + r->edi * 4u);
    uint32_t val = *slot;
    if (val != 0u && (val < 0x00400000u || val >= 0x40000000u)) {
        val = 0u;
        *slot = 0u;
    }
    r->ecx = val;
    if (val == 0u) {
        r->flags |= 0x40u;    /* ZF=1 -> jne not taken -> allocate fresh cell */
    } else {
        r->flags &= ~0x40u;   /* ZF=0 -> jne taken -> construct existing cell */
    }
    return 0;
}

/* Full-map cell iterator @0x578290 returns *(map->0x118) as the next cell and
 * steps by -0x7FC. On a 1024 plane it can step past the valid diamond into
 * garbage slots -> caller derefs a wild cell (300x300 crash at 0x568C3B). Guard
 * it with a cell-identity check: a real cell at slot N has MapCoords
 * (X@+0x24,Y@+0x26) == (N%1024, N/1024); on mismatch return a null cell so the
 * caller's iteration loop ends cleanly. Replays the stolen mov eax,[ecx+0x114]. */
extern "C" __declspec(dllexport) DWORD __cdecl Map512CellIteratorGuard(void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    if (!r) {
        return 0;
    }
    uint32_t map = r->ecx;
    uint32_t items = *(volatile uint32_t*)(map + 0x13cu);
    uint32_t cur   = *(volatile uint32_t*)(map + 0x118u);
    int stop = 0;
    if (items) {
        if (cur < items || cur >= items + CELL_COUNT_LIMIT * 4u) {
            stop = 1;
        } else {
            uint32_t cell = *(volatile uint32_t*)cur;
            /* Stop only on a wild cell pointer outside the heap range. A coord-
             * identity check was tried and reverted: it over-stops during the
             * load-time passability passes (valid cells mid-init) -> incomplete
             * recompute -> subzone recursion -> stack overflow (ntdll). */
            if (cell != 0u && (cell < 0x00400000u || cell >= 0x40000000u)) {
                stop = 1;
            }
        }
    }
    if (stop) {
        r->eax = 0u;
        return 0x005782d4u;   /* bare ret -> caller sees null -> loop ends */
    }
    r->eax = *(volatile uint32_t*)(map + 0x114u);   /* replay mov eax,[ecx+0x114] */
    return 0x00578296u;
}

/* Coord-transform @0x660540 virtual-calls the ds:0x880A04 singleton, whose
 * vtable is garbage at 1024 -> heap fatal on edge-cell orders (bottom-left).
 * Our broad build skips it (result only feeds sync-checksum logging); paired
 * with the 8 iterator shl-9 sites (0x5780b4 ...) that keep the routing coord
 * correct, so skipping does NOT wrap the destination. Returns bare ret 0x66053A. */
extern "C" __declspec(dllexport) DWORD __cdecl Map512CoordTransformGuard(void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    if (!r) {
        return 0;
    }
    r->eax = 0u;
    return 0x0066053au;
}

/* WALL-DRAW probe @0x47f96a (stolen: mov cl,[ebx+0x2a8], 6 bytes). ESI=cell,
 * EBX=OverlayTypeClass. Logs each drawn wall's stored connection frame
 * (cell+0x11E). If a wall next to another wall draws frame 0, the connection
 * PRODUCER failed to see the neighbour at 1024 (BUG-ATLAS 2.1). Capped. */
extern "C" __declspec(dllexport) DWORD __cdecl Map512WallDrawProbe(void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    static int fires = 0;
    if (r && g_patch_status > 0 && fires < 250) {
        uint32_t cell = r->esi;
        uint32_t otype = r->ebx;
        int X = (int)*(volatile int16_t*)(cell + 0x24u);
        int Y = (int)*(volatile int16_t*)(cell + 0x26u);
        int ovl = *(volatile int32_t*)(cell + 0x44u);
        if (ovl >= 0) {
            int fr = *(volatile uint8_t*)(cell + 0x11eu);
            int flag = *(volatile uint8_t*)(otype + 0x2a8u);
            ++fires;
            logf("WALLDRAW,x,%d,y,%d,overlay,%d,frame,0x%X,wallflag,%d\n",
                 X, Y, ovl, fr, flag);
        }
    }
    return 0;
}

/* SHARED CELL-LOOKUP caller probe @0x5657bb (stolen: mov ecx,[ecx+0x13c], 6
 * bytes). At this point [ESP]=caller return addr, EDX=CellStruct*{X@0,Y@2},
 * EAX=computed cell index (already 1024-correct). Dedups by caller so it logs
 * each distinct calling function ONCE (no flood) -> reveals which functions do
 * cell lookups. Do a drag-select, then build infantry: the NEW caller
 * addresses appearing (in order) localize the drag-select / unit-exit fns so
 * their inline cell math can be inspected. Capped at 96 distinct callers. */
static uint32_t g_probe_seen_callers[96];
static int g_probe_seen_n = 0;
extern "C" __declspec(dllexport) DWORD __cdecl Map512CellLookupCallerProbe(void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    if (r && g_patch_status > 0 && g_probe_seen_n < 96) {
        uint32_t caller = *(volatile uint32_t*)(r->esp);
        int seen = 0;
        for (int i = 0; i < g_probe_seen_n; ++i) {
            if (g_probe_seen_callers[i] == caller) { seen = 1; break; }
        }
        if (!seen) {
            uint32_t cs = r->edx;
            int X = (int)*(volatile int16_t*)(cs + 0u);
            int Y = (int)*(volatile int16_t*)(cs + 2u);
            g_probe_seen_callers[g_probe_seen_n++] = caller;
            logf("CELLCALLER,0x%08X,x,%d,y,%d,idx,%u\n",
                 caller, X, Y, (unsigned)r->eax);
        }
    }
    return 0;
}

/* CELL-MISS probe @0x56577a (stolen: mov [esp+0xc],si, 5 bytes) -- GetCellAt's
 * DUMMY-return path (index negative or >= capacity -> returns off-map cell
 * 0xABDC50). ESI=requested cell X, EAX=requested cell Y, [ESP+8]=caller. This
 * is the direct FAILURE signature: a function asking for a cell it can't get.
 * If drag-select / infantry-exit compute a wrong-stride coord, they land here.
 * Dedups by caller. Capped at 96. */
static uint32_t g_probe_miss_callers[96];
static int g_probe_miss_n = 0;
extern "C" __declspec(dllexport) DWORD __cdecl Map512CellMissProbe(void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    if (r && g_patch_status > 0 && g_probe_miss_n < 96) {
        uint32_t caller = *(volatile uint32_t*)(r->esp + 8u);
        int seen = 0;
        for (int i = 0; i < g_probe_miss_n; ++i) {
            if (g_probe_miss_callers[i] == caller) { seen = 1; break; }
        }
        if (!seen) {
            int X = (int)(int16_t)(r->esi & 0xffffu);
            int Y = (int)(int16_t)(r->eax & 0xffffu);
            g_probe_miss_callers[g_probe_miss_n++] = caller;
            logf("CELLMISS,0x%08X,x,%d,y,%d\n", caller, X, Y);
        }
    }
    return 0;
}

/* KICKOUT probes -- BuildingClass::KickOutUnit (0x443c60): places a produced
 * unit / free unit out of a building. ENTER (sub esp,0x130): ecx=building,
 * [esp+4]=object being kicked out. FAIL (0x445696): the fn returns 0/false
 * (did not kick out). If ENTER fires after building a unit / NAREFN but FAIL
 * follows, placement is failing (free HARV missing / clog). If ENTER succeeds
 * (no FAIL) yet units still idle on the exit cell, the fault is post-placement
 * (scatter/mission), and we probe that next. Capped. */
static int g_kickout_enter = 0;
static int g_kickout_fail = 0;
extern "C" __declspec(dllexport) DWORD __cdecl Map512KickoutEnterProbe(void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    if (r && g_patch_status > 0 && g_kickout_enter < 400) {
        uint32_t obj = *(volatile uint32_t*)(r->esp + 4u);
        ++g_kickout_enter;
        logf("KICKOUT_ENTER,%d,building,0x%08X,obj,0x%08X\n",
             g_kickout_enter, (unsigned)r->ecx, (unsigned)obj);
    }
    return 0;
}
extern "C" __declspec(dllexport) DWORD __cdecl Map512KickoutFailProbe(void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    if (r && g_patch_status > 0 && g_kickout_fail < 400) {
        ++g_kickout_fail;
        logf("KICKOUT_FAIL,%d\n", g_kickout_fail);
    }
    return 0;
}

/* SCATTER probe -- InfantryClass::Scatter (0x51d0dd), esi=the infantry. If a
 * unit that KICKOUT re-kicks repeatedly (stuck on exit) ALSO shows up here, it
 * is trying to scatter off the exit cell and failing (free-cell scan); if it
 * NEVER appears, no scatter/move order is being issued at all (fault is in the
 * factory release / mission assignment). Dedup by unit ptr, capped. */
static uint32_t g_scatter_seen[96];
static int g_scatter_n = 0;
extern "C" __declspec(dllexport) DWORD __cdecl Map512ScatterProbe(void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    if (r && g_patch_status > 0 && g_scatter_n < 96) {
        uint32_t unit = r->esi;
        int seen = 0;
        for (int i = 0; i < g_scatter_n; ++i) {
            if (g_scatter_seen[i] == unit) { seen = 1; break; }
        }
        if (!seen) {
            g_scatter_seen[g_scatter_n++] = unit;
            logf("SCATTER,unit,0x%08X\n", (unsigned)unit);
        }
    }
    return 0;
}


extern "C" __declspec(dllexport) DWORD __cdecl Map512BoundaryProbe(void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    if (!r || r->edx < 0x10000u) {
        return 0;
    }
    const int16_t x = *(volatile int16_t*)r->edx;
    const int16_t y = *(volatile int16_t*)(r->edx + 2u);
    /* Replay the two instructions overwritten by the Syringe hook. */
    r->eax = (uint32_t)(int32_t)y;
    r->esi = (uint32_t)(int32_t)x;
    if (!g_host_supported) {
        return 0;
    }
    if (g_patch_status <= 0) {
        return 0;
    }
    const int32_t flat = ((int32_t)y * CELL_AXIS_LIMIT) + (int32_t)x;
    const int axis_invalid =
        x < 0 || x >= CELL_AXIS_LIMIT || y < 0 || y >= CELL_AXIS_LIMIT;
    const int flat_invalid = flat < 0 || flat >= CELL_COUNT_LIMIT;
    ++g_getcell_calls;
    if (g_getcell_calls == 1u) {
        log_radar_state("first_getcell");
    }
    if (axis_invalid) {
        ++g_axis_invalid;
    }
    if (flat_invalid) {
        ++g_flat_invalid;
    }
    if (axis_invalid && !flat_invalid) {
        ++g_axis_row_carry;
    }
    if (x >= 0 && (uint32_t)x > g_max_x) {
        g_max_x = (uint32_t)x;
    }
    if (y >= 0 && (uint32_t)y > g_max_y) {
        g_max_y = (uint32_t)y;
    }
    if (axis_invalid && g_invalid_samples < 64u) {
        ++g_invalid_samples;
        logf("getcell_axis_reject,%u,0x%08x,%d,%d,%d,%s,%u\n",
             (unsigned)read_u32(GAME_FRAME_ADDRESS),
             (unsigned)read_u32(r->esp + 4u),
             (int)x,
             (int)y,
             (int)flat,
             !flat_invalid ? "row_carry" : "flat_reject",
             (unsigned)g_invalid_samples);
    }
    log_periodic_state(read_u32(GAME_FRAME_ADDRESS));
    if (g_getcell_calls == 1000u || g_getcell_calls == 100000u ||
        g_getcell_calls == 500000u || g_getcell_calls == 1000000u) {
        log_radar_state("getcell_milestone");
    }
    if (axis_invalid) {
        /* Force the existing flattened-cap branch to reject without row carry. */
        r->eax = CELL_AXIS_LIMIT;
        r->esi = 0u;
    }
    return 0;
}

extern "C" __declspec(dllexport) DWORD __cdecl Map512SubzoneIdCeilingGuard(
    void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    if (!r || r->esp < 0x10000u) {
        return 0;
    }

    /* Replay the overwritten mov cx,[esp+0x10] even if activation failed. */
    const uint16_t original_cx = *(volatile uint16_t*)(r->esp + 0x10u);
    r->ecx = (r->ecx & 0xffff0000u) | original_cx;
    if (!g_host_supported || g_patch_status <= 0 ||
        r->edi < 0x10000u || r->eax > 2u) {
        return 0;
    }

    const uint32_t next_id = read_u32(r->esp + 0x14u);
    if (next_id > g_subzone_max_id) {
        g_subzone_max_id = next_id;
    }
    if (next_id >= SUBZONE_ID_RESERVED) {
        *(volatile uint16_t*)(r->edi + r->eax * 2u) = SUBZONE_ID_SATURATED;
        ++g_subzone_ceiling_entries;
        if (g_subzone_ceiling_entries <= 16u ||
            g_subzone_ceiling_entries % 10000u == 0u) {
            logf("subzone_id_saturated,%u,%u,0x%08x,%u\n",
                 (unsigned)next_id,
                 (unsigned)g_subzone_ceiling_entries,
                 (unsigned)r->edi,
                 (unsigned)r->eax);
            flush_log();
        }
        return RECALCULATE_SUBZONES_OVERFLOW_CONTINUE;
    }
    return 0;
}

extern "C" __declspec(dllexport) DWORD __cdecl Map512RegularPathExitProbe(
    void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    if (!r || r->esp < 0x10000u || r->esi < 0x10000u) {
        return 0;
    }

    /* Replay the two instructions overwritten at the common regular exit. */
    r->ebx = read_u32(r->esp + 0x68u);
    r->eax = read_u32(r->esi + 0x3cu);
    if (!g_host_supported || g_patch_status <= 0) {
        return 0;
    }

    const uint32_t iterations = read_u32(r->esp + 0x28u);
    ++g_regular_path_exits;
    if (iterations == 10000u) {
        ++g_regular_path_cap_hits;
        logf("regular_path_cap,%u,%u,%u\n",
             (unsigned)read_u32(GAME_FRAME_ADDRESS),
             (unsigned)g_regular_path_cap_hits,
             (unsigned)g_regular_path_exits);
        flush_log();
    }
    return 0;
}

extern "C" __declspec(dllexport) DWORD __cdecl Map512HierarchicalFailureProbe(
    void* registers) {
    (void)registers;
    if (!g_host_supported || g_patch_status <= 0) {
        return 0;
    }
    ++g_hierarchical_failures;
    logf("hierarchical_failure,%u,%u\n",
         (unsigned)read_u32(GAME_FRAME_ADDRESS),
         (unsigned)g_hierarchical_failures);
    flush_log();
    return 0;
}

extern "C" __declspec(dllexport) DWORD __cdecl Map512RegularFailureProbe(
    void* registers) {
    (void)registers;
    if (!g_host_supported || g_patch_status <= 0) {
        return 0;
    }
    ++g_regular_failures;
    logf("regular_failure,%u,%u\n",
         (unsigned)read_u32(GAME_FRAME_ADDRESS),
         (unsigned)g_regular_failures);
    flush_log();
    return 0;
}

extern "C" __declspec(dllexport) DWORD __cdecl Map512OverlayReadSuccessProbe(
    void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    logf("overlay_uublock_result,success,%u\n", (unsigned)r->esi);
    r->ecx = read_u32(r->esp + 0x48u);
    r->eax = read_u32(r->esp + 0x3cu);
    flush_log();
    return 0;
}

extern "C" __declspec(dllexport) DWORD __cdecl Map512OverlayReadFailureProbe(
    void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    logf("overlay_uublock_result,failure,%u\n", (unsigned)r->esi);
    r->eax = read_u32(r->esp + 0x48u);
    r->edx = read_u32(r->esp + 0x3cu);
    flush_log();
    return 0;
}

extern "C" __declspec(dllexport) DWORD __cdecl Map512OverlayDataReadSuccessProbe(
    void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    logf("overlay_data_uublock_result,success,%u\n", (unsigned)r->esi);
    r->eax = read_u32(r->esp + 0x48u);
    r->edx = read_u32(r->esp + 0x3cu);
    flush_log();
    return 0;
}

extern "C" __declspec(dllexport) DWORD __cdecl Map512OverlayDataReadFailureProbe(
    void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    logf("overlay_data_uublock_result,failure,%u\n", (unsigned)r->esi);
    r->eax = read_u32(r->esp + 0x48u);
    r->ecx = r->esp + 0x50u;
    flush_log();
    return 0;
}

extern "C" __declspec(dllexport) DWORD __cdecl Map512LoadBeforePostLoadReinit(
    void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    if (!r) {
        return 0;
    }
    if (!set_reload_sensitive_iterators(0)) {
        logf("reload_iterator_transition_fatal,stock\n");
        flush_log();
        TerminateProcess(GetCurrentProcess(), 0x512u);
        return 0;
    }
    /* Replay mov eax,[0xA8B230] inside sub_685120. */
    r->eax = read_u32(0x00a8b230u);
    log_radar_state("load_before_post_load_reinit");
    return 0;
}

extern "C" __declspec(dllexport) DWORD __cdecl Map512LoadAfterTabInit(
    void* registers) {
    SyringeRegisters* r = (SyringeRegisters*)registers;
    if (!r) {
        return 0;
    }
    const uint32_t capacity =
        read_u32(MAP_INSTANCE_ADDRESS + MAP_CELLS_CAPACITY_OFFSET);
    if (capacity == CELL_COUNT_LIMIT) {
        if (!set_reload_sensitive_iterators(1)) {
            logf("reload_iterator_transition_fatal,widened\n");
            flush_log();
            TerminateProcess(GetCurrentProcess(), 0x512u);
            return 0;
        }
    } else if (capacity == 0x40000u) {
        logf("reload_iterator_transition,legacy_stock_retained,%u\n",
             (unsigned)capacity);
        flush_log();
    } else {
        logf("reload_iterator_transition_fatal,unsupported_capacity,%u\n",
             (unsigned)capacity);
        flush_log();
        TerminateProcess(GetCurrentProcess(), 0x512u);
        return 0;
    }
    /* Replay mov ebx,1 after TabClass::Init_IO. */
    r->ebx = 1u;
    log_radar_state("load_after_tab_init");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        g_probe_base = (uint32_t)(uintptr_t)instance;
        ensure_log();
        check_host_profile();
        logf("dll_attach,%s,0x%08x,0x%08x,0x%08x,%d,probe=0x%08x,ares=0x%08x,phobos=0x%08x,spawner=0x%08x\n",
             g_host_supported ? "supported" : "rejected",
             (unsigned)g_host_timestamp,
             (unsigned)g_host_entrypoint,
             (unsigned)g_host_image_size,
             g_patch_status,
             (unsigned)g_probe_base,
             (unsigned)(uintptr_t)GetModuleHandleA("Ares.dll"),
             (unsigned)(uintptr_t)GetModuleHandleA("Phobos.dll"),
             (unsigned)(uintptr_t)GetModuleHandleA("CnCNet-Spawner.dll"));
        flush_log();
    } else if (reason == DLL_PROCESS_DETACH && g_log != INVALID_HANDLE_VALUE) {
        logf("dll_detach,%u,%u,%u,%u,%u,%u,%u,%d,subzone,%u,%u,%u,extension,%u,path,%u,%u,%u,%u\n",
             (unsigned)g_getcell_calls,
             (unsigned)g_axis_invalid,
             (unsigned)g_flat_invalid,
             (unsigned)g_axis_row_carry,
             (unsigned)g_invalid_samples,
             (unsigned)g_max_x,
             (unsigned)g_max_y,
             g_patch_status,
             (unsigned)g_subzone_max_id,
             (unsigned)g_subzone_ceiling_entries,
             (unsigned)g_subzone_opcode_patches,
             (unsigned)g_extension_patches,
             (unsigned)g_regular_path_exits,
             (unsigned)g_regular_path_cap_hits,
             (unsigned)g_hierarchical_failures,
             (unsigned)g_regular_failures);
        flush_log();
        CloseHandle(g_log);
        g_log = INVALID_HANDLE_VALUE;
    }
    return TRUE;
}
