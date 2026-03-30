#!/usr/bin/env python3
"""Generate Draw.io XML UML for the Valkron codebase.

This script scans C/C++ headers, extracts class/struct declarations,
inheritance, fields, methods, and relationships, then writes:
- Draw.io XML diagram (.drawio.xml)
"""

from __future__ import annotations

import argparse
import datetime
import pathlib
import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from typing import Dict, List, Set, Tuple

CLASS_DECL_RE = re.compile(
    r"^\s*(class|struct)\s+(?:[A-Za-z_][A-Za-z0-9_]*\s+)*([A-Za-z_][A-Za-z0-9_]*)"
    r"\s*(?::\s*(?:(?:public|protected|private)\s+)?([A-Za-z_:][A-Za-z0-9_:]*))?\s*\{"
)
ENUM_DECL_RE = re.compile(r"^\s*enum(?:\s+class)?\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{")
METHOD_RE = re.compile(r"^\s*(?:virtual\s+)?[A-Za-z_~][^;{]*\([^;{}]*\)\s*(?:const\s*)?(?:=\s*0\s*)?;")
SMART_PTR_RE = re.compile(r"(?:std::)?(?:unique_ptr|shared_ptr|weak_ptr)\s*<\s*([A-Za-z_][A-Za-z0-9_:]*)\s*>")
RAW_PTR_REF_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_:]*)\s*[*&]\s*[A-Za-z_][A-Za-z0-9_]*")
VALUE_MEMBER_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_:]*)\s+[A-Za-z_][A-Za-z0-9_]*\b")
SMART_MEMBER_LINE_RE = re.compile(r"^\s*((?:std::)?(?:unique_ptr|shared_ptr|weak_ptr)\s*<[^>]+>)\s*([A-Za-z_][A-Za-z0-9_]*)")
RAW_MEMBER_LINE_RE = re.compile(r"^\s*(.+?[*&])\s*([A-Za-z_][A-Za-z0-9_]*)$")
GENERIC_MEMBER_LINE_RE = re.compile(r"^\s*(.+?)\s+([A-Za-z_][A-Za-z0-9_]*)$")

PRIMITIVE_OR_STD_PREFIXES = (
    "std::",
    "glm::",
)
PRIMITIVES = {
    "void",
    "bool",
    "char",
    "short",
    "int",
    "long",
    "float",
    "double",
    "signed",
    "unsigned",
    "size_t",
    "uint32_t",
    "uint64_t",
    "GLFWwindow",
}


@dataclass
class ParsedType:
    kind: str
    name: str
    base: str | None = None
    methods: List[Tuple[str, str]] = field(default_factory=list)
    members: List[Tuple[str, str, str, str]] = field(default_factory=list)
    enum_values: List[str] = field(default_factory=list)


@dataclass
class ParseResult:
    types: Dict[str, ParsedType] = field(default_factory=dict)


def discover_headers(root: pathlib.Path) -> List[pathlib.Path]:
    excluded = {"vendor", "build", "bin", ".git", ".idea", ".vscode"}
    headers: List[pathlib.Path] = []

    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix not in {".h", ".hpp"}:
            continue
        parts = set(path.parts)
        if parts.intersection(excluded):
            continue
        headers.append(path)

    return sorted(headers)


def clean_type_name(type_name: str) -> str:
    cleaned = type_name.strip()
    if "::" in cleaned:
        cleaned = cleaned.split("::")[-1]
    return cleaned


def should_skip_type(type_name: str) -> bool:
    if type_name in PRIMITIVES:
        return True
    return type_name.startswith(PRIMITIVE_OR_STD_PREFIXES)


def normalize_type_for_display(type_name: str) -> str:
    return re.sub(r"\s+", " ", type_name.strip())


def truncate_for_display(text: str, max_chars: int = 32) -> str:
    compact = re.sub(r"\s+", " ", text.strip())
    return compact


