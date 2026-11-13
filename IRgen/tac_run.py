#!/usr/bin/env python3
"""TAC v1 interpreter for the Mini-Rust term project (step 1: 3AC).

Usage:
    python3 tac_run.py <file.tac>            run, program output on stdout
    python3 tac_run.py --trace <file.tac>    also print each executed instruction (stderr)
    python3 tac_run.py --calls <file.tac>    also print call counts per function (stderr)

Exit code: 0 = ok, 1 = parse error, 2 = runtime error.
Errors go to stderr as "TAC parse error at line N: ..." / "TAC runtime error at line N: ...".
Spec: TAC_SPEC.md (Python 3.8+, standard library only).
"""
import json
import re
import sys

VERSION = "TAC v1 (2026-11-09)"

MAX_STEPS = 10_000_000
MAX_DEPTH = 10_000
I32_MIN, I32_MAX = -(1 << 31), (1 << 31) - 1

RESERVED = {"func", "endfunc", "goto", "if", "ifFalse", "param", "call",
            "return", "print_i32", "print_bool"}
ARITH = {"+", "-", "*", "/"}
REL = {"<", "<=", ">", ">=", "==", "!="}
BIN = ARITH | REL

TOKEN_RE = re.compile(r"-?[0-9]+|[A-Za-z_][A-Za-z0-9_]*|==|!=|<=|>=|[-+*/<>=!,():]")
NAME_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
INT_RE = re.compile(r"-?[0-9]+")


class TacError(Exception):
    def __init__(self, kind, line, msg):
        super().__init__(msg)
        self.kind, self.line, self.msg = kind, line, msg

    def __str__(self):
        where = f" at line {self.line}" if self.line else ""
        return f"TAC {self.kind} error{where}: {self.msg}"


def perr(line, msg):
    return TacError("parse", line, msg)


# ---------------------------------------------------------------- parsing

def tokenize(text, no):
    out, pos = [], 0
    while pos < len(text):
        if text[pos] in " \t":
            pos += 1
            continue
        if text[pos] == "#":          # comment to end of line
            break
        m = TOKEN_RE.match(text, pos)
        if not m:
            if text[pos] in "&|":
                raise perr(no, "there is no && / || in 3AC: use jumps (short-circuit)")
            raise perr(no, f"unexpected character {text[pos]!r}")
        out.append(m.group(0))
        pos = m.end()
    return out


def is_name(t):
    return bool(NAME_RE.fullmatch(t)) and t not in RESERVED


def check_name(t, no, what):
    if not NAME_RE.fullmatch(t):
        raise perr(no, f"{what} must be a name, got {t!r}")
    if t in RESERVED:
        raise perr(no, f"{what} cannot be the reserved word {t!r}")
    return t


def operand(t, no):
    """Returns ('c', int) for a constant or ('v', name) for a name."""
    if INT_RE.fullmatch(t):
        v = int(t)
        if not I32_MIN <= v <= I32_MAX:
            raise perr(no, f"constant {t} is out of i32 range")
        return ("c", v)
    return ("v", check_name(t, no, "operand"))


def parse_count(t, no):
    if not re.fullmatch(r"[0-9]+", t):
        raise perr(no, f"argument count must be a non-negative integer, got {t!r}")
    return int(t)


def parse_instr(t, no):
    """t = token list of one instruction line (not a label, not func/endfunc)."""
    n = len(t)
    head = t[0]
    if head == "goto":
        if n != 2:
            raise perr(no, "expected: goto L")
        return ("goto", check_name(t[1], no, "label"))
    if head in ("if", "ifFalse"):
        if n == 4 and t[2] == "goto":
            return (head, operand(t[1], no), check_name(t[3], no, "label"))
        if head == "if" and n == 6 and t[2] in REL and t[4] == "goto":
            return ("ifrel", operand(t[1], no), t[2], operand(t[3], no),
                    check_name(t[5], no, "label"))
        raise perr(no, "expected: if y goto L / ifFalse y goto L / if y relop z goto L")
    if head == "param":
        if n != 2:
            raise perr(no, "expected: param y")
        return ("param", operand(t[1], no))
    if head == "call":
        if n != 4 or t[2] != ",":
            raise perr(no, "expected: call f, n")
        return ("call", None, check_name(t[1], no, "function name"), parse_count(t[3], no))
    if head == "return":
        if n == 1:
            return ("return", None)
        if n == 2:
            return ("return", operand(t[1], no))
        raise perr(no, "expected: return / return y")
    if head in ("print_i32", "print_bool"):
        if n != 2:
            raise perr(no, f"expected: {head} y")
        return (head, operand(t[1], no))
    # x = ...
    if n >= 3 and t[1] == "=":
        x = check_name(t[0], no, "destination")
        r = t[2:]
        if len(r) == 1:
            return ("copy", x, operand(r[0], no))
        if len(r) == 2 and r[0] in ("-", "!"):
            return ("unary", x, r[0], operand(r[1], no))
        if len(r) == 3 and r[1] in BIN:
            return ("binary", x, r[1], operand(r[0], no), operand(r[2], no))
        if len(r) == 4 and r[0] == "call" and r[2] == ",":
            return ("call", x, check_name(r[1], no, "function name"), parse_count(r[3], no))
        raise perr(no, "not a 3AC instruction (one operator per line: x = y op z)")
    raise perr(no, "not a 3AC instruction")


