#!/usr/bin/env python3
"""Check include/hakios-metal/a5xx.hpp against Mesa's a5xx.xml.

Every `static constexpr uint32_t A5XX_<name> = 0x...u;  /* a5xx.xml */` line
must name a register the XML defines at that offset. A changed value or a
renamed register fails the port.
"""
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

XML = '../mesa-src/mesa-25.0.7/src/freedreno/registers/adreno/a5xx.xml'
HEADER = '../hakios-metal/include/hakios-metal/a5xx.hpp'

root = ET.parse(XML).getroot()
regs = {}
for el in root.iter():
    if el.tag.split('}')[-1] in ('reg32', 'reg64'):
        name, off = el.get('name', ''), el.get('offset')
        if name and off is not None:
            regs.setdefault(name, int(off, 0))

fail = 0
n = 0
for line in Path(HEADER).read_text().splitlines():
    m = __import__('re').search(
        r'static constexpr uint32_t A5XX_(\w+) = 0x([0-9a-fA-F]+)u;  /\* a5xx\.xml \*/', line)
    if not m:
        continue
    n += 1
    name, value = m.group(1), int(m.group(2), 16)
    if name not in regs:
        print(f'FAIL: A5XX_{name} is not in a5xx.xml')
        fail += 1
    elif regs[name] != value:
        print(f'FAIL: A5XX_{name} is 0x{value:x} in the header, 0x{regs[name]:x} in a5xx.xml')
        fail += 1
print(f'{n} constants checked, {fail} failures')
sys.exit(1 if (fail or n == 0) else 0)