def compact_param_for_display(param: str) -> str:
    p = re.sub(r"\s+", " ", param.strip())
    if not p:
        return p
    if p == "...":
        return p

    p = p.split("=", 1)[0].strip()
    if not p:
        return p

    # Remove trailing parameter name while keeping the type for shorter UML lines.
    match = re.match(r"^(.*\S)\s+([A-Za-z_][A-Za-z0-9_]*)$", p)
    if match:
        candidate_type = match.group(1).strip()
        if candidate_type and not candidate_type.endswith("::"):
            p = candidate_type

    return p


def compact_method_for_display(method: str) -> str:
    # Keep full method signatures; only normalize whitespace.
    return re.sub(r"\s+", " ", method.strip())


def compact_type_for_display(type_name: str) -> str:
    # Keep full type declarations; only normalize whitespace.
    return normalize_type_for_display(type_name)


def estimate_text_px(text: str) -> int:
    # Approximate monospace width for draw.io labels.
    cleaned = text.replace("\t", " ")
    width = 0.0
    for ch in cleaned:
        if ch in {"W", "M", "@", "#"}:
            width += 8.2
        elif ch in {"i", "l", "!", "|", ".", ",", ":", ";", "'"}:
            width += 3.2
        elif ch == " ":
            width += 3.2
        else:
            width += 6.6
    return int(width)


def is_probable_method_signature(line: str) -> bool:
    stripped = line.strip()
    if not stripped:
        return False
    if stripped.startswith(":"):
        return False
    if stripped.startswith(("if ", "for ", "while ", "switch ", "catch ", "return ")):
        return False

    prefix = stripped.split("(", 1)[0].strip()
    if not prefix or prefix.endswith(","):
        return False

    # Require a plausible function token before the parameter list.
    return re.search(r"[A-Za-z_~][A-Za-z0-9_~]*\s*$", prefix) is not None


def extract_relation_target(member_type: str, relation: str) -> str:
    if relation == "smart":
        match = re.search(r"<\s*([A-Za-z_][A-Za-z0-9_:]*)\s*>", member_type)
        if match:
            return clean_type_name(match.group(1))

    stripped = member_type.replace("*", " ").replace("&", " ").strip()
    if not stripped:
        return ""
    candidate = stripped.split()[-1]
    return clean_type_name(candidate)