def parse_header(t, no):
    # func NAME ( [p {, p}] )
    if len(t) < 4 or t[2] != "(" or t[-1] != ")":
        raise perr(no, "expected: func name(p1, p2, ...)")
    name = check_name(t[1], no, "function name")
    inner = t[3:-1]
    params = []
    if inner:
        if len(inner) % 2 == 0:
            raise perr(no, "bad parameter list")
        for i, tok in enumerate(inner):
            if i % 2 == 1:
                if tok != ",":
                    raise perr(no, "parameters must be separated by ','")
            else:
                p = check_name(tok, no, "parameter")
                if p in params:
                    raise perr(no, f"duplicate parameter {p!r}")
                params.append(p)
    return name, params


def parse(source):
    funcs = {}
    cur = None
    for no, raw in enumerate(source.splitlines(), 1):
        t = tokenize(raw, no)
        if not t:
            continue
        if cur is None:
            if t[0] != "func":
                raise perr(no, "expected 'func' (instructions must be inside func ... endfunc)")
            name, params = parse_header(t, no)
            if name in funcs:
                raise perr(no, f"function {name!r} is defined twice")
            cur = {"name": name, "params": params, "code": [], "lines": [],
                   "labels": {}, "line": no}
            continue
        if t[0] == "func":
            raise perr(no, f"missing 'endfunc' for function {cur['name']!r}")
        if t == ["endfunc"]:
            funcs[cur["name"]] = cur
            cur = None
            continue
        if len(t) == 2 and t[1] == ":":
            lab = check_name(t[0], no, "label")
            if lab in cur["labels"]:
                raise perr(no, f"label {lab!r} is defined twice in {cur['name']!r}")
            cur["labels"][lab] = len(cur["code"])
            continue
        cur["code"].append(parse_instr(t, no))
        cur["lines"].append(no)
    if cur is not None:
        raise perr(cur["line"], f"missing 'endfunc' for function {cur['name']!r}")
    check_static(funcs)
    return funcs


def check_static(funcs):
    if "main" not in funcs:
        raise perr(0, "no function 'main'")
    if funcs["main"]["params"]:
        raise perr(funcs["main"]["line"], "'main' must have no parameters")
    for f in funcs.values():
        for ins, no in zip(f["code"], f["lines"]):
            k = ins[0]
            label = {"goto": 1, "if": 2, "ifFalse": 2, "ifrel": 4}.get(k)
            if label is not None and ins[label] not in f["labels"]:
                raise perr(no, f"label {ins[label]!r} is not defined in {f['name']!r}")
            if k == "call":
                g = ins[2]
                if g not in funcs:
                    raise perr(no, f"function {g!r} is not defined")
                if ins[3] != len(funcs[g]["params"]):
                    raise perr(no, f"{g!r} has {len(funcs[g]['params'])} parameter(s), "
                                   f"but call uses {ins[3]}")


# ---------------------------------------------------------------- running

def wrap(v):
    v &= 0xFFFFFFFF
    return v - (1 << 32) if v > I32_MAX else v


def compare(op, a, b):
    if op == "<": return a < b
    if op == "<=": return a <= b
    if op == ">": return a > b
    if op == ">=": return a >= b
    if op == "==": return a == b
    return a != b


def fmt(ins):
    def o(x):
        return str(x[1])
    k = ins[0]
    if k == "copy": return f"{ins[1]} = {o(ins[2])}"
    if k == "unary": return f"{ins[1]} = {ins[2]} {o(ins[3])}"
    if k == "binary": return f"{ins[1]} = {o(ins[3])} {ins[2]} {o(ins[4])}"
    if k == "goto": return f"goto {ins[1]}"
    if k in ("if", "ifFalse"): return f"{k} {o(ins[1])} goto {ins[2]}"
    if k == "ifrel": return f"if {o(ins[1])} {ins[2]} {o(ins[3])} goto {ins[4]}"
    if k == "param": return f"param {o(ins[1])}"
    if k == "call":
        s = f"call {ins[2]}, {ins[3]}"
        return s if ins[1] is None else f"{ins[1]} = {s}"
    if k == "return": return "return" if ins[1] is None else f"return {o(ins[1])}"
    return f"{k} {o(ins[1])}"


class Frame:
    __slots__ = ("func", "env", "pc", "pending", "dest")

    def __init__(self, func, args, dest):
        self.func = func
        self.env = dict(zip(func["params"], args))
        self.pc = 0
        self.pending = []
        self.dest = dest          # caller's destination name (or None)


