import sys

with open('tools/raspi_gse_control/raspi_gse_control.py', 'r', encoding='utf-8') as f:
    lines = f.readlines()

out = []
for line in lines:
    if 'ledLimit' in line: continue
    if 'limitSwCh5Val' in line: continue
    if '<span class="mcu-label">ch5:</span>' in line: continue
    out.append(line)

with open('tools/raspi_gse_control/raspi_gse_control.py', 'w', encoding='utf-8') as f:
    f.writelines(out)