def parse_header(path: pathlib.Path, result: ParseResult) -> None:
    lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
    i = 0

    while i < len(lines):
        line = lines[i]

        enum_match = ENUM_DECL_RE.match(line)
        if enum_match:
            enum_name = enum_match.group(1)
            parsed_enum = ParsedType(kind="enum", name=enum_name)

            brace_depth = line.count("{") - line.count("}")
            i += 1

            while i < len(lines) and brace_depth > 0:
                enum_line = lines[i]
                top_level_enum_scope = brace_depth == 1
                stripped_enum = enum_line.strip()

                if top_level_enum_scope and stripped_enum and not stripped_enum.startswith("//") and not stripped_enum.startswith("#"):
                    no_comment = stripped_enum.split("//", 1)[0].strip()
                    if no_comment:
                        for token in no_comment.split(","):
                            item = token.strip()
                            if not item:
                                continue
                            item = item.split("=", 1)[0].strip()
                            item = item.split("{", 1)[0].strip()
                            item = item.split("}", 1)[0].strip()
                            if item and re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", item):
                                parsed_enum.enum_values.append(item)

                brace_depth += enum_line.count("{") - enum_line.count("}")
                i += 1

            result.types[enum_name] = parsed_enum
            continue

        decl_match = CLASS_DECL_RE.match(line)
        if not decl_match:
            i += 1
            continue

        kind, name, base = decl_match.groups()
        base = clean_type_name(base) if base else None

        parsed = ParsedType(kind=kind, name=name, base=base)
        current_access = "private" if kind == "class" else "public"
        brace_depth = line.count("{") - line.count("}")
        i += 1

        while i < len(lines) and brace_depth > 0:
            body_line = lines[i]
            top_level_member_scope = brace_depth == 1

            stripped = body_line.strip()
            if stripped.startswith("//") or stripped.startswith("#"):
                brace_depth += body_line.count("{") - body_line.count("}")
                i += 1
                continue

            if top_level_member_scope and stripped in {"public:", "private:", "protected:"}:
                current_access = stripped[:-1]
                brace_depth += body_line.count("{") - body_line.count("}")
                i += 1
                continue

            if top_level_member_scope and (stripped.startswith("using ") or stripped.startswith("typedef ")):
                brace_depth += body_line.count("{") - body_line.count("}")
                i += 1
                continue

            if top_level_member_scope and stripped.endswith(";") and "(" in stripped:
                method = stripped.rstrip(";").strip()
                prefix_before_paren = method.split("(", 1)[0]
                if (
                    method
                    and is_probable_method_signature(method)
                    and not method.startswith("friend ")
                    and "=" not in prefix_before_paren
                ):
                    parsed.methods.append((method, current_access))

            if top_level_member_scope and "(" in stripped and "{" not in stripped and not stripped.endswith(";"):
                method = stripped.strip()
                if method and is_probable_method_signature(method) and not method.startswith("friend "):
                    parsed.methods.append((method, current_access))

            if top_level_member_scope and "(" in stripped and "{" in stripped and not stripped.endswith(";"):
                method = stripped.split("{", 1)[0].strip()
                if method and is_probable_method_signature(method):
                    parsed.methods.append((method, current_access))

            member_line = ""
            if top_level_member_scope and stripped.endswith(";"):
                member_line = stripped.rstrip(";").strip()
                member_line = member_line.split("=", 1)[0].strip()
                member_line = member_line.split("{", 1)[0].strip()
                member_line = re.sub(r"^(?:static|constexpr|mutable|inline)\s+", "", member_line)

            if top_level_member_scope and member_line and "(" not in member_line:
                member_type = ""
                member_name = ""
                relation = "value"

                m_smart = SMART_MEMBER_LINE_RE.match(member_line)
                if m_smart:
                    member_type = normalize_type_for_display(m_smart.group(1))
                    member_name = m_smart.group(2)
                    relation = "smart"
                else:
                    m_ptrref = RAW_MEMBER_LINE_RE.match(member_line)
                    if m_ptrref:
                        member_type = normalize_type_for_display(m_ptrref.group(1))
                        member_name = m_ptrref.group(2)
                        relation = "ptrref"
                    else:
                        m_value = GENERIC_MEMBER_LINE_RE.match(member_line)
                        if m_value:
                            member_type = normalize_type_for_display(m_value.group(1))
                            member_name = m_value.group(2)
                            relation = "value"

                if member_type and member_name:
                    parsed.members.append((member_type, member_name, relation, current_access))

            brace_depth += body_line.count("{") - body_line.count("}")

            i += 1

        result.types[name] = parsed


