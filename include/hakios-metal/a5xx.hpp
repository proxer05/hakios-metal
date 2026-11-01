/*
 * MSM8953 / Adreno 506 (A5xx) registers, packets and descriptors, under
 * Mesa's names, each value the offset in Mesa's a5xx.xml (checked by
 * tools/check-a5xx.py). The compute-dispatch slice first, from
 * fd5_compute.c's fd5_launch_grid; the draw and texture slices follow.
 * The kgsl a5xx_reg.h of the pinned daisy-o-oss kernel is the second
 * witness. Not Cobalt GPUCC/Fabia and not the A642L's a6xx.hpp.
 */
#pragma once

#include "pm4.hpp"

namespace a5xx {


static constexpr uint32_t A5XX_SP_SP_CNTL = 0x0e580u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CONTROL_0_REG = 0x0e784u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_CS_CTRL_REG0 = 0x0e5f0u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_CONFIG = 0x0e790u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_CNTL = 0x0e796u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_CS_CONFIG = 0x0e589u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_CONSTLEN = 0x0e7dcu;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_CS_OBJ_START_LO = 0x0e5f3u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_CS_OBJ_START_HI = 0x0e5f4u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_UPDATE_CNTL = 0x0e78au;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_CNTL_0 = 0x0e7b7u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_CNTL_1 = 0x0e7b8u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_NDRANGE_0 = 0x0e7b0u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_NDRANGE_1 = 0x0e7b1u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_NDRANGE_2 = 0x0e7b2u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_NDRANGE_3 = 0x0e7b3u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_NDRANGE_4 = 0x0e7b4u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_NDRANGE_5 = 0x0e7b5u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_NDRANGE_6 = 0x0e7b6u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_KERNEL_GROUP_X = 0x0e7b9u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_KERNEL_GROUP_Y = 0x0e7bau;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_KERNEL_GROUP_Z = 0x0e7bbu;  /* a5xx.xml */

}  // namespace a5xx
