#include "hakios-metal/render5.hpp"
#include "hakios-metal/texture.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace hakios::metal::render5 {
namespace {
using namespace a5xx;
void regs(pm4::Stream& s, uint32_t r, std::initializer_list<uint32_t> w) { s.registers(r, w.begin(), uint32_t(w.size())); }
void reg(pm4::Stream& s, uint32_t r, uint32_t w) { s.registers(r, &w, 1); }
void packet(pm4::Stream& s, uint32_t op, std::initializer_list<uint32_t> w) { s.packet(op, w.begin(), uint32_t(w.size())); }
uint32_t bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
bool refuse(std::string* why, const char* text) { *why = text; return false; }
void append(pm4::Stream& to, const pm4::Stream& from) {
    const auto& w = from.words();
    for (size_t i = 0; i < w.size();) {
        const auto h = pm4::decode(w[i]);
        if (h.kind == pm4::Header::kRegisters) to.registers(h.reg, &w[i+1], h.count);
        else to.packet(h.opcode, &w[i+1], h.count);
        i += h.count + 1;
    }
}

// CP_EVENT_WRITE followed by CP_WAIT_REG_MEM (memory, equal). No A6xx
// timestamp bit: fd5_event_write supplies the scratch address and sequence.
void flush(pm4::Stream& s, uint32_t event, uint64_t address, uint32_t seq) {
    packet(s, 0x46, {event, uint32_t(address), uint32_t(address >> 32), seq});
    packet(s, 0x3c, {0x13, uint32_t(address), uint32_t(address >> 32), seq, ~0u, 16});
}
void invalidate(pm4::Stream& s) {
    s.waitForIdle();
    regs(s, A5XX_UCHE_CACHE_INVALIDATE_MIN_LO, {0, 0, 0, 0, 0x12});
    s.waitForIdle();
}

void load(pm4::Stream& s, uint32_t block, uint32_t offset, const uint32_t* w, uint32_t count) {
    std::vector<uint32_t> p{loadState4Word0(offset, kSrcDirect, block, count / 4), kStateConstants, 0};
    p.insert(p.end(), w, w + count);
    s.packet(0x30, p.data(), uint32_t(p.size()));
}
bool constants(pm4::Stream& s, const Kernel& k, const Bindings& b, uint32_t block, uint64_t program,
               const uint32_t* driverParams, std::string* why) {
    if (k.numSsbos || k.numBufs)
        return refuse(why, "A506 render: shader storage buffers are not implemented");
    if (k.driverParamsOffsetVec4 >= 0) {
        // ir3's a5xx VS takes first_vertex/base_instance/is_indexed_draw from
        // four driver-param vec4s; the FS's dynamic params stay unsupported.
        if (driverParams == nullptr)
            return refuse(why, "A506 render: driver parameters are only served to the vertex stage");
        if (uint32_t(k.driverParamsOffsetVec4) + 4 > k.constlen)
            return refuse(why, "A506 render: driver parameters exceed constlen");
        load(s, block, uint32_t(k.driverParamsOffsetVec4), driverParams, 16);
    }
    if (k.numImmediates) {
        if (!k.immediates || k.immediatesOffsetVec4 + (k.numImmediates + 3) / 4 > k.constlen)
            return refuse(why, "A506 render: immediates exceed constlen");
        std::vector<uint32_t> imm(k.immediates, k.immediates + k.numImmediates);
        imm.resize((imm.size() + 3) & ~size_t(3), 0);
        load(s, block, k.immediatesOffsetVec4, imm.data(), uint32_t(imm.size()));
    }
    auto bound = [&](uint32_t index, uint64_t start, uint64_t bytes, uint64_t* address) {
        if (index >= b.buffers.size() || !b.buffers[index].buffer) return false;
        const auto& x = b.buffers[index];
        if (x.offset > x.buffer->length() || x.offset % 4 || start > x.buffer->bo().size() - x.offset ||
            bytes > x.buffer->bo().size() - x.offset - start) return false;
        *address = x.buffer->gpuAddress() + x.offset + start;
        return true;
    };
    if (k.uboPtrsOffsetVec4 >= 0 && k.numUbos) {
        std::vector<uint32_t> w(((k.numUbos + 1) & ~1u) * 2, 0);
        if (uint32_t(k.uboPtrsOffsetVec4) + w.size() / 4 > k.constlen) return refuse(why, "A506 render: UBO pointers exceed constlen");
        for (uint32_t i = 0; i < k.numUbos; ++i) {
            uint64_t a = 0;
            if (k.constantDataUbo == i + 1) a = program + k.constantDataOffset;
            else if (!bound(i, 0, 0, &a) && i + 1 == k.numUbos) return refuse(why, "A506 render: missing UBO");
            w[2*i] = uint32_t(a); w[2*i+1] = uint32_t(a >> 32);
        }
        load(s, block, uint32_t(k.uboPtrsOffsetVec4), w.data(), uint32_t(w.size()));
    }
    for (uint32_t i = 0; i < k.numUboRanges; ++i) {
        const auto& r = k.uboRanges[i];
        if (!r.sizeDwords || r.sizeDwords % 4 || r.offsetVec4 + r.sizeDwords / 4 > k.constlen)
            return refuse(why, "A506 render: invalid pushed constant range");
        if (k.constantDataUbo == r.block + 1) {
            if (uint64_t(k.constantDataOffset) + k.constantDataSize > uint64_t(k.numWords) * 4)
                return refuse(why, "A506 render: constant data exceeds program");
            std::vector<uint32_t> w(r.sizeDwords, 0);
            for (uint32_t j = 0; j < r.sizeDwords; ++j)
                if (4ull * (r.startDword + j + 1) <= k.constantDataSize) w[j] = k.words[k.constantDataOffset / 4 + r.startDword + j];
            load(s, block, r.offsetVec4, w.data(), uint32_t(w.size()));
        } else {
            uint64_t a;
            if (!bound(r.block, 4ull * r.startDword, 4ull * r.sizeDwords, &a) || a % 16)
                return refuse(why, "A506 render: pushed constant range is unmapped or not 16-byte aligned");
            packet(s, 0x30, {loadState4Word0(r.offsetVec4, kSrcIndirect, block, r.sizeDwords / 4),
                            uint32_t(a) | kStateConstants, uint32_t(a >> 32)});
        }
    }
    return true;
}

void shader(pm4::Stream& s, const Stage& stage, uint64_t address, bool fs, uint32_t preload) {
    const Kernel& k = stage.k;
    uint32_t ctrl = 2 | (preload ? 4 : 0) | (uint32_t(k.maxHalfReg + 1) << 4) | (uint32_t(k.maxReg + 1) << 10) |
                    (k.branchstackHw << 25) | (stage.needPixlod ? 1u << 20 : 0);
    if (fs) ctrl |= 0x40000 | (uint32_t(k.doubleThreadsize) << 3) | (stage.totalIn ? 1u << 16 : 0);
    reg(s, fs ? A5XX_SP_FS_CTRL_REG0 : A5XX_SP_VS_CTRL_REG0, ctrl);
    regs(s, fs ? A5XX_SP_FS_OBJ_START_LO : A5XX_SP_VS_OBJ_START_LO,
         {uint32_t(address), uint32_t(address >> 32), 0, 0, 0, 0});
    if (preload) packet(s, 0x30, {loadState4Word0(0, kSrcIndirect, fs ? 12 : 8, preload), uint32_t(address), uint32_t(address >> 32)});
}
} // namespace

