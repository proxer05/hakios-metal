#!/usr/bin/env python3
"""Generate include/hakios-metal/a5xx.hpp constants from Mesa's a5xx.xml.

The register names are gathered mechanically from Mesa's fd5 driver sources
(compute, emit, draw, gmem, program, blend, rasterizer, zsa, texture): every
REG_A5XX_<name> they reference, under Mesa's name, at the a5xx.xml offset.
tools/check-a5xx.py re-verifies the checked-in header against the XML.
"""
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

MESA = '../mesa-src/mesa-25.0.7'
FD5 = MESA + '/src/gallium/drivers/freedreno/a5xx'
XML = MESA + '/src/freedreno/registers/adreno/a5xx.xml'
OUT = '../hakios-metal/include/hakios-metal/a5xx.hpp'

SOURCES = ['fd5_compute.c', 'fd5_emit.c', 'fd5_draw.c', 'fd5_gmem.c',
           'fd5_program.c', 'fd5_blend.c', 'fd5_rasterizer.c', 'fd5_zsa.c',
           'fd5_texture.c', 'fd5_blitter.c', 'fd5_format.c']

root = ET.parse(XML).getroot()
regs = {}
for el in root.iter():
    if el.tag.split('}')[-1] in ('reg32', 'reg64'):
        name, off = el.get('name', ''), el.get('offset')
        if name and off is not None:
            regs.setdefault(name, int(off, 0))

names = set()
for src in SOURCES:
    text = (Path(FD5) / src).read_text()
    names |= set(re.findall(r'REG_A5XX_([A-Z0-9_]+)', text))

known = sorted(n for n in names if n in regs)
unknown = sorted(n for n in names if n not in regs)

lines = ['''/*
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

''']
for n in known:
    lines.append(f'static constexpr uint32_t A5XX_{n} = 0x{regs[n]:05x}u;  /* a5xx.xml */')
lines.append('\n}  // namespace a5xx\n')

Path(OUT).write_text('\n'.join(lines))
print(f'wrote {OUT}: {len(known)} constants ({len(unknown)} unnamed in XML: {unknown})')
if len(known) == 0:
    sys.exit(1)
