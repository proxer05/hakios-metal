#!/usr/bin/env python3
"""Check include/hakios-metal/a5xx.hpp against Mesa's a5xx.xml.

Every `static constexpr uint32_t A5XX_<name> = 0x...u;  /* a5xx.xml */` line
must name a register the XML defines at that offset, and every
`static inline constexpr uint32_t A5XX_<ARRAY>(i) { return 0x...u + 0x...u * i; }`
must name an array with that base and stride. A changed value or a renamed
register fails the port. Run from the hakios-metal tree.
"""
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
UP = ROOT.parent
XML = UP / 'mesa-src/mesa-25.0.7/src/freedreno/registers/adreno/a5xx.xml'
HEADER = ROOT / 'include/hakios-metal/a5xx.hpp'

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
        for nested in el.iter():
            ntag = nested.tag.split('}')[-1]
            if ntag in ('reg32', 'reg64'):
                nname, noff = nested.get('name', ''), nested.get('offset')
                if nname and noff is not None:
                    regs.setdefault(f'{name}_{nname}', int(off, 0) + int(noff, 0))

fail = 0
n = 0
for line in HEADER.read_text().splitlines():
    m = re.search(r'static constexpr uint32_t A5XX_(\w+) = 0x([0-9a-fA-F]+)u;  /\* a5xx\.xml \*/', line)
    if m:
        n += 1
        name, value = m.group(1), int(m.group(2), 16)
        if name not in regs:
            print(f'FAIL: A5XX_{name} is not in a5xx.xml')
            fail += 1
        elif regs[name] != value:
            print(f'FAIL: A5XX_{name} is 0x{value:x} in the header, 0x{regs[name]:x} in a5xx.xml')
            fail += 1
        continue
    m = re.search(
        r'static inline constexpr uint32_t A5XX_(\w+)\(unsigned i\) \{ return 0x([0-9a-fA-F]+)u \+ 0x([0-9a-fA-F]+)u \* i; \}  /\* a5xx\.xml array \*/',
        line)
    if m:
        n += 1
        name, base, stride = m.group(1), int(m.group(2), 16), int(m.group(3), 16)
        if name not in arrays:
            print(f'FAIL: A5XX_{name}(i) is not an a5xx.xml array')
            fail += 1
        elif arrays[name] != (base, stride):
            print(f'FAIL: A5XX_{name} array base/stride mismatch: '
                  f'0x{base:x}/0x{stride:x} vs 0x{arrays[name][0]:x}/0x{arrays[name][1]:x}')
            fail += 1

print(f'{n} constants checked, {fail} failures')
sys.exit(1 if (fail or n == 0) else 0)
