// A strict interpreter for the A5xx compute dispatch's stream, the fake_gpu.hpp idea for the Adreno 506: every
// packet header must decode (its parity holds), a type-4 count must not run past the end, a type-7 opcode must be
// one this GPU knows, and the dispatch contract must hold when CP_EXEC_CS runs:
//
//   - SP_CS_CONFIG and HLSQ_CS_CONFIG both ENABLED and equal (the two copies of the id registers agree);
//   - the thread size the same in SP_CS_CTRL_REG0 and HLSQ_CONTROL_0_REG (their bit 3 and bit 2);
//   - SP_CS_CTRL_REG0's BUFFER bit is set exactly when HLSQ_CS_CNTL's INSTRLEN is not 0 (fd5 preloads through
//     the buffer and states the length together, or neither);
//   - HLSQ_CS_CNTL's INSTRLEN the same as the HLSQ_CS_INSTRLEN of the CONSTLEN pair;
//   - SP_CS_OBJ_START inside a buffer the GPU was given;
//   - HLSQ_CS_KERNEL_GROUP_X/Y/Z are 1 (fd5 writes ones);
//   - the GLOBALSIZE of NDRANGE is the work-group size times the NGROUPS of CP_EXEC_CS;
//   - nothing follows CP_EXEC_CS in the submission.
//
// It runs no shader instructions: the memory a dispatch would write stays as it was. Whether the GPU honours these
// words is the phone's to prove, with hakios-metal-probe.
#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "hakios-metal/a5xx.hpp"
#include "hakios-metal/pm4.hpp"

namespace hakios::metal::a5xx {

class FakeGpu5 {
public:
    void attach(uint64_t address, uint64_t size) {
        buffers_.push_back({address, size});
    }

    void submit(const std::vector<uint32_t> &words) {
        faulted_ = false;
        regs_.clear();
        size_t i = 0;
        bool executed = false;
        while (i < words.size()) {
            if (executed) {
                fault(i, "work after the dispatch in one submission");
                return;
            }
            pm4::Header h = pm4::decode(words[i]);
            if (h.kind == pm4::Header::kBad) {
                fault(i, "a header whose parity does not hold");
                return;
            }
            size_t body = i + 1;
            if (body + h.count > words.size()) {
                fault(i, "a count that runs past the end of the submission");
                return;
            }
            if (h.kind == pm4::Header::kRegisters) {
                for (uint32_t r = 0; r < h.count; r++) {
                    regs_[h.reg + r] = words[body + r];
                }
            } else if (h.opcode == static_cast<uint32_t>(pm4::Opcode::kNop)) {
                ;                                        // the words that follow are skipped
            } else if (h.opcode == static_cast<uint32_t>(kLoadState4)) {
                /* fd5_emit_const_user/bo: word 0 names the block, source and units; a direct load carries
                 * NUM_UNIT*4 more words, an indirect one names a buffer in words 1/2. */
                if (h.count < 3) {
                    fault(i, "a CP_LOAD_STATE4 shorter than its header");
                    return;
                }
                uint32_t w0 = words[body];
                uint32_t numUnit = (w0 >> 22) & 0x3ffu;
                uint32_t src = (w0 >> 16) & 3u;
                uint32_t block = (w0 >> 18) & 0xfu;
                /* constants and UBO-pointer tables count vec4 units (four words); the SSBO
                 * state counts two-word entries (fd5_emit_ssbos writes two words per buffer) */
                uint32_t unitWords = block == kBlockCsSsbo ? 2u : 4u;
                if (src == kSrcDirect && h.count != 3 + numUnit * unitWords) {
                    fault(i, "a direct state load whose count does not match NUM_UNIT");
                    return;
                }
                if (src != kSrcDirect && src != kSrcIndirect) {
                    fault(i, "a state load from a source this GPU does not know");
                    return;
                }
            } else if (h.opcode == static_cast<uint32_t>(kExecCs)) {
                if (h.count != 4) {
                    fault(i, "a CP_EXEC_CS that is not four words");
                    return;
                }
                if (executed) {
                    fault(i, "a second CP_EXEC_CS in one submission");
                    return;
                }
                executed = true;
                checkDispatch(words[body + 1], words[body + 2], words[body + 3]);
                if (faulted_) {
                    return;
                }
            } else {
                fault(i, "an opcode this GPU does not know");
                return;
            }
            i = body + h.count;
        }
        if (!executed) {
            fault(words.size() - 1, "a submission that never ran a dispatch");
        }
    }

