#!/usr/bin/env python3
"""Generate include/hakios-metal/a5xx.hpp constants from Mesa's a5xx.xml.

Each A5XX_<name> constant is the register offset under Mesa's name, like the
a6xx.hpp discipline. Regenerated from Mesa's a5xx.xml;
tools/check-a5xx.py re-verifies the checked-in header against the XML.
"""
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

XML = '../mesa-src/mesa-25.0.7/src/freedreno/registers/adreno/a5xx.xml'
OUT = '../hakios-metal/include/hakios-metal/a5xx.hpp'

WANTED = [
    'SP_SP_CNTL', 'HLSQ_CONTROL_0_REG', 'SP_CS_CTRL_REG0', 'HLSQ_CS_CONFIG',
    'HLSQ_CS_CNTL', 'SP_CS_CONFIG', 'HLSQ_CS_CONSTLEN', 'SP_CS_OBJ_START_LO',
    'SP_CS_OBJ_START_HI', 'HLSQ_UPDATE_CNTL', 'HLSQ_CS_CNTL_0', 'HLSQ_CS_CNTL_1',
    'HLSQ_CS_NDRANGE_0', 'HLSQ_CS_NDRANGE_1', 'HLSQ_CS_NDRANGE_2',
    'HLSQ_CS_NDRANGE_3', 'HLSQ_CS_NDRANGE_4', 'HLSQ_CS_NDRANGE_5',
    'HLSQ_CS_NDRANGE_6', 'HLSQ_CS_KERNEL_GROUP_X', 'HLSQ_CS_KERNEL_GROUP_Y',
    'HLSQ_CS_KERNEL_GROUP_Z',
]

root = ET.parse(XML).getroot()
regs = {}
for el in root.iter():
    tag = el.tag.split('}')[-1]
    if tag in ('reg32', 'reg64'):
        name, off = el.get('name', ''), el.get('offset')
        if name and off is not None:
            regs.setdefault(name, int(off, 0))

lines = ['''/*
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

''']

missing = []
for w in WANTED:
    if w in regs:
        lines.append(f'static constexpr uint32_t A5XX_{w} = 0x{regs[w]:05x}u;  /* a5xx.xml */')
    else:
        missing.append(w)
lines.append('\n}  // namespace a5xx\n')

Path(OUT).write_text('\n'.join(lines))
print(f'wrote {OUT}: {len(WANTED) - len(missing)} constants')
if missing:
    print('MISSING from XML:', missing)
    sys.exit(1)
