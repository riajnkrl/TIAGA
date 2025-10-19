#!/usr/bin/env python3
import re
from pathlib import Path
p=Path('lib/main.dart')
s=p.read_text(encoding='utf-8')
lines=s.splitlines()
# find class start
start_idx=None
for i,l in enumerate(lines):
    if re.search(r"class\s+_PlantBotHomePageState\b", l):
        start_idx=i
        break
if start_idx is None:
    print('class not found')
    raise SystemExit(1)
print('class start line', start_idx+1)
# track braces from start_idx
bal=0
found_open=False
close_line=None
for i in range(start_idx, len(lines)):
    line=lines[i]
    if not found_open:
        if '{' in line:
            found_open=True
            bal += line.count('{') - line.count('}')
            if bal==0:
                close_line=i
                break
        continue
    else:
        bal += line.count('{') - line.count('}')
        if bal==0:
            close_line=i
            break
else:
    print('class never closes; final balance', bal)
# print a window around the closing line
if close_line is not None:
    for j in range(max(start_idx, close_line-6), min(len(lines), close_line+6)):
        print(f"{j+1:4}: {lines[j]}")
else:
    print('class never closes; final balance', bal)