    uint32_t faults() const { return faults_; }
    const std::vector<const char *> &sentences() const { return sentences_; }

private:
    struct Span { uint64_t address, size; };

    void fault(size_t word, const char *why) {
        (void)word;
        faults_++;
        faulted_ = true;
        last_ = why;
        sentences_.push_back(why);
    }

    uint32_t reg(uint32_t address) const {
        auto it = regs_.find(address);
        return it != regs_.end() ? it->second : 0;
    }

    bool attached(uint64_t address) const {
        for (const Span &s : buffers_) {
            if (address >= s.address && address < s.address + s.size) {
                return true;
            }
        }
        return false;
    }

    void checkDispatch(uint32_t ngroupsX, uint32_t ngroupsY, uint32_t ngroupsZ) {
        if (reg(A5XX_SP_CS_CONFIG) != reg(A5XX_HLSQ_CS_CONFIG)) {
            fault(0, "SP_CS_CONFIG and HLSQ_CS_CONFIG disagree");
        } else if (!(reg(A5XX_SP_CS_CONFIG) & CsConfig::kEnabled)) {
            fault(0, "a dispatch whose config is not ENABLED");
        }
        uint32_t ctrl = reg(A5XX_SP_CS_CTRL_REG0);
        if (((ctrl >> 3) & 1u) != ((reg(A5XX_HLSQ_CONTROL_0_REG) >> 2) & 1u)) {
            fault(0, "the thread size differs between SP_CS_CTRL_REG0 and HLSQ_CONTROL_0_REG");
        }
        uint32_t instrlen = (reg(A5XX_HLSQ_CS_CNTL) >> 1);
        bool buffered = (ctrl >> 2) & 1u;
        /* fd5: BUFFER is set exactly when the shader preloads (instrlen != 0); a shader longer than the
         * 32*16-instruction limit gets instrlen 0 and no BUFFER. */
        if (buffered != (instrlen != 0)) {
            fault(0, "BUFFER and INSTRLEN disagree about the preload");
        }
        if (reg(A5XX_HLSQ_CS_CONSTLEN + 1) != instrlen) {
            fault(0, "HLSQ_CS_INSTRLEN and HLSQ_CS_CNTL's INSTRLEN disagree");
        }
        /* the OBJ_START pair is adjacent (a5xx.xml 0xe5f3/0xe5f4); fd5 writes it as one two-register packet */
        uint64_t start = reg(A5XX_SP_CS_OBJ_START_LO) |
                         (static_cast<uint64_t>(reg(A5XX_SP_CS_OBJ_START_LO + 1)) << 32);
        if (!attached(start)) {
            fault(0, "SP_CS_OBJ_START names no buffer the GPU was given");
        }
        if (reg(A5XX_HLSQ_CS_KERNEL_GROUP_X) != 1 || reg(A5XX_HLSQ_CS_KERNEL_GROUP_X + 1) != 1 ||
            reg(A5XX_HLSQ_CS_KERNEL_GROUP_X + 2) != 1) {
            fault(0, "KERNEL_GROUP is not 1,1,1");
        }
        uint32_t nd0 = reg(A5XX_HLSQ_CS_NDRANGE_0);
        uint32_t localX = ((nd0 >> 2) & 0x3ffu) + 1;
        uint32_t localY = ((nd0 >> 12) & 0x3ffu) + 1;
        uint32_t localZ = ((nd0 >> 22) & 0x3ffu) + 1;
        if (reg(A5XX_HLSQ_CS_NDRANGE_0 + 1) != localX * ngroupsX ||
            reg(A5XX_HLSQ_CS_NDRANGE_0 + 3) != localY * ngroupsY ||
            reg(A5XX_HLSQ_CS_NDRANGE_0 + 5) != localZ * ngroupsZ) {
            fault(0, "GLOBALSIZE is not the work-group size times NGROUPS");
        }
    }

    std::vector<Span> buffers_;
    std::map<uint32_t, uint32_t> regs_;
    uint32_t faults_ = 0;
    bool faulted_ = false;
    std::vector<const char *> sentences_;

public:
    const char *last_ = nullptr;    // the last fault's sentence, for the tests
};

}  // namespace hakios::metal::a5xx
