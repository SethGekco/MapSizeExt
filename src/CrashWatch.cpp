#include "MapSizeExt.h"
#include <windows.h>
#include <tlhelp32.h>

// ============================================================
//  CrashWatch -- a first-chance exception watcher.
//
//  Why this exists: the reports we get from other people's installs are
//  "it crashes when a unit reaches the edge of the map" with no files, or
//  an Ares except.txt whose stack dump has to be decoded by hand before it
//  says anything. Everything useful in that decode is mechanical -- which
//  module owns EIP, which return addresses on the stack belong to which DLL,
//  and whether the faulting address is the cell array being indexed past its
//  end -- so do it in-process, at the moment of the fault, and write it next
//  to the game as MapSizeExt_crash.log.
//
//  This NEVER handles the exception. It always returns
//  EXCEPTION_CONTINUE_SEARCH, so Ares/Antares still produce their own
//  except.txt and the game dies exactly as it would have. It is a tap on the
//  wire, not a handler.
//
//  Constraints it respects, because it runs on a faulting thread:
//    * no CRT -- CreateFile/WriteFile + wsprintfA (user32), so it cannot
//      deadlock on a CRT lock that the faulting code was already holding;
//    * no loader calls -- the module table is captured ahead of time, at
//      init and again once the game reaches WinMain, and only READ here;
//    * every dereference goes through SafeRead, so a bad pointer in the
//      crash context cannot turn the watcher itself into a second fault;
//    * capped, with a reentrancy guard.
// ============================================================

namespace {

struct ModInfo { DWORD base; DWORD size; char name[48]; };

ModInfo  g_Mods[160];
volatile LONG g_ModCount = 0;
volatile LONG g_Busy     = 0;
volatile LONG g_Logged   = 0;
const    LONG kMaxLogged = 6;

// Read `n` bytes from `src` without trusting it. Returns false on a bad
// pointer instead of faulting.
bool SafeRead(const void* src, void* dst, SIZE_T n)
{
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), src, dst, n, &got) && got == n;
}

bool SafeDword(DWORD va, DWORD* out)
{
    return SafeRead(reinterpret_cast<const void*>(va), out, sizeof(DWORD));
}

// base+offset attribution for an address, using the pre-captured table.
// Returns nullptr when the address belongs to no known module.
const ModInfo* ModuleOf(DWORD addr, DWORD* offset)
{
    const LONG n = g_ModCount;
    for (LONG i = 0; i < n; ++i)
    {
        if (addr >= g_Mods[i].base && addr < g_Mods[i].base + g_Mods[i].size)
        {
            if (offset) *offset = addr - g_Mods[i].base;
            return &g_Mods[i];
        }
    }
    return nullptr;
}

// --- tiny no-CRT output sink -------------------------------------------
struct Sink
{
    HANDLE h;
    char   buf[2048];

    void Open()
    {
        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        char* slash = nullptr;
        for (char* p = path; *p; ++p) if (*p == '\\') slash = p;
        if (slash) *(slash + 1) = '\0'; else path[0] = '\0';
        lstrcatA(path, "MapSizeExt_crash.log");
        h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    void Close() { if (h != INVALID_HANDLE_VALUE && h) CloseHandle(h); h = nullptr; }
    void Put(const char* s)
    {
        if (!h || h == INVALID_HANDLE_VALUE) return;
        DWORD w = 0;
        WriteFile(h, s, lstrlenA(s), &w, nullptr);
    }
};

#define EMIT(sink, ...)  do { wsprintfA((sink).buf, __VA_ARGS__); (sink).Put((sink).buf); } while (0)

// Name + offset for an address, or a bare address when unattributable.
void EmitAddr(Sink& s, const char* label, DWORD addr)
{
    DWORD off = 0;
    const ModInfo* m = ModuleOf(addr, &off);
    if (m) EMIT(s, "%s0x%08X  %s+0x%X\r\n", label, addr, m->name, off);
    else   EMIT(s, "%s0x%08X  (unattributed)\r\n", label, addr);
}

// The question every one of these reports is really asking: did something
// index the cell array past its end? Answer it directly.
void EmitCellContext(Sink& s, DWORD faultAddr)
{
    const DWORD kMap = 0x87F7E8;              // MapClass::Instance
    DWORD items = 0, w = 0, h = 0, total = 0;
    SafeDword(0x87F924, &items);              // Cells.Items
    SafeDword(kMap + 0x14C, &w);
    SafeDword(kMap + 0x150, &h);
    SafeDword(kMap + 0x154, &total);

    EMIT(s, "map        : cells=0x%08X  W=%d H=%d Total=%d   (config stride=%d total=%d)\r\n",
         items, (int)w, (int)h, (int)total, g_MapStride, g_MapTotal);

    if (!items) { s.Put("cell check : cell array not allocated yet\r\n"); return; }

    const DWORD bytes = (total ? total : (DWORD)g_MapTotal) * 4u;
    if (faultAddr >= items && faultAddr < items + bytes)
    {
        const DWORD idx = (faultAddr - items) / 4u;
        EMIT(s, "cell check : INSIDE the cell array, index %u = (%u,%u) -- the array is "
                "fine, the CellClass* it holds is probably null/stale\r\n",
             idx, idx % (DWORD)g_MapStride, idx / (DWORD)g_MapStride);
    }
    else if (faultAddr >= items && faultAddr < items + bytes + 0x100000)
    {
        const DWORD over = (faultAddr - (items + bytes)) / 4u;
        EMIT(s, "cell check : *** PAST THE END OF THE CELL ARRAY *** by %u entries "
                "(index %u, array holds %u). This is an edge-of-map overrun.\r\n",
             over, (faultAddr - items) / 4u, bytes / 4u);
    }
    else if (faultAddr < 0x10000)
    {
        s.Put("cell check : null-ish pointer, not a cell-array overrun\r\n");
    }
    else
    {
        s.Put("cell check : fault address is outside the cell array\r\n");
    }

    // The dummy cell every failed lookup returns. A crash reached THROUGH it
    // means some decode handed the engine coordinates it could not resolve.
    DWORD sentinel = 0;
    if (SafeDword(ADDR_SENTINEL_STORE, &sentinel))
        EMIT(s, "sentinel   : dummy cell 0x%08X last requested coords (%d,%d)\r\n",
             (DWORD)ADDR_SENTINEL_CELL,
             (int)(short)(sentinel & 0xFFFF), (int)(short)(sentinel >> 16));
}

// No symbols and no frame pointers to trust, so do what a human does with an
// except.txt: walk the stack and print every value that lands inside a known
// module. Noisy by construction -- but it is the call chain, attributed.
void EmitStackScan(Sink& s, DWORD esp)
{
    s.Put("stack (values that land inside a loaded module):\r\n");
    int shown = 0;
    for (DWORD a = esp; a < esp + 0x400 && shown < 48; a += 4)
    {
        DWORD v = 0;
        if (!SafeDword(a, &v)) break;        // off the end of the stack
        DWORD off = 0;
        const ModInfo* m = ModuleOf(v, &off);
        if (!m) continue;
        // A return address is preceded by a call; we cannot cheaply verify
        // that, so print candidates and let the reader judge.
        EMIT(s, "  [0x%08X] 0x%08X  %s+0x%X\r\n", a, v, m->name, off);
        ++shown;
    }
    if (!shown) s.Put("  (nothing attributable)\r\n");
}

LONG CALLBACK Veh(EXCEPTION_POINTERS* ep)
{
    if (!ep || !ep->ExceptionRecord || !ep->ContextRecord)
        return EXCEPTION_CONTINUE_SEARCH;

    const DWORD code = ep->ExceptionRecord->ExceptionCode;

    // Only hardware faults. C++ throws (0xE06D7363), debugger breakpoints and
    // the various benign first-chance codes are none of our business.
    if (code != EXCEPTION_ACCESS_VIOLATION &&
        code != EXCEPTION_ARRAY_BOUNDS_EXCEEDED &&
        code != EXCEPTION_INT_DIVIDE_BY_ZERO &&
        code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_PRIV_INSTRUCTION &&
        code != EXCEPTION_STACK_OVERFLOW)
        return EXCEPTION_CONTINUE_SEARCH;

    if (g_Logged >= kMaxLogged) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedExchange(&g_Busy, 1) != 0) return EXCEPTION_CONTINUE_SEARCH;

