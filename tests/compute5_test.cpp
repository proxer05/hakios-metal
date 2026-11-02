// The a5xx compute dispatch's words, against fd5_compute.c: every packet header is DECODED with pm4.hpp's reader
// (so its parity and register must hold) and every register VALUE is a literal worked out by hand from Mesa's
// a5xx.xml field positions. Two raw literals anchor the whole thing.
#include <vector>

#include "hakios-metal/compute5.hpp"
#include "test.hpp"

using hakios::metal::pm4::Header;
namespace pm4 = hakios::metal::pm4;

namespace {

struct Cursor {
    const std::vector<uint32_t> &w;
    size_t i = 0;
    int bad = 0;

    void reg4(uint32_t wantReg, uint32_t wantCount, const char *what) {
        Header h = pm4::decode(w[i]);
        if (h.kind != Header::kRegisters || h.reg != wantReg || h.count != wantCount) {
            std::fprintf(stderr, "  FAIL header at word %zu (%s): reg %#x count %u, want reg %#x count %u\n",
                i, what, h.reg, h.count, wantReg, wantCount);
            bad++;
        }
        i++;
    }
    void val(uint32_t want, const char *what) {
        if (w[i] != want) {
            std::fprintf(stderr, "  FAIL at word %zu (%s): got %#x, want %#x\n", i, what, w[i], want);
            bad++;
        }
        i++;
    }
};

}  // namespace

TEST(compute5_dispatch_words)
{
    // A 2x3x4 grid of 64-thread groups, no preload, 4 const units, shader at 0xa0000000.
    hakios::metal::a5xx::Dispatch d;
    d.cntl.instrlen = 0;
    d.cntl.hasSsbo = false;
    d.constlen = 1;
    d.csCtrl0.preload = false;
    d.csCtrl0.halfRegFootprint = 3;
    d.csCtrl0.fullRegFootprint = 5;
    d.csCtrl0.branchstack = 2;
    d.obj.shaderAddress = 0xa0000000ull;
    d.ndrange.localX = 64;
    d.ndrange.localY = 64;
    d.ndrange.localZ = 64;
    d.ndrange.groupsX = 2;
    d.ndrange.groupsY = 3;
    d.ndrange.groupsZ = 4;

    std::vector<uint32_t> w;
    d.emit(w);
    CHECK_EQ(w.size(), 44u);

    // The anchor literal: SP_SP_CNTL (0xe580), count 1, count-parity 0 (odd count), register parity 1
    // (0xe580 has six ones): 0x40000000 | 0x08000000 | 0x00e58000 | 1.
    CHECK_EQ(w[0], 0x48e58001u);

    Cursor c{w};
    c.reg4(0x0e580u, 1, "SP_SP_CNTL");
    c.val(0, "SP_SP_CNTL value");
    c.reg4(0x0e784u, 1, "HLSQ_CONTROL_0");
    c.val(0x880u, "HLSQ_CONTROL_0 value");
    c.reg4(0x0e5f0u, 1, "SP_CS_CTRL_REG0");
    c.val(0x2u | (5u << 10) | (3u << 4) | (2u << 25), "SP_CS_CTRL_REG0 value");
    c.reg4(0x0e790u, 1, "HLSQ_CS_CONFIG");
    c.val(1u, "HLSQ_CS_CONFIG value (ENABLED)");
    c.reg4(0x0e796u, 1, "HLSQ_CS_CNTL");
    c.val(0u, "HLSQ_CS_CNTL value (INSTRLEN 0, no SSBO)");
    c.reg4(0x0e589u, 1, "SP_CS_CONFIG");
    c.val(1u, "SP_CS_CONFIG value (ENABLED)");
    c.reg4(0x0e7dcu, 2, "HLSQ_CS_CONSTLEN");
    c.val(1u, "HLSQ_CS_CONSTLEN value");
    c.val(0u, "HLSQ_CS_INSTRLEN value");
    c.reg4(0x0e5f3u, 6, "SP_CS_OBJ_START_LO");
    c.val(0xa0000000u, "SP_CS_OBJ_START_LO value");
    c.val(0, "SP_CS_OBJ_START_HI value");
    c.val(0, "SP_CS_PVT_MEM_PARAM value");
    c.val(0, "SP_CS_PVT_MEM_ADDR_LO value");
    c.val(0, "SP_CS_PVT_MEM_ADDR_HI value");
    c.val(0, "SP_CS_PVT_MEM_SIZE value");
    c.reg4(0x0e78au, 1, "HLSQ_UPDATE_CNTL");
    c.val(0x1f00000u, "HLSQ_UPDATE_CNTL value");
    c.reg4(0x0e7b7u, 2, "HLSQ_CS_CNTL_0");
    c.val(0xfcfcfcfcu, "HLSQ_CS_CNTL_0 value (four 8-bit r63.x IDs)");
    c.val(0x1u, "HLSQ_CS_CNTL_1 value");
    c.reg4(0x0e7b0u, 7, "HLSQ_CS_NDRANGE_0");
    c.val((3u << 0) | (63u << 2) | (63u << 12) | (63u << 22), "HLSQ_CS_NDRANGE_0 value");
    c.val(128u, "GLOBALSIZE_X");
    c.val(0u, "GLOBALOFF_X");
    c.val(192u, "GLOBALSIZE_Y");
    c.val(0u, "GLOBALOFF_Y");
    c.val(256u, "GLOBALSIZE_Z");
    c.val(0u, "GLOBALOFF_Z");
    c.reg4(0x0e7b9u, 3, "HLSQ_CS_KERNEL_GROUP_X");
    c.val(1u, "KERNEL_GROUP_X");
    c.val(1u, "KERNEL_GROUP_Y");
    c.val(1u, "KERNEL_GROUP_Z");

    // The second anchor: CP_EXEC_CS (0x33), 4 words; count parity 0 (4 has one one-bit), opcode parity 1
    // (0x33 has four ones): 0x70000000 | 4 | 0x00330000 | 0x00800000.
    CHECK_EQ(w[c.i], 0x70b30004u);
    c.i++;
    c.val(0u, "CP_EXEC_CS reserved");
    c.val(2u, "NGROUPS_X");
    c.val(3u, "NGROUPS_Y");
    c.val(4u, "NGROUPS_Z");

    CHECK_EQ(c.bad, 0);
    test::failures() += static_cast<unsigned>(c.bad);
}
