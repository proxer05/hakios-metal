// The a5xx dispatch against the fake GPU: the well-formed stream runs, and every mutation twin of the dispatch
// contract faults (the a6xx tests' discipline). The stream's layout is the one compute5_test holds: 40 words.
#include <vector>

#include "hakios-metal/compute5.hpp"
#include "hakios-metal/fake_gpu5.hpp"
#include "test.hpp"

using hakios::metal::a5xx::Dispatch;
using hakios::metal::a5xx::FakeGpu5;

namespace {

struct Stream {
    std::vector<uint32_t> words;

    explicit Stream(bool preloaded = true) {
        Dispatch d;
        d.cntl.instrlen = preloaded ? 3 : 0;   // 3 units of 32*16 instructions
        d.cntl.hasSsbo = false;
        d.constlen = 1;
        d.csCtrl0.preload = preloaded;
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
        d.emit(words);
    }

    std::vector<uint32_t> mutated(size_t word, uint32_t value) const {
        std::vector<uint32_t> out = words;
        out[word] = value;
        return out;
    }
};

// The layout compute5_test holds, for the mutations.
enum Word {
    kHlsqControl0 = 3,
    kSpCsCtrl = 5,
    kHlsqCsConfig = 7,
    kHlsqCsCntl = 9,
    kSpCsConfig = 11,
    kHlsqCsConstlen = 13,
    kHlsqCsInstrlen = 14,
    kShaderObjLo = 16,
    kKernelGroupX = 36,
    kKernelGroupY = 37,
    kKernelGroupZ = 38,
    kExecCs = 39,
    kNgroupsX = 41,
};

}  // namespace

// Each submission gets a fresh GPU: faults() counts a whole rig's submissions.
static void
submitExpect(const std::vector<uint32_t> &words, bool expectFault)
{
    FakeGpu5 gpu;
    gpu.attach(0xa0000000ull, 0x100000ull);
    gpu.attach(0xb0000000ull, 0x1000ull);    // a second buffer, unused
    gpu.submit(words);
    if ((gpu.faults() != 0u) != expectFault) {
        for (const char *why : gpu.sentences()) {
            std::fprintf(stderr, "  fault: %s\n", why);
        }
    }
    CHECK_EQ(gpu.faults() != 0u, expectFault);
}

TEST(a5xx_dispatch_well_formed_runs)
{
    Stream s(true);
    submitExpect(s.words, false);
    Stream s0(false);
    submitExpect(s0.words, false);
}

TEST(a5xx_dispatch_config_copies_agree)
{
    Stream s;
    submitExpect(s.mutated(kSpCsConfig, 0), true);           // SP side disabled, HLSQ side enabled
    submitExpect(s.mutated(kHlsqCsConfig, 0), true);         // HLSQ side disabled
    submitExpect(s.mutated(kHlsqCsConfig, 0x12u), true);     // equal but not ENABLED
}

TEST(a5xx_dispatch_thread_size_agrees)
{
    Stream s;
    submitExpect(s.mutated(kSpCsCtrl, s.words[kSpCsCtrl] ^ (1u << 3)), true);
    submitExpect(s.mutated(kHlsqControl0, s.words[kHlsqControl0] ^ (1u << 2)), true);
}

TEST(a5xx_dispatch_buffer_matches_instrlen)
{
    Stream s;                                                // preloaded: BUFFER set, INSTRLEN 3
    submitExpect(s.mutated(kSpCsCtrl, s.words[kSpCsCtrl] & ~(1u << 2)), true);   // BUFFER clear, instrlen != 0
    submitExpect(s.mutated(kHlsqCsCntl, s.words[kHlsqCsCntl] & ~0xfffffffeu), true);  // INSTRLEN 0, BUFFER set
    submitExpect(s.mutated(kHlsqCsInstrlen, 0), true);       // the CONSTLEN pair disagrees with HLSQ_CS_CNTL
}

TEST(a5xx_dispatch_shader_object_is_addressed)
{
    Stream s;
    submitExpect(s.mutated(kShaderObjLo, 0xdeadbee0u), true);  // no buffer there
}

TEST(a5xx_dispatch_state_loads)
{
    // constants, a buffer-backed constant load, UBO pointers and device-buffer state, then the dispatch.
    Stream s;
    std::vector<uint32_t> head(s.words.begin(), s.words.begin() + 15);  // everything before SP_CS_OBJ_START
    std::vector<uint32_t> tail(s.words.begin() + 15, s.words.end());
    std::vector<uint32_t> full = head;
    std::vector<uint32_t> consts = {1, 2, 3, 4, 5, 6, 7, 8};
    hakios::metal::a5xx::loadConstantsDirect(full, 0, consts.data(), 8);
    hakios::metal::a5xx::loadConstantsBuffer(full, 64, 0xb0000000ull, 256, 8);
    uint64_t ubos[2] = {0xb0000000ull, 0xb0010000ull};
    hakios::metal::a5xx::loadUboPointers(full, 128, ubos, 2);
    uint64_t addrs[2] = {0xb0000000ull, 0xb0020000ull};
    uint64_t sizes[2] = {4096, 8192};
    hakios::metal::a5xx::loadSsbos(full, addrs, sizes, 2);
    full.insert(full.end(), tail.begin(), tail.end());

    FakeGpu5 gpu;
    gpu.attach(0xa0000000ull, 0x100000ull);
    gpu.attach(0xb0000000ull, 0x100000ull);
    gpu.submit(full);
    if (gpu.faults() != 0) {
        for (const char *why : gpu.sentences()) {
            std::fprintf(stderr, "  fault: %s\n", why);
        }
    }
    CHECK_EQ(gpu.faults(), 0u);

    // a direct constants load whose NUM_UNIT disagrees with its word count
    std::vector<uint32_t> bad = full;
    bad[16] = hakios::metal::a5xx::loadState4Word0(0, hakios::metal::a5xx::kSrcDirect,
        hakios::metal::a5xx::kBlockCsShader, 50);
    FakeGpu5 gpu2;
    gpu2.attach(0xa0000000ull, 0x100000ull);
    gpu2.attach(0xb0000000ull, 0x100000ull);
    gpu2.submit(bad);
    CHECK(gpu2.faults() != 0);
}

TEST(a5xx_dispatch_grid_consistency)
{
    Stream s;
    submitExpect(s.mutated(kNgroupsX, 5), true);             // GLOBALSIZE_X says 128, 5 * 64 is not 128
    submitExpect(s.mutated(kKernelGroupX, 2), true);
    submitExpect(s.mutated(kKernelGroupY, 0), true);
}

TEST(a5xx_dispatch_stream_discipline)
{
    Stream s;
    std::vector<uint32_t> broken = s.words;
    broken[kExecCs] ^= 0x10000u;                             // an opcode the GPU does not know
    submitExpect(broken, true);
    submitExpect(s.mutated(kExecCs, s.words[kExecCs] & ~0x74u), true);   // a type-7 header with broken parity
    std::vector<uint32_t> after = s.words;
    after.push_back(hakios::metal::pm4::type4(0x0e580u, 1));
    after.push_back(0);                                      // work after the dispatch
    submitExpect(after, true);
    std::vector<uint32_t> over(s.words);
    over[kExecCs] = hakios::metal::a5xx::type7(hakios::metal::a5xx::kExecCs, 9);  // count past the end
    submitExpect(over, true);
    FakeGpu5 gpu;
    gpu.attach(0xa0000000ull, 0x100000ull);
    gpu.submit(std::vector<uint32_t>{});                     // nothing at all
    CHECK_EQ(gpu.faults(), 1u);
}
