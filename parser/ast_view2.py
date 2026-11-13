#!/usr/bin/env python3
"""Show an annotated AST JSON as a text tree (checking tool for the 09 lab).

Usage: python3 ast_view.py <ast.json> [--ascii]
  --ascii   use |- `- instead of box-drawing characters (if the terminal breaks them)

Not used for grading. It helps you check by eye that your JSON says what you meant.
  - Expression nodes show their type:      BinaryExpr + : i32
  - Declarations show their number:        VarDecl x #1
  - Names show which declaration they use: Identifier x -> #0 : i32
  - A child field that is null is shown as `else: null`.
  - Marked with ⚠: missing fields (including `"type": null` left out), nodes or fields
    that are not in the spec, wrong value formats, `id` numbers that do not follow the
    numbering rule, and `decl` numbers that point to no declaration (or one with another name).
    (No ⚠ does not mean the answer is right. Compare with the example JSON yourself.)
"""
import json
import sys

EXPR = {"AssignExpr", "BinaryExpr", "UnaryExpr", "CallExpr", "Identifier",
        "IntLiteral", "BoolLiteral", "ParenExpr"}

# Fields of each node (07 fields + the 09 fields id / decl / type)
SCHEMA = {
    "Program": ["functions"],
    "FnDecl": ["name", "params", "returnType", "body"],
    "Param": ["name", "type", "id"],
    "Block": ["stmts"],
    "VarDecl": ["name", "mutable", "type", "id", "init"],
    "IfStmt": ["cond", "then", "else"],
    "WhileStmt": ["cond", "body"],
    "ReturnStmt": ["value"],
    "PrintStmt": ["value"],
    "ExprStmt": ["expr"],
    "AssignExpr": ["type", "target", "value"],
    "BinaryExpr": ["op", "type", "left", "right"],
    "UnaryExpr": ["op", "type", "operand"],
    "CallExpr": ["name", "type", "args"],
    "Identifier": ["name", "decl", "type"],
    "IntLiteral": ["value", "type"],
    "BoolLiteral": ["value", "type"],
    "ParenExpr": ["type", "expr"],
}


def num(n, key, mark):
    # "#3" when present; missing fields are reported separately
    return f" {mark}{n[key]}" if key in n else ""


def param_text(p):
    if isinstance(p, dict):
        return f"{p.get('name')}: {p.get('type')}" + num(p, "id", "#")
    return str(p)


# Text shown on the node's own line
INLINE = {
    "FnDecl": lambda n: f"{n.get('name')}({', '.join(param_text(p) for p in n.get('params') or [])})"
                        + (f" -> {n['returnType']}" if n.get("returnType") else ""),
    "Param": param_text,
    "VarDecl": lambda n: ("mut " if n.get("mutable") is True else "") + str(n.get("name"))
                         + (f": {n['type']}" if n.get("type") else "") + num(n, "id", "#"),
    "BinaryExpr": lambda n: str(n.get("op")),
    "UnaryExpr": lambda n: str(n.get("op")),
    "CallExpr": lambda n: str(n.get("name")),
    "Identifier": lambda n: str(n.get("name")) + num(n, "decl", "-> #"),
    "IntLiteral": lambda n: json.dumps(n.get("value")),
    "BoolLiteral": lambda n: json.dumps(n.get("value")),
}
# Fields already shown on the node's line (not drawn again as children)
SHOWN = {
    "FnDecl": {"name", "params", "returnType"},
    "Param": {"name", "type", "id"},
    "VarDecl": {"name", "mutable", "type", "id"},
    "BinaryExpr": {"op"},
    "UnaryExpr": {"op"},
    "CallExpr": {"name"},
    "Identifier": {"name", "decl"},
    "IntLiteral": {"value"},
    "BoolLiteral": {"value"},
}
for _k in EXPR:
    SHOWN.setdefault(_k, set()).add("type")
# Nodes whose children are drawn with their field name
LABELED = {"IfStmt", "WhileStmt", "AssignExpr"}
# Fields that are always lists ([] when empty)
LIST_FIELDS = {"Program": ["functions"], "FnDecl": ["params"], "Block": ["stmts"], "CallExpr": ["args"]}

warnings = 0
EXTRA = {}  # id(node) -> warnings found by the numbering pass


def warn(msg):
    global warnings
    warnings += 1
    return f"  ⚠ {msg}"


def is_int(v):
    return type(v) is int  # bool is not accepted


def value_problems(kind, n):
    """Value formats that fail the strict comparison (true as 1, 7 as "7" or 7.0, ...)."""
    out = []
    if kind == "IntLiteral" and "value" in n and type(n["value"]) is not int:
        out.append("value는 JSON 정수여야 함")
    if kind == "BoolLiteral" and "value" in n and type(n["value"]) is not bool:
        out.append("value는 true/false여야 함")
    if kind == "VarDecl" and "mutable" in n and type(n["mutable"]) is not bool:
        out.append("mutable은 true/false여야 함")
    for f in LIST_FIELDS.get(kind, ()):
        if f in n and not isinstance(n[f], list):
            out.append(f"{f}는 목록이어야 함 (비어 있으면 null이 아니라 [])")
    if kind in EXPR and "type" in n and n["type"] not in ("i32", "bool", None):
        out.append('type은 "i32" / "bool" / null 중 하나여야 함')
    for f in ("id", "decl"):
        if f in n and not is_int(n[f]):
            out.append(f"{f}는 JSON 정수여야 함")
    return out