    Sink s; s.h = nullptr;
    s.Open();
    if (s.h && s.h != INVALID_HANDLE_VALUE)
    {
        InterlockedIncrement(&g_Logged);

        const CONTEXT* c = ep->ContextRecord;
        const DWORD    eip = c->Eip;

        s.Put("\r\n============================================================\r\n");
        EMIT(s, "MapSizeExt crash watch -- first-chance exception #%d\r\n", (int)g_Logged);
        EMIT(s, "code       : 0x%08X   tick %u   thread %u\r\n",
             code, GetTickCount(), GetCurrentThreadId());

        DWORD faultAddr = 0;
        if (code == EXCEPTION_ACCESS_VIOLATION &&
            ep->ExceptionRecord->NumberParameters >= 2)
        {
            const ULONG_PTR kind = ep->ExceptionRecord->ExceptionInformation[0];
            faultAddr = (DWORD)ep->ExceptionRecord->ExceptionInformation[1];
            EMIT(s, "access     : %s 0x%08X\r\n",
                 kind == 0 ? "READ from" : (kind == 1 ? "WRITE to" : "EXECUTE at"),
                 faultAddr);
        }

        EmitAddr(s, "eip        : ", eip);
        EMIT(s, "regs       : eax=%08X ebx=%08X ecx=%08X edx=%08X\r\n"
                "             esi=%08X edi=%08X ebp=%08X esp=%08X\r\n",
             c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);

        EmitCellContext(s, faultAddr);
        EmitStackScan(s, c->Esp);

        s.Put("(MapSizeExt did not handle this -- the game's own handler runs next.)\r\n");
        s.Close();
    }

    InterlockedExchange(&g_Busy, 0);
    return EXCEPTION_CONTINUE_SEARCH;       // never, ever swallow it
}

} // namespace

// Snapshot the loaded modules so the watcher never has to call the loader
// from a faulting thread. Safe to call repeatedly; later calls pick up DLLs
// that were injected after us (which is most of them).
void CaptureModuleTable()
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return;

    ModInfo tmp[160];
    LONG n = 0;

    MODULEENTRY32 me = { sizeof(MODULEENTRY32) };
    if (Module32First(snap, &me))
    {
        do
        {
            if (n >= (LONG)(sizeof(tmp) / sizeof(tmp[0]))) break;
            tmp[n].base = (DWORD)me.modBaseAddr;
            tmp[n].size = (DWORD)me.modBaseSize;
            lstrcpynA(tmp[n].name, me.szModule, sizeof(tmp[n].name));
            ++n;
        } while (Module32Next(snap, &me));
    }
    CloseHandle(snap);

    if (!n) return;
    // Publish by copying into the live table, then raising the count: the
    // watcher only ever reads entries below g_ModCount.
    for (LONG i = 0; i < n; ++i) g_Mods[i] = tmp[i];
    InterlockedExchange(&g_ModCount, n);
}

void InstallCrashWatch()
{
    CaptureModuleTable();
    AddVectoredExceptionHandler(1 /* call us first */, Veh);
}