def build_drawio_xml(result: ParseResult, title: str) -> str:
    class_names: List[str] = sorted(result.types.keys())
    class_set: Set[str] = set(class_names)
    access_symbol = {"public": "+", "private": "-", "protected": "#"}
    line_height_px = 14
    min_width = 80
    right_wall_offset_px = 8

    def is_interface(parsed: ParsedType) -> bool:
        if parsed.kind != "class" or not parsed.methods:
            return False

        pure_virtual_count = 0
        actionable_count = 0
        for method, _access in parsed.methods:
            compact = method.replace(" ", "")
            if (parsed.name + "(") in compact or ("~" + parsed.name + "(") in compact:
                continue

            method_stripped = method.strip()
            if method_stripped.startswith("static "):
                continue

            actionable_count += 1

            if "virtual" in method_stripped and "= 0" in method_stripped:
                pure_virtual_count += 1
                continue

            return False

        return actionable_count > 0 and pure_virtual_count == actionable_count

    def node_kind(parsed: ParsedType) -> str:
        if parsed.kind == "enum":
            return "enum"
        if parsed.kind == "struct":
            return "struct"
        if is_interface(parsed):
            return "interface"
        return "class"

    def node_style(kind: str) -> str:
        # Draw.io style tuned for readability and inspired by UML palette variants.
        if kind == "interface":
            return (
                "rounded=1;arcSize=8;whiteSpace=wrap;html=1;align=left;verticalAlign=top;"
                "spacing=4;fontFamily=Consolas;fontSize=9;strokeWidth=2;dashed=1;"
                "fillColor=#E8F1FF;strokeColor=#2F5597;fontColor=#1F2A44;"
            )
        if kind == "struct":
            return (
                "rounded=1;arcSize=8;whiteSpace=wrap;html=1;align=left;verticalAlign=top;"
                "spacing=4;fontFamily=Consolas;fontSize=9;strokeWidth=2;"
                "fillColor=#EAF7EA;strokeColor=#2E7D32;fontColor=#173A1A;"
            )
        if kind == "enum":
            return (
                "rounded=1;arcSize=8;whiteSpace=wrap;html=1;align=left;verticalAlign=top;"
                "spacing=4;fontFamily=Consolas;fontSize=9;strokeWidth=2;"
                "fillColor=#EEF1F5;strokeColor=#455A64;fontColor=#102027;"
            )
        # Class 5-inspired readable neutral style.
        return (
            "rounded=1;arcSize=8;whiteSpace=wrap;html=1;align=left;verticalAlign=top;"
            "spacing=4;fontFamily=Consolas;fontSize=9;strokeWidth=2;"
            "fillColor=#FFF4E5;strokeColor=#B35A00;fontColor=#3B2300;"
        )

    def node_stereotype(kind: str) -> str:
        if kind == "interface":
            return "<<interface>>"
        if kind == "struct":
            return "<<struct>>"
        if kind == "enum":
            return "<<enum>>"
        return "<<class>>"

    mxfile = ET.Element(
        "mxfile",
        {
            "host": "app.diagrams.net",
            "modified": datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
            "agent": "GitHub Copilot",
            "version": "24.0.0",
        },
    )
    diagram = ET.SubElement(mxfile, "diagram", {"id": "uml", "name": title})
    model = ET.SubElement(
        diagram,
        "mxGraphModel",
        {
            "dx": "1600",
            "dy": "1000",
            "grid": "1",
            "gridSize": "10",
            "guides": "1",
            "tooltips": "1",
            "connect": "1",
            "arrows": "1",
            "fold": "1",
            "page": "1",
            "pageScale": "1",
            "pageWidth": "2200",
            "pageHeight": "1600",
            "math": "0",
            "shadow": "0",
        },
    )

    root = ET.SubElement(model, "root")
    ET.SubElement(root, "mxCell", {"id": "0"})
    ET.SubElement(root, "mxCell", {"id": "1", "parent": "0"})

    # Build node rendering info first.
    node_infos: Dict[str, Tuple[str, str, int, int]] = {}

    for class_name in class_names:
        parsed = result.types[class_name]
        kind = node_kind(parsed)

        if kind == "enum":
            variables_lines = [truncate_for_display(f"+ {name}") for name in parsed.enum_values]
        else:
            variables_lines = [
                truncate_for_display(
                    f"{access_symbol.get(access, '~')} {member_name}: {compact_type_for_display(member_type)}"
                )
                for member_type, member_name, _relation, access in parsed.members
            ]
        method_lines = [
            truncate_for_display(f"{access_symbol.get(access, '~')} {compact_method_for_display(method)}")
            for method, access in parsed.methods
        ]

        def to_html_lines(items: List[str]) -> str:
            if not items:
                return "<div><i>(none)</i></div>"
            # Keep each attribute/method on a single visual line for readability.
            return "".join(
                f"<div style='white-space:nowrap; line-height:{line_height_px}px'>{item}</div>"
                for item in items
            )

        vars_html = to_html_lines(variables_lines)
        methods_html = to_html_lines(method_lines)

        content_candidates: List[str] = [
            class_name,
            node_stereotype(kind),
            *variables_lines,
            *method_lines,
            "(none)",
        ]
        longest_px = max((estimate_text_px(item) for item in content_candidates), default=50)
        width = max(min_width, longest_px + right_wall_offset_px)
        table_width = max(1, width - 6)

        label = (
            f"<table border='1' cellspacing='0' cellpadding='2' width='{table_width}px'>"
            f"<tr><td align='center'><b>{class_name}</b><br/><font color='#666666'>{node_stereotype(kind)}</font></td></tr>"
            f"<tr><td align='left'><b>Attributes</b><br/>{vars_html}</td></tr>"
            f"<tr><td align='left'><b>Methods</b><br/>{methods_html}</td></tr>"
            "</table>"
        )

        header_height = 34
        section_header_height = 18
        variables_block_height = max(line_height_px, line_height_px * max(1, len(variables_lines)))
        methods_block_height = max(line_height_px, line_height_px * max(1, len(method_lines)))
        padding = 10
        height = header_height + section_header_height + variables_block_height + section_header_height + methods_block_height + padding
        node_infos[class_name] = (label, node_style(kind), width, height)

    kind_by_name: Dict[str, str] = {
        name: node_kind(result.types[name])
        for name in class_names
    }
    layout_lane_width = max((info[2] for info in node_infos.values()), default=320)
    core_names = [name for name in class_names if kind_by_name[name] in {"class", "interface"}]
    data_names = [name for name in class_names if kind_by_name[name] in {"struct", "enum"}]

    relation_triples: List[Tuple[str, str, str]] = []
    for owner_name in class_names:
        owner = result.types[owner_name]
        for target_type, _member_name, relation, _access in owner.members:
            relation_target = extract_relation_target(target_type, relation)
            if relation_target not in class_set or relation_target == owner.name:
                continue
            relation_triples.append((owner.name, relation_target, relation))

    core_set = set(core_names)
    core_adj: Dict[str, Set[str]] = {name: set() for name in core_names}

    for class_name in core_names:
        base = result.types[class_name].base
        if base and base in core_set:
            core_adj[class_name].add(base)
            core_adj[base].add(class_name)

    for src, dst, _relation in relation_triples:
        if src in core_set and dst in core_set:
            core_adj[src].add(dst)
            core_adj[dst].add(src)

    core_degree: Dict[str, int] = {name: len(neighbors) for name, neighbors in core_adj.items()}

    components: List[List[str]] = []
    unvisited = set(core_names)
    while unvisited:
        start = min(unvisited)
        stack = [start]
        unvisited.remove(start)
        component: List[str] = []

        while stack:
            node = stack.pop()
            component.append(node)
            for neighbor in sorted(core_adj[node]):
                if neighbor in unvisited:
                    unvisited.remove(neighbor)
                    stack.append(neighbor)

        components.append(component)

    components.sort(key=lambda comp: (-len(comp), min(comp)))

    placement: Dict[str, Tuple[int, int]] = {}
    placed_order: List[str] = []

    base_x = 40
    base_y = 40
    row_gap = 28
    group_gap = 80

    # Connected core classes first, split into two vertical lanes by component.
    core_lane_x = [base_x, base_x + layout_lane_width + 180]
    core_lane_y = [base_y, base_y]
    kind_order = {"interface": 0, "class": 1, "struct": 2, "enum": 3}

    for comp in components:
        lane = 0 if core_lane_y[0] <= core_lane_y[1] else 1
        y_cursor = core_lane_y[lane]
        for name in sorted(
            comp,
            key=lambda n: (kind_order[kind_by_name[n]], -core_degree.get(n, 0), n),
        ):
            _label, _style, _width, height = node_infos[name]
            placement[name] = (core_lane_x[lane], y_cursor)
            placed_order.append(name)
            y_cursor += height + row_gap

        core_lane_y[lane] = y_cursor + group_gap

    # Keep data structures separate on the right and ordered by connectivity.
    data_links_to_core: Dict[str, int] = {name: 0 for name in data_names}
    for src, dst, _relation in relation_triples:
        if src in data_links_to_core and dst in core_set:
            data_links_to_core[src] += 1
        if dst in data_links_to_core and src in core_set:
            data_links_to_core[dst] += 1
    for name in data_names:
        base = result.types[name].base
        if base and base in core_set:
            data_links_to_core[name] += 1

    data_column_x = core_lane_x[-1] + layout_lane_width + 220
    data_second_column_x = data_column_x + layout_lane_width + 100
    data_column_break_y = max(core_lane_y) + 220
    data_y = base_y
    current_data_x = data_column_x

    for name in sorted(data_names, key=lambda n: (-data_links_to_core[n], n)):
        _label, _style, _width, height = node_infos[name]
        if data_y > data_column_break_y and current_data_x == data_column_x:
            current_data_x = data_second_column_x
            data_y = base_y
        placement[name] = (current_data_x, data_y)
        placed_order.append(name)
        data_y += height + row_gap

    # Fallback placement for any unclassified nodes.
    fallback_x = data_second_column_x + layout_lane_width + 120
    fallback_y = base_y
    for name in class_names:
        if name in placement:
            continue
        _label, _style, _width, height = node_infos[name]
        placement[name] = (fallback_x, fallback_y)
        placed_order.append(name)
        fallback_y += height + row_gap

    class_cell_ids: Dict[str, str] = {
        name: str(2 + idx)
        for idx, name in enumerate(class_names)
    }
    next_id = 2 + len(class_names)

    def add_edge(source: str, target: str, style: str, label: str = "") -> None:
        nonlocal next_id
        edge_cell = ET.SubElement(
            root,
            "mxCell",
            {
                "id": str(next_id),
                "value": label,
                "style": style,
                "edge": "1",
                "parent": "1",
                "source": class_cell_ids[source],
                "target": class_cell_ids[target],
            },
        )
        ET.SubElement(edge_cell, "mxGeometry", {"relative": "1", "as": "geometry"})
        next_id += 1

    # Emit edges first so nodes draw over them and remain readable.
    for class_name in class_names:
        parsed = result.types[class_name]
        if parsed.base and parsed.base in class_set:
            add_edge(class_name, parsed.base, "endArrow=block;endFill=0;html=1;edgeStyle=orthogonalEdgeStyle;rounded=0;", "inherits")

    emitted_edges: Set[Tuple[str, str, str]] = set()
    for owner_name, relation_target, relation in relation_triples:
        if relation == "smart":
            style = "endArrow=diamond;endFill=1;html=1;edgeStyle=orthogonalEdgeStyle;rounded=0;"
            label = "owns"
        elif relation == "ptrref":
            style = "endArrow=open;endFill=0;dashed=1;html=1;edgeStyle=orthogonalEdgeStyle;rounded=0;"
            label = "ref"
        else:
            style = "endArrow=open;endFill=0;html=1;edgeStyle=orthogonalEdgeStyle;rounded=0;"
            label = "uses"

        key = (owner_name, relation_target, relation)
        if key in emitted_edges:
            continue

        add_edge(owner_name, relation_target, style, label)
        emitted_edges.add(key)

    # Emit class nodes after edges so class cards remain on top.
    for class_name in placed_order:
        label, style, width, height = node_infos[class_name]
        x, y = placement[class_name]
        class_cell = ET.SubElement(
            root,
            "mxCell",
            {
                "id": class_cell_ids[class_name],
                "value": label,
                "style": style,
                "vertex": "1",
                "parent": "1",
            },
        )
        ET.SubElement(
            class_cell,
            "mxGeometry",
            {"x": str(x), "y": str(y), "width": str(width), "height": str(height), "as": "geometry"},
        )

    try:
        ET.indent(mxfile, space="  ")
    except AttributeError:
        pass

    return ET.tostring(mxfile, encoding="unicode")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate Draw.io XML UML class diagram from headers")
    parser.add_argument(
        "--root",
        type=pathlib.Path,
        default=pathlib.Path("."),
        help="Repository root to scan (default: current directory)",
    )
    parser.add_argument(
        "--drawio-output",
        type=pathlib.Path,
        default=pathlib.Path("docs/uml/codebase_class_diagram.drawio.xml"),
        help="Output Draw.io XML file path",
    )
    parser.add_argument(
        "--title",
        type=str,
        default="Valkron Engine Class Diagram",
        help="Diagram title",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = args.root.resolve()

    headers = discover_headers(root)
    if not headers:
        raise SystemExit("No header files found.")

    result = ParseResult()
    for header in headers:
        parse_header(header, result)

    drawio_text = build_drawio_xml(result, args.title)
    drawio_output_path = (root / args.drawio_output).resolve()
    drawio_output_path.parent.mkdir(parents=True, exist_ok=True)
    drawio_output_path.write_text(drawio_text, encoding="utf-8")

    print(f"Generated Draw.io XML: {drawio_output_path}")
    print(f"Parsed types: {len(result.types)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
