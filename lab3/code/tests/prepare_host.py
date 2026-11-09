"""Extract the delivered kernel function verbatim; do not maintain a test copy."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
source = (root / 'code/kern/trap/trap.c').read_text(encoding='utf-8')
start = source.index('void exception_handler(struct trapframe *tf) {')
end = start
depth = 0
for end in range(source.index('{', start), len(source)):
    if source[end] == '{': depth += 1
    if source[end] == '}': depth -= 1
    if depth == 0: break
macros = (root / 'code/libs/riscv.h').read_text(encoding='utf-8')
defines = '\n'.join(re.findall(r'^#define CAUSE_(?:ILLEGAL_INSTRUCTION|BREAKPOINT)\s+\S+', macros, re.M))
assert defines.count('#define') == 2
(root / 'tests/handler_under_test.inc').write_text(defines + '\n' + source[start:end+1] + '\n', encoding='utf-8')
