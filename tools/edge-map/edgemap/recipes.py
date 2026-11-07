# Recipes: how each value the DLL needs from msedge.dll is found in a build, so a
# new build is mapped by running them rather than by hand. One TOML table per
# value, named as the C++ code names it. A recipe is one of:
#
#   symbol = "<name>"                 the address of a named function or object
#   symbols = "<regex>"               every address of every matching name (a list)
#   slot_of = { vtable = "<name>", function = "<name>" }
#                                     the byte offset of the vtable slot holding it
#   in = "<function>", find = [steps], take = "<what>"
#                                     a search through the function's code
#   value = <n>, stable = "<why>"     not Edge's to change (a std or Windows layout,
#                                     a value of our own)
#   same_as = "<other entry>"         equal to another entry by definition
#
# A step is { call = "<name>" } (also `calls = [...]`), { match = "<regex>" } or
# { follow = true } (continue in the code the current jump or call leads to), each
# with an optional nth (default 1) and back = true (search backwards from the
# cursor). `take` is "start" (the instruction's address), "after" (the next one's:
# a call's return address), "target" (a jump, call or rip-relative operand's
# address), "imm_at" / "disp_at" (where the instruction's immediate or
# displacement bytes start) or "group" (regex group 1 of the last match, a number).
#
# Then optionally `add = <n>`, and `read = "u32" | "i32" | "u64" | "pointer"` to
# take the value stored at the address found instead of the address.
import os
import re
import tomllib
from dataclasses import dataclass, field

from .code import DEFAULT_LIMIT, Region

STEP_KEYS = {"call", "calls", "match", "follow", "nth", "back", "jumps", "limit"}
TAKES = {"start", "after", "target", "imm_at", "disp_at", "group"}


@dataclass
class Recipe:
    name: str
    source: str           # the recipe file, for messages
    spec: dict
    type: str = "uintptr_t"
    features: list = field(default_factory=list)


class Unresolved(Exception):
    pass


def load(folder):
    recipes = {}
    for file_name in sorted(os.listdir(folder)):
        if not file_name.endswith(".toml"):
            continue
        with open(os.path.join(folder, file_name), "rb") as handle:
            tables = tomllib.load(handle)
        area = file_name[:-len(".toml")]
        for name, spec in tables.items():
            if name in recipes:
                raise ValueError(f"{name} is defined in {recipes[name].source} and {file_name}")
            recipes[name] = Recipe(name, file_name, spec, spec.get("type", "uintptr_t"), [area])
    return recipes


def parse_number(text):
    text = text.strip()
    negative = text.startswith("-")
    digits = text.lstrip("-")
    number = int(digits, 16) if digits.lower().startswith("0x") else int(digits)
    return -number if negative else number