def run(funcs, out=sys.stdout, trace=None):
    """Runs main. Returns (steps, call_counts). Raises TacError on runtime errors."""
    calls = {name: 0 for name in funcs}
    calls["main"] = 1
    stack = [Frame(funcs["main"], [], None)]
    steps = 0

    while stack:
        fr = stack[-1]
        f = fr.func
        code = f["code"]
        if fr.pc >= len(code):           # reached endfunc: return without value
            ins, no = ("return", None), None
        else:
            ins, no = code[fr.pc], f["lines"][fr.pc]
            fr.pc += 1
        steps += 1
        if steps > MAX_STEPS:
            raise TacError("runtime", no, f"more than {MAX_STEPS} instructions executed (infinite loop?)")
        if trace is not None and no is not None:
            trace.write(f"[{f['name']}:{no}] {fmt(ins)}\n")

        env = fr.env

        def val(o):
            if o[0] == "c":
                return o[1]
            if o[1] not in env:
                raise TacError("runtime", no, f"{o[1]!r} is read before any value is assigned "
                                              f"(in {f['name']!r})")
            return env[o[1]]

        def boolean(o, what):
            v = val(o)
            if v not in (0, 1):
                raise TacError("runtime", no, f"{what} needs a bool (0 or 1), got {v}")
            return v

        k = ins[0]
        if k == "copy":
            env[ins[1]] = val(ins[2])
        elif k == "unary":
            if ins[2] == "-":
                env[ins[1]] = wrap(-val(ins[3]))
            else:
                env[ins[1]] = 1 - boolean(ins[3], "'!'")
        elif k == "binary":
            op, a, b = ins[2], val(ins[3]), val(ins[4])
            if op == "+": r = wrap(a + b)
            elif op == "-": r = wrap(a - b)
            elif op == "*": r = wrap(a * b)
            elif op == "/":
                if b == 0:
                    raise TacError("runtime", no, "division by zero")
                q = abs(a) // abs(b)
                r = wrap(q if (a < 0) == (b < 0) else -q)   # truncate toward zero
            else:
                r = 1 if compare(op, a, b) else 0
            env[ins[1]] = r
        elif k == "goto":
            fr.pc = f["labels"][ins[1]]
        elif k == "if":
            if boolean(ins[1], "'if'"):
                fr.pc = f["labels"][ins[2]]
        elif k == "ifFalse":
            if not boolean(ins[1], "'ifFalse'"):
                fr.pc = f["labels"][ins[2]]
        elif k == "ifrel":
            if compare(ins[2], val(ins[1]), val(ins[3])):
                fr.pc = f["labels"][ins[4]]
        elif k == "param":
            fr.pending.append(val(ins[1]))
        elif k == "call":
            n = ins[3]
            if n > len(fr.pending):
                raise TacError("runtime", no, f"call {ins[2]}, {n} needs {n} param(s), "
                                              f"but only {len(fr.pending)} are pending")
            args = fr.pending[len(fr.pending) - n:]
            del fr.pending[len(fr.pending) - n:]
            if len(stack) >= MAX_DEPTH:
                raise TacError("runtime", no, f"call depth over {MAX_DEPTH} (infinite recursion?)")
            calls[ins[2]] += 1
            stack.append(Frame(funcs[ins[2]], args, (ins[1], no)))
        elif k == "return":
            rv = None if ins[1] is None else val(ins[1])
            stack.pop()
            if fr.dest is not None:
                dest, call_no = fr.dest
                if dest is not None:
                    if rv is None:
                        raise TacError("runtime", call_no,
                                       f"{dest} = call {f['name']}: {f['name']!r} returned no value")
                    stack[-1].env[dest] = rv
        elif k == "print_i32":
            out.write(f"{val(ins[1])}\n")
        elif k == "print_bool":
            out.write("true\n" if boolean(ins[1], "print_bool") else "false\n")
    return steps, calls


def main(argv):
    args = argv[1:]
    trace = "--trace" in args
    show_calls = "--calls" in args
    files = [a for a in args if a not in ("--trace", "--calls")]
    if "--version" in files:
        print(VERSION)
        return 0
    if len(files) != 1 or files[0].startswith("-"):
        print(__doc__.strip(), file=sys.stderr)
        return 1
    try:
        with open(files[0], encoding="utf-8") as fp:
            source = fp.read()
    except (OSError, UnicodeDecodeError) as e:
        print(f"TAC error: cannot read {files[0]}: {e}", file=sys.stderr)
        return 1
    try:
        funcs = parse(source)
    except TacError as e:
        print(e, file=sys.stderr)
        return 1
    calls = None
    try:
        steps, calls = run(funcs, sys.stdout, sys.stderr if trace else None)
    except TacError as e:
        sys.stdout.flush()
        print(e, file=sys.stderr)
        return 2
    finally:
        if show_calls and calls is not None:
            sys.stdout.flush()
            print("CALLS " + json.dumps(calls, sort_keys=True), file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
