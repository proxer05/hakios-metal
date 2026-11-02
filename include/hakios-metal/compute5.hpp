// The Adreno 506's (A5xx, MSM8953) compute dispatch: the register words fd5_compute.c writes, as encoders that
// compute them from a kernel's facts (thread size, register footprints, branchstack, constlen, instrlen), and the
// NDRANGE and CP_EXEC_CS packets that run the grid. Every field position comes from Mesa's a5xx.xml /
// adreno_common.xml / adreno_pm4.xml; tools/check-a5xx.py holds a5xx.hpp to the same XML.
//
// Witness: Mesa 25.0.7 src/gallium/drivers/freedreno/a5xx/fd5_compute.c (cs_program_emit and fd5_launch_grid).
// The pinned kgsl a5xx_reg.h of the daisy-o-oss kernel is the second witness. PM4 packets are the shared
// type-4/type-7 ones (pm4.hpp).
//
// Differences from the A6xx dispatch worth a name:
//   - the shader preloads through SP_CS_OBJ_START when it fits the 32*16-instruction limit (instrlen 0 = none);
//   - there is no CP_SET_MARKER; the mode is implied;
//   - CONSTLEN and INSTRLEN are one two-register packet (HLSQ_CS_CONSTLEN, HLSQ_CS_INSTRLEN);
//   - the work-group and local IDs live in HLSQ_CS_CNTL_0/1; regid(63,0) means "not used";
//   - CP_EXEC_CS is opcode 0x33, CP_EXEC_CS_INDIRECT 0x41 (adreno_pm4.xml), not the A6xx numbering.
#pragma once

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "a5xx.hpp"
#include "loadstate5.hpp"
#include "pm4.hpp"

namespace hakios::metal::a5xx {

using pm4::Opcode;

static constexpr Opcode kExecCs = static_cast<Opcode>(0x33);           // CP_EXEC_CS (adreno_pm4.xml)
static constexpr Opcode kExecCsIndirect = static_cast<Opcode>(0x41);   // CP_EXEC_CS_INDIRECT

// a3xx_threadsize (adreno_common.xml): TWO_QUADS 0, FOUR_QUADS 1. fd5's cs path uses TWO_QUADS.
static constexpr uint32_t kThreadSizeTwoQuads = 0;

// ir3.h: regid(r,c) = (r << 2) | c. 63.0 is the unused 8-bit sysval ID.
constexpr uint16_t regid(uint8_t row, uint8_t col) { return uint16_t((row << 2) | col); }
static constexpr uint16_t kRegidUnused = regid(63, 0);

// --- HLSQ_CONTROL_0_REG: FSTHREADSIZE pos 0, CSTHREADSIZE pos 2 (a5xx.xml); fd5 adds 0x880 --------------------------
struct HlsqControl0 {
    uint32_t words() const {
        // FSTHREADSIZE(TWO_QUADS) | CSTHREADSIZE(TWO_QUADS) | 0x880
        return (kThreadSizeTwoQuads << 0) | (kThreadSizeTwoQuads << 2) | 0x880u;
    }
};

// --- SP_CS_CTRL_REG0 (bitset a5xx_sp_xs_ctrl_reg0): BUFFER pos 2, THREADSIZE pos 3, HALF 4-9, FULL 10-15,
// BRANCHSTACK 25-31; fd5 adds bit 1 ("almost always set"). ---------------------------------------------------------------------------
struct SpCsCtrlReg0 {
    bool     preload = true;        // the BUFFER bit: the shader fits the internal buffer
    uint8_t  threadSize = kThreadSizeTwoQuads;
    uint8_t  halfRegFootprint = 1;  // i->max_half_reg + 1
    uint8_t  fullRegFootprint = 1;  // i->max_reg + 1
    uint8_t  branchstack = 0;

    uint32_t words() const {
        uint32_t v = 0x2u;                                       // fd5's XXX constant (bit 1)
        if (preload) {
            v |= 1u << 2;                                        // BUFFER
        }
        v |= static_cast<uint32_t>(threadSize) << 3;             // THREADSIZE
        v |= static_cast<uint32_t>(halfRegFootprint) << 4;       // HALFREGFOOTPRINT
        v |= static_cast<uint32_t>(fullRegFootprint) << 10;      // FULLREGFOOTPRINT
        v |= static_cast<uint32_t>(branchstack) << 25;           // BRANCHSTACK
        return v;
    }
};

// --- a5xx_xs_config (SP_CS_CONFIG / HLSQ_CS_CONFIG): ENABLED pos 0, CONSTOBJECTOFFSET 1-7, SHADEROBJOFFSET 8-14 -----
struct CsConfig {
    static constexpr uint32_t kEnabled = 1u << 0;

    uint32_t config() const { return kEnabled; }   // CONSTOBJECTOFFSET(0) | SHADEROBJOFFSET(0) | ENABLED
};

// --- a5xx_xs_cntl (HLSQ_CS_CNTL): SSBO_ENABLE pos 0, INSTRLEN 1-31 ---------------------------------------------------
struct CsCntl {
    uint16_t instrlen = 0;
    bool     hasSsbo = false;

