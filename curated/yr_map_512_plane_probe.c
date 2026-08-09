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
    uint8_t module_present[16] = {0};
    uint8_t module_applied[16] = {0};
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
            logf("extension_patch_preflight_mismatch,%u,%s,0x%08x\n",
                 (unsigned)i,
                 patch->module_name,
                 (unsigned)address);
            g_patch_status = -1;
            return 0;
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
