// The A506 sysmem draw's own words, against Mesa's fd5_emit / fd5_gmem: the stream is decoded packet by packet with
// pm4.hpp's reader (so every header's parity and register must hold), and the registers that steer the draw are held
// to literals worked out by hand. What the shaders themselves contain is not this test's subject; the two hand stages
// are the smallest pair that links (a position-only VS and one-colour FS).
//
// SPDX-License-Identifier: MIT
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "hakios-metal/a5xx.hpp"
#include "hakios-metal/command.hpp"
#include "hakios-metal/fake_gpu.hpp"
#include "hakios-metal/pm4.hpp"
#include "hakios-metal/render5.hpp"
#include "hakios-metal/texture.hpp"
#include "test.hpp"

using namespace hakios::metal;
namespace pm4 = hakios::metal::pm4;

namespace {

constexpr uint32_t kChipA506 = 0x05000600;
constexpr uint32_t kChipA635 = 0x06050002;
constexpr uint32_t kRegidNone = 0xfc;
constexpr uint8_t kSlotPos = 0, kFragData0 = 4;

// One zeroed instruction; the encoders read instrlen/constlen, not the words.
const uint32_t kShaderWords[32] = {};

Stage handStage(uint8_t kind, const uint32_t* words, uint32_t constlen) {
    Stage s{};
    s.kind = kind;
    s.k.words = words;
    s.k.numWords = 32;
    s.k.instrlen = 1;          // one 128-byte unit
    s.k.constlen = constlen;   // vec4 units, as ir3 leaves it
    s.k.maxReg = 0;
    s.k.maxHalfReg = -1;
    s.k.chipId = kChipA506;
    s.k.driverParamsOffsetVec4 = -1;   // no ir3 driver params (the kernel reads no grid builtins)
    s.k.uboPtrsOffsetVec4 = -1;        // no ldc through a UBO pointer table
    s.k.name = kind == kStageVertex ? "render5_vs" : "render5_fs";
    s.vertexIdRegid = s.instanceIdRegid = s.positionRegid = s.pointSizeRegid = kRegidNone;
    s.fragCoordRegid = s.faceRegid = s.sampleIdRegid = s.sampleMaskInRegid = kRegidNone;
    for (uint32_t& ij : s.ijRegid) ij = kRegidNone;
    for (uint32_t& c : s.colorRegid) c = kRegidNone;
    s.depthRegid = s.sampleMaskRegid = s.stencilRefRegid = kRegidNone;
    return s;
}

Program handProgram() {
    Program p{};
    p.vs = handStage(kStageVertex, kShaderWords, 8);
    p.vs.vertexIdRegid = 0;   // r0.x
    p.vs.positionRegid = 1;   // r0.y
    p.vs.numOutputs = 1;
    p.vs.outputs[0] = StageOutput{kSlotPos, 1, 0, 0};

    p.fs = handStage(kStageFragment, kShaderWords, 0);
    p.fs.colorRegid[0] = 0;   // r0.x
    p.fs.numColorOutputs = 1;
    p.fs.outputs[0] = StageOutput{kFragData0, 0, 0, 0};
    p.samples = 1;
    return p;
}

struct Rig {
    Rig() {
        auto f = std::make_unique<FakeGpu>();
        fake = f.get();
        gpu = std::make_unique<Gpu>(std::move(f));
    }
    FakeGpu* fake;
    std::unique_ptr<Gpu> gpu;
};

std::shared_ptr<Texture> makeTarget(Rig& r, uint32_t w = 64, uint32_t h = 64) {
    TextureDesc d;
    d.format = PixelFormat::kRGBA8Unorm;
    d.width = w;
    d.height = h;
    d.usage = kTextureUsageRenderTarget | kTextureUsageRead;
    std::string why;
    auto t = Texture::make(*r.gpu, d, &why);
    CHECK(t != nullptr);
    return t;
}

// Addresses the encoder is given, and the scratch words its fences wait on.
constexpr uint64_t kVsAddress = 0x80000000ull, kFsAddress = 0x80001000ull, kScratch = 0x70000000ull;

// Reading the stream back: first header of OPCODE, last value written to REG, and the CP_EVENT_WRITE sequence.
struct Words {
    explicit Words(const std::vector<uint32_t>& words) : w(words) {}