class Resolver:
    def __init__(self, image, symbols, recipes):
        self.image = image
        self.symbols = symbols
        self.recipes = recipes
        self.values = {}

    def resolve_all(self):
        results = {}
        for name in self.recipes:
            try:
                results[name] = self.resolve(name)
            except (Unresolved, LookupError, ValueError) as error:
                results[name] = error
        return results

    def resolve(self, name, depth=0):
        if name in self.values:
            return self.values[name]
        if depth > 8:
            raise Unresolved(f"{name}: same_as loops")
        recipe = self.recipes.get(name)
        if recipe is None:
            raise Unresolved(f"no recipe {name}")
        value = self._locate(recipe, depth)
        self.values[name] = value
        return value

    def _locate(self, recipe, depth):
        spec = recipe.spec
        if "value" in spec:
            if "stable" not in spec:
                raise Unresolved(f"{recipe.name}: a fixed value must say why it is stable")
            return spec["value"]
        if "same_as" in spec:
            return self.resolve(spec["same_as"], depth + 1)
        if "symbols" in spec:
            found = self.symbols.matching(spec["symbols"])
            addresses = sorted({rva for rvas in found.values() for rva in rvas})
            if not addresses:
                raise Unresolved(f"{recipe.name}: nothing matches {spec['symbols']}")
            return addresses
        if "slot_of" in spec:
            return self._slot_of(recipe, spec["slot_of"])
        if "symbol" in spec:
            address = self.symbols.address(spec["symbol"])
        elif "in" in spec:
            address = self._search(recipe)
        else:
            raise Unresolved(f"{recipe.name}: no recipe kind")
        address += spec.get("add", 0)
        return self._read(recipe, address)

    def _read(self, recipe, address):
        kind = recipe.spec.get("read")
        if kind is None:
            return address
        readers = {"u32": self.image.u32, "i32": self.image.i32, "u64": self.image.u64,
                   "pointer": self.image.pointer_rva}
        if kind not in readers:
            raise Unresolved(f"{recipe.name}: unknown read {kind}")
        return readers[kind](address)

    def _slot_of(self, recipe, slot):
        vtable = self.symbols.address(slot["vtable"])
        functions = set(self.symbols.addresses(slot["function"]))
        if not functions:
            raise Unresolved(f"{recipe.name}: no function {slot['function']}")
        for index in range(slot.get("max_slots", 400)):
            target = self.image.pointer_rva(vtable + index * 8)
            if target in functions:
                return index * 8
        raise Unresolved(f"{recipe.name}: {slot['function']} is in no slot of {slot['vtable']}")

    def _search(self, recipe):
        spec = recipe.spec
        region = Region(self.image, self.symbols, self.symbols.address(spec["in"]),
                        spec.get("limit", DEFAULT_LIMIT))
        instruction = None
        last_match = None
        for step in spec.get("find", []):
            unknown = set(step) - STEP_KEYS
            if unknown:
                raise Unresolved(f"{recipe.name}: unknown step keys {sorted(unknown)}")
            nth = step.get("nth", 1)
            backward = step.get("back", False)
            if step.get("follow"):
                if instruction is None or instruction.target is None:
                    raise Unresolved(f"{recipe.name}: nothing to follow")
                region = Region(self.image, self.symbols, instruction.target,
                                step.get("limit", DEFAULT_LIMIT))
                instruction = None
                continue
            if "call" in step or "calls" in step:
                names = step.get("calls", [step.get("call")])
                callees = [rva for callee in names for rva in self.symbols.addresses(callee)]
                if not callees:
                    raise Unresolved(f"{recipe.name}: no callee {names}")
                instruction = region.find_call(callees, nth, backward, step.get("jumps", False))
                if instruction is None:
                    raise Unresolved(f"{recipe.name}: call #{nth} to {names} not found in {spec['in']}")
                continue
            if "match" in step:
                instruction, last_match = region.find_match(step["match"], nth, backward)
                if instruction is None:
                    raise Unresolved(f"{recipe.name}: '{step['match']}' #{nth} not found")
                continue
            raise Unresolved(f"{recipe.name}: empty step")
        take = spec.get("take", "start")
        if take not in TAKES:
            raise Unresolved(f"{recipe.name}: unknown take {take}")
        if take == "group":
            if last_match is None or not last_match.groups():
                raise Unresolved(f"{recipe.name}: take = group needs a match with a group")
            return parse_number(last_match.group(1))
        if instruction is None:
            raise Unresolved(f"{recipe.name}: the steps found no instruction")
        if take == "start":
            return instruction.rva
        if take == "after":
            return instruction.end
        if take == "target":
            if instruction.target is None:
                raise Unresolved(f"{recipe.name}: {instruction.text} has no target")
            return instruction.target
        if take == "imm_at":
            if not instruction.imm_offset:
                raise Unresolved(f"{recipe.name}: {instruction.text} has no immediate")
            return instruction.rva + instruction.imm_offset
        if not instruction.disp_offset:
            raise Unresolved(f"{recipe.name}: {instruction.text} has no displacement")
        return instruction.rva + instruction.disp_offset
