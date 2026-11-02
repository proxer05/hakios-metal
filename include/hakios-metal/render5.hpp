// A506 sysmem rendering. References: Mesa 25.0.7 fd5_program.c,
// fd5_emit.c and fd5_gmem.c; register fields are a5xx.xml's.
// SPDX-License-Identifier: MIT
#pragma once
#include "hakios-metal/render.hpp"

namespace hakios::metal::render5 {
// Initial slice: attribute-free VS, one linear RGBA8/BGRA8 target, triangle
// lists, no depth/MSAA/blend. Constants and sampled VS/FS textures are served.
// Framebuffer fetch is a flushed sysmem texture read. One triangle per fetching
// draw is required: A5xx does not provide raster-ordered feedback this way.
// All validation precedes publication to STREAM; unsupported state is refused.
bool encode(pm4::Stream& stream, const Program& program, const RenderPass& pass,
            const Draw& draw, const Bindings& vs, const Bindings& fs,
            uint64_t vsAddress, uint64_t fsAddress, uint64_t scratch, std::string* why);
}