    size_t find(uint32_t opcode, size_t from = 0) const {
        for (size_t i = from; i < w.size();) {
            pm4::Header h = pm4::decode(w[i]);
            CHECK(h.kind != pm4::Header::kBad);
            if (h.kind == pm4::Header::kCommand && h.opcode == opcode) return i;
            i += 1 + h.count;
        }
        return size_t(-1);
    }
    size_t lastReg(uint32_t reg) const {
        size_t found = size_t(-1);
        for (size_t i = 0; i < w.size();) {
            pm4::Header h = pm4::decode(w[i]);
            if (h.kind == pm4::Header::kRegisters && reg >= h.reg && reg < h.reg + h.count) found = i + 1 + (reg - h.reg);
            i += 1 + h.count;
        }
        return found;
    }
    uint32_t reg(uint32_t r) const {
        size_t i = lastReg(r);
        return i == size_t(-1) ? 0xdeadbeefu : w[i];
    }
    // every CP_EVENT_WRITE of EVENT, by header index
    std::vector<size_t> events(uint32_t event) const {
        std::vector<size_t> out;
        for (size_t i = 0; i < w.size();) {
            pm4::Header h = pm4::decode(w[i]);
            if (h.kind == pm4::Header::kCommand && h.opcode == pm4::kEventWrite && h.count >= 1 && w[i + 1] == event)
                out.push_back(i);
            i += 1 + h.count;
        }
        return out;
    }
    const std::vector<uint32_t>& w;
};

void expectWords(const Words& s, size_t at, std::initializer_list<uint32_t> want, const char* what) {
    uint32_t i = 0;
    for (uint32_t x : want) {
        if (s.w[at + i] != x) {
            std::fprintf(stderr, "  FAIL %s word %u: got %#x, want %#x\n", what, i, s.w[at + i], x);
            test::failures()++;
        }
        i++;
    }
}

struct Encoded {
    Encoded(Rig& r, const Program& p, RenderPass pass, const Draw& d) {
        std::string why;
        target = makeTarget(r);
        pass.color0 = target.get();
        ok = render5::encode(stream, p, pass, d, Bindings{}, Bindings{}, kVsAddress, kFsAddress, kScratch, &why);
        error = why;
    }
    pm4::Stream stream;
    std::shared_ptr<Texture> target;
    bool ok = false;
    std::string error;
};

Draw triangle() {
    Draw d;
    d.primitive = Primitive::kTriangle;
    d.vertexCount = 3;
    d.instanceCount = 1;
    return d;
}

void refused(const Program& p, const RenderPass& pass, const Draw& d, const char* want) {
    Rig r;
    Encoded e(r, p, pass, d);
    CHECK(!e.ok);
    if (e.error != want) std::fprintf(stderr, "  got \"%s\"\n  expected \"%s\"\n", e.error.c_str(), want);
    CHECK(e.error == want);
    CHECK_EQ(e.stream.size(), size_t(0));   // nothing is published before every check passes
}

}  // namespace

TEST(render5_sysmem_draw_words) {
    Rig r;
    Encoded e(r, handProgram(), RenderPass{}, triangle());
    if (!e.ok) std::fprintf(stderr, "  encode refused: \"%s\"\n", e.error.c_str());
    CHECK(e.ok);
    CHECK(e.error.empty());
    CHECK(e.stream.size() != 0);
    Words s(e.stream.words());
    CHECK_EQ(pm4::decode(e.stream.words()[0]).opcode, uint32_t(pm4::kWaitForIdle));   // fd5_emit_restore begins at idle

    // fd5_emit_restore's CP_SET_RENDER_MODE(BYPASS): the mode is word 0 of the packet.
    size_t mode = s.find(0x6c);
    CHECK(mode != size_t(-1));
    if (mode != size_t(-1)) expectWords(s, mode + 1, {1}, "CP_SET_RENDER_MODE");

    // fd5_emit's framebuffer block: RT0 writes all four components.
    CHECK_EQ(s.reg(a5xx::A5XX_RB_RENDER_COMPONENTS), 0xfu);

    // The VS/FS program addresses and their config words: ENABLED (1), VS constlen 2 vec4, FS at instruction 63,
    // which is 64 - instrlen(1) in the high byte; HLSQ_VS_CNTL's two INSTRLEN fields are 1 instruction each.
    const uint32_t fsoff = 63u << 8;
    size_t vsConfig = s.lastReg(a5xx::A5XX_HLSQ_VS_CONFIG);
    CHECK(vsConfig != size_t(-1));
    if (vsConfig != size_t(-1)) expectWords(s, vsConfig, {1, 1u | (2u << 1) | fsoff, fsoff, fsoff, fsoff, 0},
                                             "HLSQ_VS_CONFIG");
    size_t vsCntl = s.lastReg(a5xx::A5XX_HLSQ_VS_CNTL);
    CHECK(vsCntl != size_t(-1));
    if (vsCntl != size_t(-1)) expectWords(s, vsCntl, {2, 2, 0, 0, 0, 0}, "HLSQ_VS_CNTL");
    CHECK_EQ(s.reg(a5xx::A5XX_HLSQ_VS_CONSTLEN), 2u);
    CHECK_EQ(s.reg(a5xx::A5XX_HLSQ_FS_CONSTLEN), 0u);

    // The target is the only colour attachment and its address is the texture's, not a mock's.
    size_t mrt = s.lastReg(a5xx::A5XX_RB_MRT_BUF_INFO);
    CHECK(mrt != size_t(-1));
    if (mrt != size_t(-1)) {
        CHECK_EQ(s.w[mrt + 3], uint32_t(e.target->gpuAddress()));
        CHECK_EQ(s.w[mrt + 4], uint32_t(e.target->gpuAddress() >> 32));
    }

    // No textures are bound in either stage.
    CHECK_EQ(s.reg(a5xx::A5XX_TPL1_VS_TEX_COUNT), 0u);
    CHECK_EQ(s.reg(a5xx::A5XX_TPL1_FS_TEX_COUNT), 0u);

    // The draw itself: DI_PT_TRILIST(4) | AUTO_INDEX(2 << 6) | IGNORE_VISIBILITY(0 << 8), one instance, three vertices.
    size_t draw = s.find(0x38);
    CHECK(draw != size_t(-1));
    if (draw != size_t(-1)) {
        CHECK_EQ(pm4::decode(s.w[draw]).count, 3u);
        expectWords(s, draw + 1, {0x84, 1, 3}, "CP_DRAW_INDX_OFFSET");
    }

    // The fences: a colour flush before the fetch, the CCU invalidate, then the pass-end flush and timestamp. Each
    // timed one targets the scratch word the caller gave and waits for it to read back the sequence number.
    std::vector<size_t> events;
    for (size_t i = 0; i < e.stream.words().size();) {
        pm4::Header h = pm4::decode(e.stream.words()[i]);
        if (h.kind == pm4::Header::kCommand && h.opcode == pm4::kEventWrite && h.count >= 1) events.push_back(i);
        i += 1 + h.count;
    }
    CHECK_EQ(events.size(), size_t(4));
    if (events.size() == 4) {
        CHECK_EQ(s.w[events[0] + 1], 29u);   // PC_CCU_FLUSH_COLOR_TS before the draw
        expectWords(s, events[0] + 2, {uint32_t(kScratch), uint32_t(kScratch >> 32), 1}, "flush 29");
        CHECK_EQ(s.w[events[1] + 1], 25u);   // PC_CCU_INVALIDATE_COLOR
        CHECK_EQ(s.w[events[2] + 1], 29u);   // PC_CCU_FLUSH_COLOR_TS at pass end
        expectWords(s, events[2] + 2, {uint32_t(kScratch + 4), uint32_t((kScratch + 4) >> 32), 2}, "flush 29 end");
        CHECK_EQ(s.w[events[3] + 1], 4u);    // CACHE_FLUSH_TS
        expectWords(s, events[3] + 2, {uint32_t(kScratch + 8), uint32_t((kScratch + 8) >> 32), 3}, "flush 4");
    }
}

