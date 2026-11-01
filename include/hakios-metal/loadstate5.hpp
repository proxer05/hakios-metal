// The a5xx state loads (CP_LOAD_STATE4, opcode 0x30) and the full shader-object packet, appended to compute5.hpp's
// dispatch. Fields from adreno_pm4.xml's cp_load_state4 domain (DST_OFF 0-13, STATE_SRC 16-17, STATE_BLOCK 18-21,
// NUM_UNIT 22-31; STATE_TYPE 0-1, EXT_SRC_ADDR 2-31 shr 2; EXT_SRC_ADDR_HI) and a5xx.xml's pvt-memory bitsets.
// Witness: fd5_emit.c's fd5_emit_const_user/bo and fd5_program.c's fd5_emit_shader_obj.
#pragma once

#include <cstdint>
#include <vector>

#include "pm4.hpp"

namespace hakios::metal::a5xx {

using pm4::type4;
using pm4::type7;

// CP_LOAD_STATE4 (adreno_pm4.xml): the same encoding serves A4xx and A5xx.
static constexpr pm4::Opcode kLoadState4 = static_cast<pm4::Opcode>(0x30);

// a4xx_state_block (adreno_pm4.xml): the blocks a compute dispatch uses.
enum : uint32_t {
    kBlockCsTex = 0x5,
    kBlockCsShader = 0xd,
    kBlockCsSsbo = 0xf,
};
enum : uint32_t {
    kStateShader = 0,          // ST4_SHADER
    kStateConstants = 1,       // ST4_CONSTANTS
    kStateUbo = 2,             // ST4_UBO
};
enum : uint32_t {
    kSrcDirect = 0,            // SS4_DIRECT
    kSrcIndirect = 2,          // SS4_INDIRECT
};

// word 0/1/2 of CP_LOAD_STATE4 (opcode 0x30).
constexpr uint32_t loadState4Word0(uint32_t dstOff, uint32_t src, uint32_t block, uint32_t numUnit) {
    return ((dstOff & 0x3fffu) << 0) | ((src & 3u) << 16) | ((block & 0xfu) << 18) | ((numUnit & 0x3ffu) << 22);
}
constexpr uint32_t loadState4Word1(uint32_t stateType, uint32_t extAddrLow) {
    return ((stateType & 3u) << 0) | ((extAddrLow >> 2) << 2);
}

// Constants straight from the stream (fd5_emit_const_user): 3 header words, then the values.
inline void
loadConstantsDirect(std::vector<uint32_t> &out, uint32_t regid, const uint32_t *dwords, uint32_t sizedwords)
{
    out.push_back(type7(kLoadState4, 3 + sizedwords));
    out.push_back(loadState4Word0(regid / 4, kSrcDirect, kBlockCsShader, sizedwords / 4));
    out.push_back(loadState4Word1(kStateConstants, 0));
    out.push_back(0);                                   // EXT_SRC_ADDR_HI
    for (uint32_t i = 0; i < sizedwords; i++) {
        out.push_back(dwords[i]);
    }
}

// Constants from a buffer (fd5_emit_const_bo): 3 header words, the buffer's address in words 1/2
// (EXT_SRC_ADDR carries the byte offset, shifted; the STATE_TYPE rides bits 0-1 of word 1).
inline void
loadConstantsBuffer(std::vector<uint32_t> &out, uint32_t regid, uint64_t bufferAddress, uint32_t byteOffset,
                    uint32_t sizedwords)
{
    uint32_t dstOff = regid / 4;
    uint32_t numUnit = sizedwords / 4;
    uint64_t address = bufferAddress + byteOffset;
    out.push_back(type7(kLoadState4, 3));
    out.push_back(loadState4Word0(dstOff, kSrcIndirect, kBlockCsShader, numUnit));
    out.push_back(loadState4Word1(kStateConstants, static_cast<uint32_t>(address)));
    out.push_back(static_cast<uint32_t>(address >> 32));
}

// The full shader-object packet (fd5_emit_shader_obj): six registers from SP_CS_OBJ_START_LO — the shader's
// address, its private-memory parameter, address and size. A kernel that spills none writes zeros for the three
// private-memory words; the sizes are in the units fd5 uses (MEMSIZEPERITEM in 128-bit units per fiber,
// HWSTACKOFFSET in 256-bit units, TOTALPVTMEMSIZE in 1KiB units — the caller's numbers are already scaled).
struct ShaderObj {
    uint64_t shaderAddress = 0;
    uint32_t pvtMemSizePerItem = 0;      // MEMSIZEPERITEM
    uint32_t hwStackOffset = 0;          // HWSTACKOFFSET
    uint64_t pvtMemAddress = 0;
    uint32_t totalPvtMemSize = 0;        // TOTALPVTMEMSIZE

    void emit(std::vector<uint32_t> &out) const {
        out.push_back(type4(A5XX_SP_CS_OBJ_START_LO, 6));
        out.push_back(static_cast<uint32_t>(shaderAddress));
        out.push_back(static_cast<uint32_t>(shaderAddress >> 32));
        out.push_back((pvtMemSizePerItem & 0xffu) | ((hwStackOffset & 0xffffu) << 8));
        out.push_back(static_cast<uint32_t>(pvtMemAddress));
        out.push_back(static_cast<uint32_t>(pvtMemAddress >> 32));
        out.push_back(totalPvtMemSize & 0x3ffffu);
    }
};

}  // namespace hakios::metal::a5xx
