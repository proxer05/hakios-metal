// A5xx descriptors against literal fd5/a5xx.xml words, including a second
// texture slot: a mistaken 16-word stride must not look like a passing test.
#include <cstring>
#include "hakios-metal/texture.hpp"
#include "hakios-metal/fake_gpu.hpp"
#include "test.hpp"
using namespace hakios::metal;
namespace {
class A506 : public FakeGpu {
    int call(uint32_t request, void* data, size_t size) override {
        int r = FakeGpu::call(request, data, size);
        if (!r && request == kGetParam && static_cast<MsmParam*>(data)->param == kParamChipId)
            static_cast<MsmParam*>(data)->value = 0x05000600;
        return r;
    }
};
}

TEST(a5xx_texture_tables_have_12_word_slots_and_fd5_swizzles) {
    Gpu gpu(std::make_unique<A506>());
    std::string why;
    TextureDesc d;
    d.width = 65; d.height = 33; d.format = PixelFormat::kBGRA8Unorm;
    auto bgra = Texture::make(gpu, d, &why);
    d.format = PixelFormat::kA8Unorm;
    auto alpha = Texture::make(gpu, d, &why);
    CHECK(bgra && alpha);
    if (!bgra || !alpha) return;
    CHECK_EQ(bgra->buffer()->length(), 32768u); // 512-byte rows, 64 padded rows
    a5xx::TexConst bc, ac;
    CHECK(bgra->sampledDescriptor5(&bc, &why));
    CHECK(alpha->sampledDescriptor5(&ac, &why));
    CHECK_EQ(bc.dword[0], 0x0c0060a0u); // TFMT5_8_8_8_8_UNORM + z,y,x,w
    CHECK_EQ(ac.dword[0], 0x00c01240u); // TFMT5_8_UNORM + zero,zero,zero,x
    CHECK_EQ(bc.dword[1], 0x00108041u);
    CHECK_EQ(bc.dword[2], 0x20010002u); // byte pitch 512, pitchalign log2(4)
    CHECK_EQ(bc.dword[3], 8u); // 32 KiB layer
    CHECK_EQ(bc.dword[5] >> 17, 1u);
    for (unsigned i = 6; i < 12; ++i) CHECK_EQ(bc.dword[i], 0u);
    Sampler s;
    SamplerDesc sd;
    sd.minFilter = sd.magFilter = 1;
    CHECK(makeSampler(sd, &s, &why));
    a5xx::TexSamp sc;
    CHECK(samplerDescriptor5(s, &sc, &why));
    CHECK_EQ(sc.dword[0], 0x92au); // linear + clamp-to-edge on S,T,R
    CHECK_EQ(sc.dword[1], 0x2000u); // non-mipmapped MAX_LOD=0.125
    Kernel k{};
    k.numTextureSlots = 3; k.samplerMask = 4;
    k.textures[0] = {kTexSampled, kTexDim2D, 0, kElemFloat, 0, 1, 0, 0, 0};
    k.textures[2] = k.textures[0];
    Bindings b;
    b.textures = {bgra.get(), nullptr, alpha.get()};
    b.samplers = {nullptr, nullptr, &s};
    TextureTables5 tables;
    CHECK(textureTables5(k, b, &tables, &why));
    pm4::Stream stream;
    tables.emit(stream, 5);
    const auto& w = stream.words();
    CHECK_EQ(w.size(), 56u); // 2 headers + 2*3 control + 3*4 + 3*12
    CHECK_EQ(w[1], 0x00d40000u); // SB4_CS_TEX=5, NUM_UNIT=3, SS4_DIRECT
    CHECK_EQ(w[2], 0u); // ST4_SHADER samplers
    CHECK_EQ(w[17], 0x00d40000u);
    CHECK_EQ(w[18], 1u); // ST4_CONSTANTS textures
    CHECK_EQ(w[20], 0x0c0060a0u);
    CHECK_EQ(w[32], 0u); // hole
    CHECK_EQ(w[44], 0x00c01240u);
    sd.minFilter = 2; // A6xx cubic cannot turn into A5xx anisotropic
    CHECK(makeSampler(sd, &s, &why));
    CHECK(!textureTables5(k, b, &tables, &why));
    CHECK(why.find("unsupported") != std::string::npos);
    d.mipLevels = 2;
    CHECK(!Texture::make(gpu, d, &why));
}
