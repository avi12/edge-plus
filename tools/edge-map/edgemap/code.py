# Searching a function's instructions: the call to a named function, an
# instruction matching a pattern, the jump into a function's cold part. Each
# instruction is matched as capstone prints it, "mnemonic op_str", e.g.
# "mov rax, qword ptr [rcx + 0x368]".
import re
from dataclasses import dataclass

from capstone import CS_ARCH_X86, CS_MODE_64, CS_OP_IMM, CS_OP_MEM, Cs, x86

from .image import IMAGE_BASE

# The most a function is read when the PDB gives it no size.
DEFAULT_LIMIT = 0x4000
_disassembler = Cs(CS_ARCH_X86, CS_MODE_64)
_disassembler.detail = True


@dataclass
class Instruction:
    rva: int
    size: int
    text: str
    target: int | None      # a direct call / jump target, or a rip-relative operand's
    imm_offset: int         # where the immediate starts within the bytes (0 = none)
    disp_offset: int        # where the displacement starts (0 = none)

    @property
    def end(self):
        return self.rva + self.size

    @property
    def mnemonic(self):
        return self.text.split(" ", 1)[0]


def decode(image, start, length):
    code = image.read(start, length)
    instructions = []
    for raw in _disassembler.disasm(code, IMAGE_BASE + start):
        target = None
        for operand in raw.operands:
            if operand.type == CS_OP_IMM and (raw.mnemonic == "call" or raw.mnemonic.startswith("j")):
                target = operand.imm - IMAGE_BASE
            if operand.type == CS_OP_MEM and operand.mem.base == x86.X86_REG_RIP:
                target = raw.address + raw.size + operand.mem.disp - IMAGE_BASE
        text = f"{raw.mnemonic} {raw.op_str}".strip()
        instructions.append(Instruction(raw.address - IMAGE_BASE, raw.size, text, target,
                                        raw.imm_offset, raw.disp_offset))
    return instructions


class Region:
    """The instructions of one stretch of code, searched by a cursor."""

    def __init__(self, image, symbols, start, limit=DEFAULT_LIMIT):
        self.image = image
        self.symbols = symbols
        self.start = start
        length = symbols.extent(start, limit)
        self.instructions = decode(image, start, length)
        self.cursor = -1  # before the first instruction

    def _scan(self, predicate, nth, backward):
        step = -1 if backward else 1
        index = self.cursor + step if self.cursor >= 0 or not backward else len(self.instructions) - 1
        seen = 0
        while 0 <= index < len(self.instructions):
            if predicate(self.instructions[index]):
                seen += 1
                if seen == nth:
                    self.cursor = index
                    return self.instructions[index]
            index += step
        return None

    def find_call(self, callee_rvas, nth=1, backward=False, jumps_too=False):
        callees = set(callee_rvas)
        mnemonics = ("call", "jmp") if jumps_too else ("call",)

        def is_call(instruction):
            return instruction.mnemonic in mnemonics and instruction.target in callees
        return self._scan(is_call, nth, backward)

    def find_match(self, pattern, nth=1, backward=False):
        regex = re.compile(pattern)

        def is_match(instruction):
            return regex.fullmatch(instruction.text) is not None
        found = self._scan(is_match, nth, backward)
        return found, (regex.fullmatch(found.text) if found else None)
