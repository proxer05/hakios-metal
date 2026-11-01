// The a5xx compute dispatch's words, against fd5_compute.c: every packet header is DECODED with pm4.hpp's Header
// (so its parity and register must hold), and every register VALUE is a literal worked out by hand from Mesa's
// a5xx.xml field positions. Two raw literals anchor the whole thing. Host build:
//   g++ -std=c++17 -Wall -Wextra -Iinclude -o compute5_test tests/compute5_test.cpp && ./compute5_test
#include <cassert>
#include <cstdio>

#include "hakios-metal/compute5.hpp"

using hakios::metal::pm4::Header;
namespace pm4 = hakios::metal::pm4;
using hakios::metal::a5xx::Dispatch;

static size_t g_word;

static void
reg4(const std::vector<uint32_t> &w, uint32_t wantReg, uint32_t wantCount, const char *what)
{
    Header h = pm4::decode(w[g_word]);
    assert(h.kind == Header::kRegisters);
    if (h.reg != wantReg || h.count != wantCount) {
        fprintf(stderr, "FAIL header at word %zu (%s): reg %#x count %u, want reg %#x count %u\n",
            g_word, what, h.reg, h.count, wantReg, wantCount);
        assert(false);
    }
    g_word++;
}

static void
val(uint32_t got, uint32_t want, const char *what)
{
    if (got != want) {
        fprintf(stderr, "FAIL at word %zu (%s): got %#x, want %#x\n", g_word, what, got, want);
        assert(false);
    }
    g_word++;
}

int
main()
{
    // A 2x3x4 grid of 64-thread groups, no preload, 4 const units, shader at 0xa0000000.
    Dispatch d;
    d.config.instrlen = 0;
    d.cntl.hasSsbo = false;
    d.constlen = 1;
    d.csCtrl0.preload = false;
    d.csCtrl0.halfRegFootprint = 3;
    d.csCtrl0.fullRegFootprint = 5;
    d.csCtrl0.branchstack = 2;
    d.shaderObj = 0xa0000000ull;
    d.ndrange.localX = 64;
    d.ndrange.localY = 64;
    d.ndrange.localZ = 64;
    d.ndrange.groupsX = 2;
    d.ndrange.groupsY = 3;
    d.ndrange.groupsZ = 4;

    std::vector<uint32_t> w;
    d.emit(w);

    // The anchor literal: SP_SP_CNTL (0xe580), count 1, count-parity 0 (odd count), register parity 1
    // (0xe580 has six ones): 0x40000000 | 0x08000000 | 0x00e58000 | 1.
    if (w[0] != 0x48e58001u) {
        fprintf(stderr, "FAIL anchor: got %#x\n", w[0]);
        return 1;
    }
    g_word = 0;

    reg4(w, 0x0e580u, 1, "SP_SP_CNTL");
    val(w[g_word], 0, "SP_SP_CNTL value");

    reg4(w, 0x0e784u, 1, "HLSQ_CONTROL_0");
    // FSTHREADSIZE(TWO_QUADS=0) | CSTHREADSIZE(0)<<2 | 0x880
    val(w[g_word], 0x880u, "HLSQ_CONTROL_0 value");

    reg4(w, 0x0e5f0u, 1, "SP_CS_CTRL_REG0");
    // bit1 0x2 | FULL(5)<<10 | HALF(3)<<4; no BUFFER, no branchstack
    val(w[g_word], 0x2u | (5u << 10) | (3u << 4) | (2u << 25), "SP_CS_CTRL_REG0 value");

    reg4(w, 0x0e790u, 1, "HLSQ_CS_CONFIG");
    val(w[g_word], 1u, "HLSQ_CS_CONFIG value (ENABLED)");

    reg4(w, 0x0e796u, 1, "HLSQ_CS_CNTL");
    val(w[g_word], 0u, "HLSQ_CS_CNTL value (INSTRLEN 0, no SSBO)");

    reg4(w, 0x0e589u, 1, "SP_CS_CONFIG");
    val(w[g_word], 1u, "SP_CS_CONFIG value (ENABLED)");

    reg4(w, 0x0e7dcu, 2, "HLSQ_CS_CONSTLEN");
    val(w[g_word], 1u, "HLSQ_CS_CONSTLEN value");
    val(w[g_word], 0u, "HLSQ_CS_INSTRLEN value");

    reg4(w, 0x0e5f3u, 2, "SP_CS_OBJ_START_LO");
    val(w[g_word], 0xa0000000u, "SP_CS_OBJ_START_LO value");
    val(w[g_word], 0, "SP_CS_OBJ_START_HI value");

    reg4(w, 0x0e78au, 1, "HLSQ_UPDATE_CNTL");
    val(w[g_word], 0x1f00000u, "HLSQ_UPDATE_CNTL value");

    reg4(w, 0x0e7b7u, 2, "HLSQ_CS_CNTL_0");
    // WGIDCONSTID regid(63,0)=0x3f00 | UNK0 0x3f00<<8 | UNK1 0x3f00<<16 | LOCALID 0x3f00<<24
    val(w[g_word], 0x3f00u | (0x3f00u << 8) | (0x3f00u << 16) | (0x3f00u << 24), "HLSQ_CS_CNTL_0 value");
    val(w[g_word], 0x1u, "HLSQ_CS_CNTL_1 value");

    reg4(w, 0x0e7b0u, 7, "HLSQ_CS_NDRANGE_0");
    // KERNELDIM 3 | LOCALSIZEX(64-1)<<2 | LOCALSIZEY<<12 | LOCALSIZEZ<<22; GLOBALSIZE X 128 Y 192 Z 256
    val(w[g_word], (3u << 0) | (63u << 2) | (63u << 12) | (63u << 22), "HLSQ_CS_NDRANGE_0 value");
    val(w[g_word], 128u, "GLOBALSIZE_X");
    val(w[g_word], 0u, "GLOBALOFF_X");
    val(w[g_word], 192u, "GLOBALSIZE_Y");
    val(w[g_word], 0u, "GLOBALOFF_Y");
    val(w[g_word], 256u, "GLOBALSIZE_Z");
    val(w[g_word], 0u, "GLOBALOFF_Z");

    reg4(w, 0x0e7b9u, 3, "HLSQ_CS_KERNEL_GROUP_X");
    val(w[g_word], 1u, "KERNEL_GROUP_X");
    val(w[g_word], 1u, "KERNEL_GROUP_Y");
    val(w[g_word], 1u, "KERNEL_GROUP_Z");

    // The second anchor: CP_EXEC_CS (0x33), 4 words; count parity 0 (4 has one one-bit), opcode parity 1
    // (0x33 has four ones): 0x70000000 | 4 | 0x00330000 | 0x00800000.
    if (w[g_word] != 0x70b30004u) {
        fprintf(stderr, "FAIL CP_EXEC_CS anchor: got %#x\n", w[g_word]);
        return 1;
    }
    g_word++;
    val(w[g_word], 0u, "CP_EXEC_CS reserved");
    val(w[g_word], 2u, "NGROUPS_X");
    val(w[g_word], 3u, "NGROUPS_Y");
    val(w[g_word], 4u, "NGROUPS_Z");

    printf("PASS: %zu words, every header decodes and every value matches fd5\n", w.size());
    return 0;
}