    uint32_t words() const {
        uint32_t v = static_cast<uint32_t>(instrlen) << 1;
        if (hasSsbo) {
            v |= 1u;
        }
        return v;
    }
};

// --- HLSQ_CS_CNTL_0/1: WGIDCONSTID 0-7, UNK0 8-15, UNK1 16-23, LOCALIDREGID 24-31 (a5xx.xml) ------------------------
struct CsCntl01 {
    uint16_t workGroupId = kRegidUnused;        // SYSTEM_VALUE_WORKGROUP_ID
    uint16_t localInvocationId = kRegidUnused;  // SYSTEM_VALUE_LOCAL_INVOCATION_ID

    uint32_t cntl0() const {
        return static_cast<uint32_t>(workGroupId) |
               (static_cast<uint32_t>(kRegidUnused) << 8) |
               (static_cast<uint32_t>(kRegidUnused) << 16) |
               (static_cast<uint32_t>(localInvocationId) << 24);
    }
    uint32_t cntl1() const { return 0x1u; }     // fd5's constant
};

// --- HLSQ_CS_NDRANGE_0..6: KERNELDIM 0-1, LOCALSIZEX 2-11, LOCALSIZEY 12-21, LOCALSIZEZ 22-31; GLOBALSIZE/OFF full --
struct NdRange {
    uint32_t kernelDim = 3;
    uint32_t localX = 1, localY = 1, localZ = 1;     // work-group size (values, not minus one)
    uint32_t groupsX = 1, groupsY = 1, groupsZ = 1;

    void words(uint32_t out[7]) const {
        out[0] = ((kernelDim & 3u) << 0) |
                 ((localX - 1) << 2) |
                 ((localY - 1) << 12) |
                 ((localZ - 1) << 22);
        out[1] = localX * groupsX;    // GLOBALSIZE_X
        out[2] = 0;                   // GLOBALOFF_X
        out[3] = localY * groupsY;    // GLOBALSIZE_Y
        out[4] = 0;                   // GLOBALOFF_Y
        out[5] = localZ * groupsZ;    // GLOBALSIZE_Z
        out[6] = 0;                   // GLOBALOFF_Z
    }
};

// --- the packets ------------------------------------------------------------------------------------------------------
// The shared pm4.hpp headers: type 4 has its count parity at bit 7, type 7 at bit 15, checked against Mesa's
// pm4_pkt4_hdr / pm4_pkt7_hdr by the a6xx tests. Counts stay under pm4::kMaxType4Count.
using pm4::type4;
using pm4::type7;

// A dispatch's whole register sequence, as fd5 writes it (cs_program_emit then the grid part). The shader's own
// preload (fd5_emit_shader) follows HLSQ_UPDATE_CNTL when instrlen != 0; the constants go in through
// fd5_emit_cs_consts before the NDRANGE packet.
struct Dispatch {
    HlsqControl0 control0;
    SpCsCtrlReg0 csCtrl0;
    CsConfig     config;
    CsCntl       cntl;              // instrlen lives here, once
    uint16_t     constlen = 0;    // v->constlen / 4 (128-bit units; fd5 asserts a multiple of 4)
    CsCntl01     cntl01;
    NdRange      ndrange;
    ShaderObj    obj;             // the shader address and its private memory

    void emit(std::vector<uint32_t> &out) const {
        auto w4 = [&](uint32_t reg, std::initializer_list<uint32_t> words) {
            out.push_back(type4(reg, static_cast<uint32_t>(words.size())));
            for (uint32_t v : words) {
                out.push_back(v);
            }
        };

        w4(A5XX_SP_SP_CNTL, {0});
        w4(A5XX_HLSQ_CONTROL_0_REG, {control0.words()});
        w4(A5XX_SP_CS_CTRL_REG0, {csCtrl0.words()});
        w4(A5XX_HLSQ_CS_CONFIG, {config.config()});
        w4(A5XX_HLSQ_CS_CNTL, {cntl.words()});
        w4(A5XX_SP_CS_CONFIG, {config.config()});
        w4(A5XX_HLSQ_CS_CONSTLEN, {static_cast<uint32_t>(constlen),
                                         static_cast<uint32_t>(cntl.instrlen)});
        obj.emit(out);
        w4(A5XX_HLSQ_UPDATE_CNTL, {0x1f00000u});
        w4(A5XX_HLSQ_CS_CNTL_0, {cntl01.cntl0(), cntl01.cntl1()});
        uint32_t nd[7];
        ndrange.words(nd);
        w4(A5XX_HLSQ_CS_NDRANGE_0, {nd[0], nd[1], nd[2], nd[3], nd[4], nd[5], nd[6]});
        w4(A5XX_HLSQ_CS_KERNEL_GROUP_X, {1, 1, 1});

        // CP_EXEC_CS: one reserved word, then the three NGROUPS words.
        out.push_back(type7(kExecCs, 4));
        out.push_back(0);
        out.push_back(ndrange.groupsX);
        out.push_back(ndrange.groupsY);
        out.push_back(ndrange.groupsZ);
    }
};

}  // namespace hakios::metal::a5xx
