// A5xx texture state, from Mesa 25.0.7 a5xx.xml and fd5_emit.c.
// SPDX-License-Identifier: MIT
#pragma once
#include "hakios-metal/loadstate5.hpp"

namespace hakios::metal::a5xx {
struct TexConst { uint32_t dword[12] = {}; }; // ST4_CONSTANTS, 48 bytes per unit
struct TexSamp { uint32_t dword[4] = {}; };   // ST4_SHADER, 16 bytes per unit

// Both compute and graphics use the same descriptor layouts; BLOCK selects
// SB4_VS_TEX (0), SB4_FS_TEX (4), or SB4_CS_TEX (5).
inline void loadTextureTable(pm4::Stream& stream, uint32_t block, uint32_t type,
                             const uint32_t* data, uint32_t units) {
    if (!units) return;
    const uint32_t stride = type == kStateConstants ? 12 : 4;
    std::vector<uint32_t> w{loadState4Word0(0, kSrcDirect, block, units), type, 0};
    w.insert(w.end(), data, data + units * stride);
    stream.packet(uint32_t(kLoadState4), w.data(), uint32_t(w.size()));
}
} // namespace hakios::metal::a5xx
