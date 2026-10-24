#!/usr/bin/env python3
"""AST JSON을 텍스트 트리로 보여 주는 확인용 도구 (07 실습 배포용).

사용법: python3 ast_view.py <ast.json> [--ascii]
  --ascii   선 문자 대신 |- `- 사용 (터미널에서 └─ 가 깨질 때)

채점과는 무관하다. 자기 출력이 의도한 트리 모양인지 눈으로 확인하는 데 쓴다.
  - 값이 null인 자식 필드는 `else: null`처럼 보여 준다.
  - 명세의 필드가 빠졌거나, 명세에 없는 노드·필드가 있거나, 값의 형식이 다르면 ⚠ 로 표시한다.
    (빈 목록을 [] 대신 null로 쓴 것도 표시)
    (⚠ 가 없다고 정답이라는 뜻은 아니다. 트리 모양은 예시 JSON과 직접 비교할 것)
"""
import json
import sys

# 노드별 필드 (명세 순서)
SCHEMA = {
    "Program": ["functions"],
    "FnDecl": ["name", "params", "returnType", "body"],
    "Param": ["name", "type"],
    "Block": ["stmts"],
    "VarDecl": ["name", "mutable", "type", "init"],
    "IfStmt": ["cond", "then", "else"],
    "WhileStmt": ["cond", "body"],
    "ReturnStmt": ["value"],
    "PrintStmt": ["value"],
    "ExprStmt": ["expr"],
    "AssignExpr": ["target", "value"],
    "BinaryExpr": ["op", "left", "right"],
    "UnaryExpr": ["op", "operand"],
    "CallExpr": ["name", "args"],
    "Identifier": ["name"],
    "IntLiteral": ["value"],
    "BoolLiteral": ["value"],
    "ParenExpr": ["expr"],
}


def param_text(p):
    if isinstance(p, dict):
        return f"{p.get('name')}: {p.get('type')}"
    return str(p)


# 이름 줄에 붙여 보여 줄 값
INLINE = {
    "FnDecl": lambda n: f"{n.get('name')}({', '.join(param_text(p) for p in n.get('params') or [])})"
                        + (f" -> {n['returnType']}" if n.get("returnType") else ""),
    "Param": param_text,
    "VarDecl": lambda n: ("mut " if n.get("mutable") is True else "") + str(n.get("name"))
                         + (f": {n['type']}" if n.get("type") else ""),
    "BinaryExpr": lambda n: str(n.get("op")),
    "UnaryExpr": lambda n: str(n.get("op")),
    "CallExpr": lambda n: str(n.get("name")),
    "Identifier": lambda n: str(n.get("name")),
    "IntLiteral": lambda n: json.dumps(n.get("value")),
    "BoolLiteral": lambda n: json.dumps(n.get("value")),
}
# 이름 줄에서 이미 보여 준 필드 (자식으로 다시 그리지 않음)
SHOWN = {
    "FnDecl": {"name", "params", "returnType"},
    "Param": {"name", "type"},
    "VarDecl": {"name", "mutable", "type"},
    "BinaryExpr": {"op"},
    "UnaryExpr": {"op"},
    "CallExpr": {"name"},
    "Identifier": {"name"},
    "IntLiteral": {"value"},
    "BoolLiteral": {"value"},
}
# 자식 앞에 필드 이름을 붙여 역할을 보여 줄 노드
LABELED = {"IfStmt", "WhileStmt", "AssignExpr"}
# 항상 목록인 필드 (비어 있으면 [])
LIST_FIELDS = {"Program": ["functions"], "FnDecl": ["params"], "Block": ["stmts"], "CallExpr": ["args"]}

warnings = 0


def warn(msg):
    global warnings
    warnings += 1
    return f"  ⚠ {msg}"


def value_problems(kind, n):
    """strict 비교에서 틀리기 쉬운 값 형식 (true를 1로, 7을 "7"이나 7.0으로 쓰는 것 등)"""
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
    return out


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
    missing = [f for f in SCHEMA[kind] if f not in n]
    if missing:
        text += warn("필드 없음: " + ", ".join(missing))
    unknown = [k for k in n if k != "node" and k not in SCHEMA[kind]]
    if unknown:
        text += warn("명세에 없는 필드: " + ", ".join(unknown))
    for p in value_problems(kind, n):
        text += warn(p)
    return text


def children(n):
    """(필드 이름 또는 None, 자식) 목록. 자식이 None이면 'k: null' 한 줄로 그림."""
    if not isinstance(n, dict) or n.get("node") not in SCHEMA:
        # 모르는 노드: dict/list 자식만 필드 이름을 붙여 그림
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
    lines = [label(tree)]
    draw(tree, "", lines, glyphs)
    sys.stdout.reconfigure(encoding="utf-8")
    print("\n".join(lines))
    if warnings:
        print(f"\n⚠ 경고 {warnings}개 — 위의 ⚠ 표시를 확인하세요.")


if __name__ == "__main__":
    main()
