/*
 * MSM8953 / Adreno 506 (A5xx) registers, packets and descriptors, under
 * Mesa's names, each value the offset in Mesa's a5xx.xml (checked by
 * tools/check-a5xx.py). The names are gathered mechanically from Mesa's
 * fd5 driver sources (compute, emit, draw, gmem, program, blend,
 * rasterizer, zsa, texture); UNKNOWN_* names from the sources have no XML
 * entry and are left out. The kgsl a5xx_reg.h of the pinned daisy-o-oss
 * kernel is the second witness. Not Cobalt GPUCC/Fabia and not the
 * A642L's a6xx.hpp.
 */
#pragma once

#include "pm4.hpp"

namespace a5xx {


static constexpr uint32_t A5XX_GRAS_2D_BLIT_CNTL = 0x02180u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_2D_DST_INFO = 0x02182u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_2D_SRC_INFO = 0x02181u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_CL_CNTL = 0x0e000u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_CL_VPORT_XOFFSET_0 = 0x0e010u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_CNTL = 0x0e005u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_LRZ_BUFFER_BASE_LO = 0x0e101u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_LRZ_CNTL = 0x0e100u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_LRZ_FAST_CLEAR_BUFFER_BASE_LO = 0x0e104u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_SC_BIN_CNTL = 0x0e0a1u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_SC_RAS_MSAA_CNTL = 0x0e0a2u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_SC_SCREEN_SCISSOR_CNTL = 0x0e0a4u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_SC_SCREEN_SCISSOR_TL_0 = 0x0e0aau;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_SC_VIEWPORT_SCISSOR_TL_0 = 0x0e0cau;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_SC_WINDOW_SCISSOR_TL = 0x0e0eau;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_SU_CNTL = 0x0e090u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_SU_CONSERVATIVE_RAS_CNTL = 0x0e099u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_SU_DEPTH_BUFFER_INFO = 0x0e098u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_SU_DEPTH_PLANE_CNTL = 0x0e094u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_SU_LAYERED = 0x0e093u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_SU_POINT_MINMAX = 0x0e091u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_SU_POLY_OFFSET_SCALE = 0x0e095u;  /* a5xx.xml */
static constexpr uint32_t A5XX_GRAS_VS_CL_CNTL = 0x0e001u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CONTROL_0_REG = 0x0e784u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_CNTL = 0x0e796u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_CNTL_0 = 0x0e7b7u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_CONFIG = 0x0e790u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_CONSTLEN = 0x0e7dcu;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_KERNEL_GROUP_X = 0x0e7b9u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_CS_NDRANGE_0 = 0x0e7b0u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_DBG_ECO_CNTL = 0x00e04u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_DS_CONSTLEN = 0x0e7cdu;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_FS_CONSTLEN = 0x0e7d7u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_GS_CONSTLEN = 0x0e7d2u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_HS_CONSTLEN = 0x0e7c8u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_MODE_CNTL = 0x00e06u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_TIMEOUT_THRESHOLD_0 = 0x00e00u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_UPDATE_CNTL = 0x0e78au;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_VS_CNTL = 0x0e791u;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_VS_CONFIG = 0x0e78bu;  /* a5xx.xml */
static constexpr uint32_t A5XX_HLSQ_VS_CONSTLEN = 0x0e7c3u;  /* a5xx.xml */
static constexpr uint32_t A5XX_PC_CLIP_CNTL = 0x0e389u;  /* a5xx.xml */
static constexpr uint32_t A5XX_PC_GS_LAYERED = 0x0e38du;  /* a5xx.xml */
static constexpr uint32_t A5XX_PC_GS_PARAM = 0x0e38eu;  /* a5xx.xml */
static constexpr uint32_t A5XX_PC_HS_PARAM = 0x0e38fu;  /* a5xx.xml */
static constexpr uint32_t A5XX_PC_MODE_CNTL = 0x00d02u;  /* a5xx.xml */
static constexpr uint32_t A5XX_PC_POWER_CNTL = 0x0e3b0u;  /* a5xx.xml */
static constexpr uint32_t A5XX_PC_PRIMITIVE_CNTL = 0x0e384u;  /* a5xx.xml */
static constexpr uint32_t A5XX_PC_PRIM_VTX_CNTL = 0x0e385u;  /* a5xx.xml */
static constexpr uint32_t A5XX_PC_RASTER_CNTL = 0x0e388u;  /* a5xx.xml */
static constexpr uint32_t A5XX_PC_RESTART_INDEX = 0x0e38cu;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_2D_BLIT_CNTL = 0x02100u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_2D_DST_INFO = 0x02110u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_2D_SRC_INFO = 0x02107u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_ALPHA_CONTROL = 0x0e1a8u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_BLEND_CNTL = 0x0e1a9u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_BLEND_RED = 0x0e1a0u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_BLIT_CNTL = 0x0e210u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_BLIT_FLAG_DST_LO = 0x0e263u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_CCU_CNTL = 0x00cc7u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_CLEAR_CNTL = 0x0e21cu;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_CLEAR_COLOR_DW0 = 0x0e218u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_CNTL = 0x0e140u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_DBG_ECO_CNTL = 0x00cc4u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_DEPTH_BUFFER_INFO = 0x0e1b2u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_DEPTH_CNTL = 0x0e1b1u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_DEPTH_FLAG_BUFFER_BASE_LO = 0x0e240u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_DEPTH_PLANE_CNTL = 0x0e1b0u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_DEST_MSAA_CNTL = 0x0e143u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_FS_OUTPUT_CNTL = 0x0e146u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_MODE_CNTL = 0x00cc6u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_RAS_MSAA_CNTL = 0x0e142u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_RENDER_CNTL = 0x0e141u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_RENDER_COMPONENTS = 0x0e147u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_RENDER_CONTROL0 = 0x0e144u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_RESOLVE_CNTL_1 = 0x0e211u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_RESOLVE_CNTL_3 = 0x0e213u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_STENCILREFMASK = 0x0e1c6u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_STENCIL_CONTROL = 0x0e1c0u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_STENCIL_INFO = 0x0e1c1u;  /* a5xx.xml */
static constexpr uint32_t A5XX_RB_WINDOW_OFFSET = 0x0e1d0u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_BLEND_CNTL = 0x0e5c9u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_CS_CONFIG = 0x0e589u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_CS_CTRL_REG0 = 0x0e5f0u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_CS_OBJ_START_LO = 0x0e5f3u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_DBG_ECO_CNTL = 0x00ec0u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_FS_CONFIG_MAX_CONST = 0x0e58bu;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_FS_CTRL_REG0 = 0x0e5c0u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_FS_OBJ_START_LO = 0x0e5c3u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_FS_OUTPUT_CNTL = 0x0e5cau;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_GS_CTRL_REG0 = 0x0e640u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_HS_CTRL_REG0 = 0x0e600u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_MODE_CNTL = 0x00ec2u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_PRIMITIVE_CNTL = 0x0e592u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_SP_CNTL = 0x0e580u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_VS_CONFIG = 0x0e584u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_VS_CONFIG_MAX_CONST = 0x0e58au;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_VS_CTRL_REG0 = 0x0e590u;  /* a5xx.xml */
static constexpr uint32_t A5XX_SP_VS_OBJ_START_LO = 0x0e5acu;  /* a5xx.xml */
static constexpr uint32_t A5XX_TPL1_CS_TEX_COUNT = 0x0e751u;  /* a5xx.xml */
static constexpr uint32_t A5XX_TPL1_DS_TEX_COUNT = 0x0e702u;  /* a5xx.xml */
static constexpr uint32_t A5XX_TPL1_FS_TEX_COUNT = 0x0e750u;  /* a5xx.xml */
static constexpr uint32_t A5XX_TPL1_GS_TEX_COUNT = 0x0e703u;  /* a5xx.xml */
static constexpr uint32_t A5XX_TPL1_HS_TEX_COUNT = 0x0e701u;  /* a5xx.xml */
static constexpr uint32_t A5XX_TPL1_MODE_CNTL = 0x00f02u;  /* a5xx.xml */
static constexpr uint32_t A5XX_TPL1_TP_BORDER_COLOR_BASE_ADDR_LO = 0x0e706u;  /* a5xx.xml */
static constexpr uint32_t A5XX_TPL1_TP_FS_ROTATION_CNTL = 0x0e764u;  /* a5xx.xml */
static constexpr uint32_t A5XX_TPL1_TP_RAS_MSAA_CNTL = 0x0e704u;  /* a5xx.xml */
static constexpr uint32_t A5XX_TPL1_VS_TEX_COUNT = 0x0e700u;  /* a5xx.xml */
static constexpr uint32_t A5XX_UNKNOWN_0BC5 = 0x00bc5u;  /* a5xx.xml */
static constexpr uint32_t A5XX_UNKNOWN_2184 = 0x02184u;  /* a5xx.xml */
static constexpr uint32_t A5XX_UNKNOWN_E004 = 0x0e004u;  /* a5xx.xml */
static constexpr uint32_t A5XX_UNKNOWN_E292 = 0x0e292u;  /* a5xx.xml */
static constexpr uint32_t A5XX_UNKNOWN_E5AB = 0x0e5abu;  /* a5xx.xml */
static constexpr uint32_t A5XX_UNKNOWN_E5C2 = 0x0e5c2u;  /* a5xx.xml */
static constexpr uint32_t A5XX_UNKNOWN_E5DB = 0x0e5dbu;  /* a5xx.xml */
static constexpr uint32_t A5XX_UNKNOWN_E7C0 = 0x0e7c0u;  /* a5xx.xml */
static constexpr uint32_t A5XX_UNKNOWN_E7C5 = 0x0e7c5u;  /* a5xx.xml */
static constexpr uint32_t A5XX_UNKNOWN_E7CA = 0x0e7cau;  /* a5xx.xml */
static constexpr uint32_t A5XX_UNKNOWN_E7CF = 0x0e7cfu;  /* a5xx.xml */
static constexpr uint32_t A5XX_UNKNOWN_E7D4 = 0x0e7d4u;  /* a5xx.xml */
static constexpr uint32_t A5XX_UNKNOWN_E7D9 = 0x0e7d9u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VFD_CONTROL_0 = 0x0e400u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VFD_CONTROL_1 = 0x0e401u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VFD_INDEX_OFFSET = 0x0e408u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VFD_MODE_CNTL = 0x00e42u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VFD_POWER_CNTL = 0x0e4f0u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VPC_CLIP_CNTL = 0x0e29au;  /* a5xx.xml */
static constexpr uint32_t A5XX_VPC_CNTL_0 = 0x0e280u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VPC_DBG_ECO_CNTL = 0x00e60u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VPC_FS_PRIMITIVEID_CNTL = 0x0e2a0u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VPC_GS_SIV_CNTL = 0x0e298u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VPC_MODE_CNTL = 0x00e62u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VPC_PACK = 0x0e29du;  /* a5xx.xml */
static constexpr uint32_t A5XX_VPC_SO_BUF_CNTL = 0x0e2a1u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VPC_SO_CNTL = 0x0e2a3u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VPC_SO_OVERRIDE = 0x0e2a2u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VPC_SO_PROG = 0x0e2a4u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VSC_BIN_SIZE = 0x00bc2u;  /* a5xx.xml */
static constexpr uint32_t A5XX_VSC_RESOLVE_CNTL = 0x00cddu;  /* a5xx.xml */

}  // namespace a5xx