bool encode(pm4::Stream& stream, const Program& p, const RenderPass& pass, const Draw& d,
            const Bindings& vsb, const Bindings& fsb, uint64_t va, uint64_t fa, uint64_t scratch, std::string* why) {
    const Stage& vs = p.vs; const Stage& fs = p.fs;
    if (!pass.color0 || pass.depth || pass.stencil || pass.level || pass.slice || pass.load == LoadAction::kClear ||
        (pass.store != StoreAction::kStore && pass.store != StoreAction::kDontCare))
        return refuse(why, "A506 sysmem: one colour level with Load/DontCare is required");
    for (const auto& a : pass.more) if (a.texture) return refuse(why, "A506 sysmem: multiple render targets are not implemented");
    const Texture& target = *pass.color0;
    const auto format = target.format();
    if ((format != PixelFormat::kRGBA8Unorm && format != PixelFormat::kBGRA8Unorm) || target.type() != TextureType::k2D ||
        !(target.desc().usage & kTextureUsageRenderTarget) || target.samples() != 1 || target.compressed())
        return refuse(why, "A506 sysmem: linear single-sample RGBA8/BGRA8 target required");
    if (p.samples != 1 || p.blend.enabled || p.blend.writeMask != 15 || vs.numAttributes || vs.clipMask || vs.cullMask ||
        vs.writesLayer || vs.writesViewport || vs.pointSizeRegid != 0xfc || fs.numColorOutputs != 1 || fs.colorRegid[0] == 0xfc ||
        fs.writesPos || fs.writesSmask || fs.writesStencilref || fs.dualSrcBlend || fs.perSamp || fs.readsPrimid || fs.numSamplerPrefetch)
        return refuse(why, "A506 sysmem: pipeline features exceed the initial draw slice");
    if (d.primitive != Primitive::kTriangle || !d.vertexCount || d.vertexCount % 3 || d.indexBuffer || d.indirect ||
        d.instanceCount != 1 || d.vertexStart || d.baseInstance || d.cullMode || d.depth.write || d.depth.compare != 7 ||
        d.stencil.enabled || d.visibilityMode)
        return refuse(why, "A506 sysmem: nonindexed triangle list, one instance and no depth/stencil/query required");
    if (fs.fbRead && d.vertexCount != 3) return refuse(why, "A506 framebuffer fetch requires one triangle per draw for ordered feedback");
    for (const Stage* s : {&vs, &fs}) {
        const Kernel& k = s->k;
        if (k.chipId != 0x05000600 || !k.words || !k.instrlen || uint64_t(k.instrlen) * 128 > uint64_t(k.numWords) * 4 ||
            k.pvtmemSize || k.mergedRegs || k.constlen > 256 || k.maxReg >= 63 || k.maxHalfReg >= 63 ||
            k.numUboRanges > kMaxUboRanges || k.numUbos > kMaxBufs)
            return refuse(why, "A506 sysmem: shader target, binary or register/constant limits are invalid");
    }
    if ((vs.k.constlen + 3) / 4 + (fs.k.constlen + 3) / 4 > 64 || !va || !fa || (va | fa) % 128 || !scratch || scratch % 16)
        return refuse(why, "A506 sysmem: program/scratch alignment or combined constants exceed hardware limits");
    Linkage link;
    if (!render::link(vs, fs, &link, why)) return false;
    if (link.cnt > 31 || link.maxLoc > 127) return refuse(why, "A506 sysmem: linkage exceeds VPC limits");
    const uint32_t w = target.width(), h = target.height();
    a5xx::TexConst td;
    if (!target.sampledDescriptor5(&td, why)) return false;
    Bindings feedback = fsb;
    for (uint32_t i = 0; i < kMaxTextureSlots; ++i) {
        if (!fs.k.textures[i].fbTarget) continue;
        if (!fs.fbRead || fs.k.textures[i].fbTarget != 1) return refuse(why, "A506 framebuffer fetch names an absent colour attachment");
        if (feedback.textures.size() <= i) feedback.textures.resize(i + 1, nullptr);
        feedback.textures[i] = &target;
    }
    TextureTables5 vt, ft;
    if (!textureTables5(vs.k, vsb, &vt, why) || !textureTables5(fs.k, feedback, &ft, why)) return false;
    // ir3_driver_params_vs: draw_id, vtxid_base, instid_base, vtxcnt_max, is_indexed_draw. The slice draws
    // nonindexed triangles from vertex zero with no base instance, but the values are still filled in.
    uint32_t driverParams[16] = {};
    driverParams[0] = 0;                       // draw_id
    driverParams[1] = d.vertexStart;           // vtxid_base
    driverParams[2] = d.baseInstance;          // instid_base
    driverParams[3] = d.vertexCount;           // vtxcnt_max
    driverParams[4] = d.indexBuffer ? ~0u : 0; // is_indexed_draw
    pm4::Stream vc, fc;
    if (!constants(vc, vs.k, vsb, 8, va, driverParams, why) || !constants(fc, fs.k, fsb, 12, fa, nullptr, why)) return false;
    Viewport v = d.viewport;
    if (v.width == 0) v = {0, 0, double(w), double(h), 0, 1};
    if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.width) || !std::isfinite(v.height) ||
        !std::isfinite(v.znear) || !std::isfinite(v.zfar) || v.width <= 0 || v.height <= 0 ||
        v.x < 0 || v.y < 0 || v.x + v.width > w || v.y + v.height > h || v.znear < 0 || v.zfar > 1 || v.znear > v.zfar)
        return refuse(why, "A506 sysmem: invalid viewport");
    Scissor sc = d.scissor;
    if (!sc.width) sc = {0, 0, w, h};
    if (!sc.height || uint64_t(sc.x) + sc.width > w || uint64_t(sc.y) + sc.height > h) return refuse(why, "A506 sysmem: invalid scissor");

    // fd5_emit_restore + fd5_emit_sysmem_prep. A506 has one SP core.
    pm4::Stream s;
    s.waitForIdle();
    packet(s, 0x6c, {1, 0, 0, 0, 0}); // CP_SET_RENDER_MODE BYPASS
    flush(s, 29, scratch, 1); // PC_CCU_FLUSH_COLOR_TS: previous draw before fetch
    invalidate(s);
    packet(s, 0x46, {25}); // PC_CCU_INVALIDATE_COLOR
    const std::pair<uint32_t, uint32_t> initial[] = {
        {A5XX_HLSQ_UPDATE_CNTL, 0xfffff}, {A5XX_PC_RESTART_INDEX, ~0u}, {A5XX_PC_RASTER_CNTL, 0x12},
        {A5XX_RB_MODE_CNTL, 0x44}, {A5XX_RB_DBG_ECO_CNTL, 0x100000}, {A5XX_VFD_MODE_CNTL, 0},
        {A5XX_PC_MODE_CNTL, 0x1f}, {A5XX_SP_MODE_CNTL, 0x1e}, {A5XX_SP_DBG_ECO_CNTL, 0x40000800},
        {A5XX_TPL1_MODE_CNTL, 0x544}, {A5XX_VPC_DBG_ECO_CNTL, 0x400}, {A5XX_HLSQ_MODE_CNTL, 1},
        {A5XX_VPC_MODE_CNTL, 0}, {A5XX_SP_VS_CONFIG_MAX_CONST, 0}, {A5XX_SP_FS_CONFIG_MAX_CONST, 0},
        {A5XX_GRAS_SU_CONSERVATIVE_RAS_CNTL, 0}, {A5XX_GRAS_SC_SCREEN_SCISSOR_CNTL, 0},
        {A5XX_GRAS_SC_BIN_CNTL, 0}, {A5XX_VPC_FS_PRIMITIVEID_CNTL, 0xff}, {A5XX_VPC_SO_OVERRIDE, 0},
        {A5XX_VPC_SO_BUF_CNTL, 0}, {A5XX_PC_GS_PARAM, 0}, {A5XX_PC_HS_PARAM, 0},
        {A5XX_TPL1_TP_FS_ROTATION_CNTL, 0}, {A5XX_GRAS_SU_LAYERED, 0}, {A5XX_PC_GS_LAYERED, 0},
        {A5XX_SP_HS_CTRL_REG0, 0}, {A5XX_SP_GS_CTRL_REG0, 0}, {A5XX_PC_POWER_CNTL, 0}, {A5XX_VFD_POWER_CNTL, 0},
        {A5XX_RB_CCU_CNTL, 0x10000000}, {A5XX_RB_CNTL, 1u << 17}, {A5XX_GRAS_SC_CNTL, 0}, {A5XX_RB_RENDER_CNTL, 0},
        {A5XX_RB_WINDOW_OFFSET, 0}, {A5XX_GRAS_LRZ_CNTL, 0},
        {A5XX_RB_DEPTH_CNTL, 0}, {A5XX_RB_STENCIL_CONTROL, 0}, {A5XX_GRAS_SU_DEPTH_BUFFER_INFO, 0},
        // fd5_emit_msaa(MSAA_ONE): SAMPLES=0, and MSAA_DISABLE (bit 2) in each DEST_MSAA_CNTL.
        {A5XX_RB_RAS_MSAA_CNTL, 0}, {A5XX_GRAS_SC_RAS_MSAA_CNTL, 0}, {A5XX_TPL1_TP_RAS_MSAA_CNTL, 0},
        {A5XX_RB_DEST_MSAA_CNTL, 4}, {A5XX_GRAS_SC_DEST_MSAA_CNTL, 4}, {A5XX_TPL1_TP_DEST_MSAA_CNTL, 4},
    };
    for (auto [r, x] : initial) reg(s, r, x);
    regs(s, A5XX_HLSQ_TIMEOUT_THRESHOLD_0, {0x80, 0});
    packet(s, 0x43, {0x40000, 0, 0}); // CP_SET_DRAW_STATE disable all
    packet(s, 0x1d, {0}); // CP_SKIP_IB2_ENABLE_GLOBAL
    packet(s, 0x64, {1}); // CP_SET_VISIBILITY_OVERRIDE
    regs(s, A5XX_GRAS_SC_WINDOW_SCISSOR_TL, {0, (w - 1) | ((h - 1) << 16)});
    regs(s, A5XX_RB_RESOLVE_CNTL_1, {0, (w - 1) | ((h - 1) << 16)});
    regs(s, A5XX_RB_DEPTH_BUFFER_INFO, {0, 0, 0, 0, 0});
    reg(s, A5XX_RB_STENCIL_INFO, 0);
    regs(s, A5XX_RB_MRT_BUF_INFO, {0x30u | (uint32_t(target.info().swap) << 13), uint32_t(target.bytesPerRow() >> 6),
        td.dword[3] << 6, uint32_t(target.gpuAddress()), uint32_t(target.gpuAddress() >> 32)});
    reg(s, A5XX_SP_FS_MRT_REG, 0x30);
    reg(s, A5XX_RB_RENDER_COMPONENTS, 0xf); // fd5_emit: RT0 writes R, G, B and A
    for (unsigned i = 0; i < 8; ++i) {
        regs(s, A5XX_RB_MRT_FLAG_BUFFER(i), {0, 0, 0, 0});
        reg(s, A5XX_RB_MRT_CONTROL + 7 * i, i == 0 ? 0x780 : 0);
    }
    reg(s, A5XX_SP_BLEND_CNTL, 0x100); reg(s, A5XX_RB_BLEND_CNTL, 0xffff0000);
    const uint32_t vsconst = (vs.k.constlen + 3) / 4, fsconst = (fs.k.constlen + 3) / 4;
    uint32_t vi = vs.k.instrlen, fi = fs.k.instrlen;
    if (vi + fi > 64) { if (fi < 64) vi = 0; else if (vi < 64) fi = 0; else vi = fi = 0; }
    const uint32_t fsoff = (64 - fi) << 8, fsconfig = 1 | (vsconst << 1) | fsoff;
    regs(s, A5XX_HLSQ_VS_CONFIG, {1, fsconfig, fsoff, fsoff, fsoff, 0});
    regs(s, A5XX_SP_VS_CONFIG, {1, fsconfig, fsoff, fsoff, fsoff, 0});
    regs(s, A5XX_HLSQ_VS_CNTL, {vi << 1, fi << 1, 0, 0, 0, 0});
    regs(s, A5XX_HLSQ_VS_CONSTLEN, {vsconst, vi}); regs(s, A5XX_HLSQ_FS_CONSTLEN, {fsconst, fi});
    for (uint32_t r : {A5XX_HLSQ_HS_CONSTLEN, A5XX_HLSQ_DS_CONSTLEN, A5XX_HLSQ_GS_CONSTLEN, A5XX_HLSQ_CS_CONSTLEN}) regs(s, r, {0, 0});
    shader(s, vs, va, false, vi); shader(s, fs, fa, true, fi);
    for (unsigned i = 0; i < 4; ++i) reg(s, A5XX_VPC_VAR_DISABLE + i, ~link.varmask[i]);
    uint32_t out[16] = {}, loc[8] = {};
    for (unsigned i = 0; i < link.cnt; ++i) {
        out[i/2] |= (link.var[i].regid | (uint32_t(link.var[i].compmask) << 8)) << (16 * (i % 2));
        loc[i/4] |= uint32_t(link.var[i].loc) << (8 * (i % 4));
    }
    s.registers(A5XX_SP_VS_OUT_REG, out, 16); s.registers(A5XX_SP_VS_VPC_DST_REG, loc, 8);
    reg(s, A5XX_PC_PRIM_VTX_CNTL, 0); reg(s, A5XX_SP_PRIMITIVE_CNTL, link.cnt);
    reg(s, A5XX_VPC_CNTL_0, link.maxLoc | (fs.totalIn ? 0x800 : 0) | 0x10000);
    reg(s, A5XX_VPC_PACK, fs.totalIn | 0xff00);
    s.registers(A5XX_VPC_VARYING_INTERP_MODE, link.interpMode, 8); s.registers(A5XX_VPC_VARYING_PS_REPL_MODE, link.psReplMode, 8);
    const auto* ij = fs.ijRegid;
    uint32_t zw = fs.fragCoordRegid == 0xfc ? 0xfc : fs.fragCoordRegid + 2;
    regs(s, A5XX_HLSQ_CONTROL_0_REG, {0x880u | uint32_t(fs.k.doubleThreadsize), 0x3f,
        fs.faceRegid | (fs.sampleIdRegid << 8) | (fs.sampleMaskInRegid << 16) | (ij[6] << 24),
        ij[0] | (ij[3] << 8) | (ij[2] << 16) | (ij[5] << 24),
        fs.fragCoordRegid | (zw << 8) | (ij[1] << 16) | (ij[4] << 24)});
    uint32_t inputs = uint32_t(fs.fragcoordCompmask) << 6;
    const unsigned ijOrder[6] = {0, 2, 1, 3, 5, 4};
    for (unsigned i = 0; i < 6; ++i) if (ij[ijOrder[i]] != 0xfc) inputs |= 1u << i;
    if (fs.fragcoordCompmask || fs.faceRegid != 0xfc) inputs |= 8;
    reg(s, A5XX_GRAS_CNTL, inputs); regs(s, A5XX_RB_RENDER_CONTROL0, {inputs, fs.faceRegid != 0xfc ? 2u : 0u});
    reg(s, A5XX_SP_FS_OUTPUT_REG, fs.colorRegid[0] | (fs.colorHalf[0] ? 0x100u : 0));
    reg(s, A5XX_RB_FS_OUTPUT_CNTL, 1); reg(s, A5XX_SP_FS_OUTPUT_CNTL, 1 | (0xfcu << 5) | (0xfcu << 13));
    reg(s, A5XX_HLSQ_UPDATE_CNTL, 0x020fffff); reg(s, A5XX_VPC_GS_SIV_CNTL, 0xffff); reg(s, A5XX_SP_SP_CNTL, 0x10);
    reg(s, A5XX_GRAS_VS_CL_CNTL, 0); reg(s, A5XX_VPC_CLIP_CNTL, 0xffff00); reg(s, A5XX_PC_CLIP_CNTL, 0);
    reg(s, A5XX_VFD_CONTROL_0, 0);
    regs(s, A5XX_VFD_CONTROL_1, {vs.vertexIdRegid | (vs.instanceIdRegid << 8) | 0xfc0000, 0xfcfc, 0xfcfc, 0xfc, 0});
    regs(s, A5XX_VFD_INDEX_OFFSET, {0, 0});
    reg(s, A5XX_PC_PRIMITIVE_CNTL, link.maxLoc);
    reg(s, A5XX_GRAS_CL_CNTL, 0x40); reg(s, A5XX_GRAS_SU_CNTL, 0x10);
    regs(s, A5XX_GRAS_SU_POLY_OFFSET_SCALE, {0, 0, 0});
    regs(s, A5XX_GRAS_CL_VPORT_XOFFSET_0, {bits(v.x + v.width / 2), bits(v.width / 2), bits(v.y + v.height / 2),
         bits(-v.height / 2), bits(v.znear), bits(v.zfar - v.znear)});
    regs(s, A5XX_GRAS_SC_VIEWPORT_SCISSOR_TL_0, {sc.x | (sc.y << 16), (sc.x + sc.width - 1) | ((sc.y + sc.height - 1) << 16)});
    for (const auto* c : {&vc, &fc}) append(s, *c);
    vt.emit(s, 0); ft.emit(s, 4);
    regs(s, A5XX_TPL1_VS_TEX_COUNT, {vt.numTextures, 0, 0, 0}); regs(s, A5XX_TPL1_FS_TEX_COUNT, {ft.numTextures, 0});
    packet(s, 0x38, {0x84, 1, d.vertexCount}); // DI_PT_TRILIST, AUTO_INDEX, IGNORE_VISIBILITY
    s.waitForIdle(); flush(s, 29, scratch + 4, 2); flush(s, 4, scratch + 8, 3);
    append(stream, s);
    return true;
}
} // namespace hakios::metal::render5