def walk(n):
    """All nodes under n in source order (pre-order, fields in spec order)."""
    if isinstance(n, list):
        for x in n:
            yield from walk(x)
    elif isinstance(n, dict):
        yield n
        for k in SCHEMA.get(n.get("node"), list(n)):
            if isinstance(n.get(k), (dict, list)):
                yield from walk(n[k])


def check_numbers(tree):
    """id: per function from 0, parameters first, then each `let` in source order.
    decl: must point to a declaration with the same name in the same function."""
    if not isinstance(tree, dict) or not isinstance(tree.get("functions"), list):
        return
    for f in tree["functions"]:
        if not isinstance(f, dict):
            continue
        decls = [p for p in f.get("params") or [] if isinstance(p, dict)]
        decls += [x for x in walk(f.get("body")) if x.get("node") == "VarDecl"]
        names = {}
        for i, d in enumerate(decls):
            if "id" in d and is_int(d["id"]):
                if d["id"] != i:
                    EXTRA.setdefault(id(d), []).append(f"id 번호 규칙과 다름: 이 선언은 #{i}이어야 함")
                names[d["id"]] = d.get("name")
        for x in walk(f.get("body")):
            if x.get("node") == "Identifier" and "decl" in x and is_int(x["decl"]):
                if x["decl"] not in names:
                    EXTRA.setdefault(id(x), []).append(f"decl #{x['decl']}인 선언이 이 함수에 없음")
                elif names[x["decl"]] != x.get("name"):
                    EXTRA.setdefault(id(x), []).append(
                        f"decl #{x['decl']}은 '{names[x['decl']]}'의 선언 (이름이 다름)")


def label(n):
    if not isinstance(n, dict):
        return json.dumps(n, ensure_ascii=False)
    kind = n.get("node")
    if kind is None:
        return "?" + warn('"node" 필드 없음') + "  " + json.dumps(n, ensure_ascii=False)[:60]
    text = str(kind)
    if kind not in SCHEMA:
        return text + warn("명세에 없는 노드")
    if kind in INLINE:
        text += " " + INLINE[kind](n)
    if kind in EXPR and "type" in n:
        text += " : " + ("null" if n["type"] is None else str(n["type"]))
    missing = [f for f in SCHEMA[kind] if f not in n]
    if missing:
        text += warn("필드 없음: " + ", ".join(missing))
    unknown = [k for k in n if k != "node" and k not in SCHEMA[kind]]
    if unknown:
        text += warn("명세에 없는 필드: " + ", ".join(unknown))
    for p in value_problems(kind, n) + EXTRA.get(id(n), []):
        text += warn(p)
    if kind == "FnDecl":  # parameters are drawn on this line, so their warnings go here too
        for p in n.get("params") or []:
            if isinstance(p, dict):
                miss = [f for f in SCHEMA["Param"] if f not in p]
                if miss:
                    text += warn(f"매개변수 {p.get('name')}: 필드 없음: " + ", ".join(miss))
                for q in value_problems("Param", p) + EXTRA.get(id(p), []):
                    text += warn(f"매개변수 {p.get('name')}: {q}")
    return text


def children(n):
    """List of (field name or None, child). A None child is drawn as one 'k: null' line."""
    if not isinstance(n, dict) or n.get("node") not in SCHEMA:
        # unknown node: draw only dict/list children, with their field names
        out = []
        if isinstance(n, dict):
            for k, v in n.items():
                if isinstance(v, dict):
                    out.append((k, v))
                elif isinstance(v, list):
                    out += [(k, x) for x in v if isinstance(x, dict)]
        return out
    kind = n["node"]
    shown = SHOWN.get(kind, set())
    out = []
    for k in SCHEMA[kind] + [k for k in n if k != "node" and k not in SCHEMA[kind]]:
        if k in shown or k not in n:
            continue
        v = n[k]
        tag = k if kind in LABELED or k not in SCHEMA[kind] else None
        if v is None:
            out.append((k, None))
        elif isinstance(v, dict):
            out.append((tag, v))
        elif isinstance(v, list):
            out += [(tag, x) for x in v]
    return out


def draw(n, prefix, lines, glyphs):
    tee, last, bar, blank = glyphs
    kids = children(n)
    for i, (tag, c) in enumerate(kids):
        end = i == len(kids) - 1
        lines.append(prefix + (last if end else tee) + (f"{tag}: " if tag else "") + label(c))
        if isinstance(c, dict):
            draw(c, prefix + (blank if end else bar), lines, glyphs)


def main():
    args = [a for a in sys.argv[1:] if a != "--ascii"]
    if len(args) != 1:
        sys.exit(__doc__)
    glyphs = ("|- ", "`- ", "|  ", "   ") if "--ascii" in sys.argv else ("├─ ", "└─ ", "│  ", "   ")
    with open(args[0], encoding="utf-8") as f:
        text = f.read()
    try:
        tree = json.loads(text)
    except json.JSONDecodeError as e:
        first = text.strip().splitlines()[0] if text.strip() else "(빈 파일)"
        sys.exit(f"JSON이 아님: {e}\n첫 줄: {first}")
    check_numbers(tree)
    lines = [label(tree)]
    draw(tree, "", lines, glyphs)
    sys.stdout.reconfigure(encoding="utf-8")
    print("\n".join(lines))
    if warnings:
        print(f"\n⚠ 경고 {warnings}개 — 위의 ⚠ 표시를 확인하세요.")


if __name__ == "__main__":
    main()