TEST(render5_serves_vertex_driver_params) {
    // A5xx lowers vertex_id to VERTEX_ID_ZERO_BASE (the VFD's in0) plus the first_vertex driver param, so the VS
    // declares IR3_CONST_ALLOC_DRIVER_PARAMS even when it binds no buffer (tri_flat_vs: driver_params at c0.x).
    Program p = handProgram();
    p.vs.k.driverParamsOffsetVec4 = 0;
    p.vs.k.constlen = 8;
    Rig r;
    Encoded e(r, p, RenderPass{}, triangle());
    if (!e.ok) std::fprintf(stderr, "  encode refused: \"%s\"\n", e.error.c_str());
    CHECK(e.ok);

    const uint32_t want = a5xx::loadState4Word0(0, a5xx::kSrcDirect, 8, 4);
    size_t at = size_t(-1);
    for (size_t i = 0; i < e.stream.size();) {
        pm4::Header h = pm4::decode(e.stream.words()[i]);
        if (h.kind == pm4::Header::kCommand && h.opcode == 0x30 && h.count >= 3 && e.stream.words()[i + 1] == want) {
            at = i;
            break;
        }
        i += 1 + h.count;
    }
    CHECK(at != size_t(-1));
    if (at != size_t(-1)) {
        Words s(e.stream.words());
        CHECK_EQ(pm4::decode(e.stream.words()[at]).count, 19u);   // 3 header words + four vec4s
        expectWords(s, at + 2, {a5xx::kStateConstants, 0}, "driver params header");
        // draw_id, vtxid_base, instid_base, vtxcnt_max, is_indexed_draw, then the unused pad.
        expectWords(s, at + 4, {0, 0, 0, 3, 0}, "driver params");
    }

    // A fragment stage's dynamic params (frag_size/frag_offset) are not part of this slice.
    p = handProgram();
    p.fs.k.driverParamsOffsetVec4 = 0;
    p.fs.k.constlen = 8;
    refused(p, RenderPass{}, triangle(),
            "A506 render: driver parameters are only served to the vertex stage");
}

TEST(render5_refusals_leave_the_stream_empty) {
    Program p = handProgram();
    RenderPass pass;
    pass.depth = reinterpret_cast<const Texture*>(1);   // any non-null pointer: refused before it is read
    refused(p, pass, triangle(), "A506 sysmem: one colour level with Load/DontCare is required");

    p = handProgram();
    p.fs.numColorOutputs = 2;
    refused(p, RenderPass{}, triangle(), "A506 sysmem: pipeline features exceed the initial draw slice");

    p = handProgram();
    p.fs.k.chipId = kChipA635;
    refused(p, RenderPass{}, triangle(), "A506 sysmem: shader target, binary or register/constant limits are invalid");

    Draw indexed = triangle();
    indexed.indexBuffer = reinterpret_cast<const Buffer*>(1);
    refused(handProgram(), RenderPass{}, indexed,
            "A506 sysmem: nonindexed triangle list, one instance and no depth/stencil/query required");
}
