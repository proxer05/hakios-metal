#!/usr/bin/env python3
"""Generate include/hakios-metal/a5xx.hpp constants from Mesa's a5xx.xml.

Register names are gathered mechanically from Mesa's fd5 driver sources
(compute, emit, draw, gmem, program, blend, rasterizer, zsa, texture,
blitter, format): every REG_A5XX_<name> they reference, under Mesa's name,
at the a5xx.xml offset. rnndb arrays become A5XX_<ARRAY>(i) = base + stride*i.
tools/check-a5xx.py re-verifies the checked-in header against the XML.

Run from the hakios-metal tree:  python3 tools/gen-a5xx.py
MESA_SRC= overrides the Mesa source root.
"""
import os
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent          # the hakios-metal tree
UP = ROOT.parent                                        # upstream workspace
MESA = Path(os.environ.get('MESA_SRC', UP / 'mesa-src' / 'mesa-25.0.7'))
XML = MESA / 'src/freedreno/registers/adreno/a5xx.xml'
FD5 = MESA / 'src/gallium/drivers/freedreno/a5xx'
OUT = ROOT / 'include/hakios-metal/a5xx.hpp'

SOURCES = ['fd5_compute.c', 'fd5_emit.c', 'fd5_draw.c', 'fd5_gmem.c',
           'fd5_program.c', 'fd5_blend.c', 'fd5_rasterizer.c', 'fd5_zsa.c',
           'fd5_texture.c', 'fd5_blitter.c', 'fd5_format.c']

root = ET.parse(XML).getroot()
regs = {}
arrays = {}
for el in root.iter():
    tag = el.tag.split('}')[-1]
    name, off = el.get('name', ''), el.get('offset')
    if not name or off is None:
        continue
    if tag in ('reg32', 'reg64'):
        regs.setdefault(name, int(off, 0))
    elif tag == 'array':
        arrays.setdefault(name, (int(off, 0), int(el.get('stride', '1'), 0)))

names = set()
for src in SOURCES:
    text = (Path(FD5) / src).read_text()
    names |= set(re.findall(r'REG_A5XX_([A-Z0-9_]+)', text))

known = sorted(n for n in names if n in regs)
arrays_needed = sorted(n for n in names if n not in regs and n in arrays)
unknown = sorted(n for n in names if n not in regs and n not in arrays)

lines = ['''/*
 * MSM8953 / Adreno 506 (A5xx) registers, packets and descriptors, under
 * Mesa's names, each value the offset in Mesa's a5xx.xml (checked by
 * tools/check-a5xx.py). The names are gathered mechanically from Mesa's
 * fd5 driver sources; array registers are A5XX_<ARRAY>(i); UNKNOWN_* names
 * from the sources have no XML entry and are left out. The kgsl a5xx_reg.h
 * of the pinned daisy-o-oss kernel is the second witness. Not Cobalt
 * GPUCC/Fabia and not the A642L's a6xx.hpp.
 */
#pragma once

#include "pm4.hpp"

namespace a5xx {

''']
for n in known:
    lines.append(f'static constexpr uint32_t A5XX_{n} = 0x{regs[n]:05x}u;  /* a5xx.xml */')
for n in arrays_needed:
    base, stride = arrays[n]
    lines.append(f'static inline constexpr uint32_t A5XX_{n}(unsigned i) {{ return 0x{base:05x}u + 0x{stride:x}u * i; }}  /* a5xx.xml array */')
lines.append('\n}  // namespace a5xx\n')

OUT.write_text('\n'.join(lines))
print(f'wrote {OUT}: {len(known)} registers, {len(arrays_needed)} arrays '
      f'({len(unknown)} unnamed in XML: {unknown})')
if not known and not arrays_needed:
    sys.exit(1)
