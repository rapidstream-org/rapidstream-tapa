"""Generate floorplan slot cpp for hls synth."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import re
from string import Template

_SCALAR_PRAGMA = (
    "#pragma HLS interface ap_none port = {name} register\n"
    "  {{ auto val = reinterpret_cast<volatile uint8_t &>({name}); }}"
)
_FIFO_IN_PRAGMA = (
    "#pragma HLS disaggregate variable = {name}\n"
    "#pragma HLS interface ap_fifo port = {name}._\n"
    "#pragma HLS aggregate variable = {name}._ bit\n"
    "  void({name}._.empty());\n"
    "  {{ auto val = {name}.read(); }}\n"
)
_FIFO_OUT_PRAGMA = (
    "#pragma HLS disaggregate variable = {name}\n"
    "#pragma HLS interface ap_fifo port = {name}._\n"
    "#pragma HLS aggregate variable = {name}._ bit\n"
    "  void({name}._.full());\n"
    "  {name}.write({type}());"
)

_PRAGMA = {
    "scalar": _SCALAR_PRAGMA,
    "mmap": _SCALAR_PRAGMA,
    "istream": _FIFO_IN_PRAGMA,
    "ostream": _FIFO_OUT_PRAGMA,
    "istreams": _FIFO_IN_PRAGMA,
    "ostreams": _FIFO_OUT_PRAGMA,
}

_SCALAR_PORT_TEMPLATE = "{type} {name}"
_PORT_TEMPLATE = {
    "istream": "tapa::istream<{type}>& {name}",
    "ostream": "tapa::ostream<{type}>& {name}",
    "istreams": "tapa::istream<{type}>& {name}",
    "ostreams": "tapa::ostream<{type}>& {name}",
    "scalar": _SCALAR_PORT_TEMPLATE,
    "mmap": _SCALAR_PORT_TEMPLATE,
    "async_mmap": _SCALAR_PORT_TEMPLATE,
}
_DEF_TEMPLATE = """void $name($ports) {
    $pragma
}
"""

_DECL_TEMPLATE = """void $name($ports);
"""


def gen_slot_cpp(slot_name: str, top_name: str, ports: list, top_cpp: str) -> str:
    """Generate floorplan slot cpp for hls synth.

    slot_name: Name of the slot
    ports: List of ports in the slot. Each port should match port format
        in tapa graph dict.
        e.g.
        {
            "cat": "istream",
            "name": "a",
            "type": "float",
            "width": 32
        }
    """

    cpp_ports = []
    cpp_pragmas = []
    for port in ports:
        assert isinstance(port, dict)
        assert "cat" in port
        assert "name" in port
        assert "type" in port
        assert "width" in port
        assert port["cat"] in _PORT_TEMPLATE
        port_type = port["type"]
        port_cat = port["cat"]

        # if "name[idx]" exists, replace it with "name_idx"
        match = re.fullmatch(r"([a-zA-Z_]\w*)\[(\d+)\]", port["name"])
        if match:
            n, i = match.groups()
            name = f"{n}_{i}"
        elif "[" in port and "]" in port:
            msg = f"Invalid port index in '{port}': must be a numeric index."
            raise ValueError(msg)
        else:
            name = port["name"]

        # when port is an array, find array element type
        # TODO: fix scalar cat due to mmap/streams
        if port_cat == "scalar":
            match = re.search(r"(?:tapa::)?(\w+)<([^,>]+)", port_type)
            if match:
                port_cat = match.group(1)
                port_type = match.group(2)

        # convert pointer to uint64_t
        if "*" in port_type:
            port_type = "uint64_t"

        cpp_ports.append(
            _PORT_TEMPLATE[port_cat].format(
                name=name,
                type=port_type,
            )
        )
        assert port_cat in _PRAGMA
        cpp_pragmas.append(_PRAGMA[port_cat].format(name=name, type=port_type))
        continue

    # Read template from file
    def_template = Template(_DEF_TEMPLATE)
    decl_template = Template(_DECL_TEMPLATE)

    # Substitute values
    new_def = def_template.substitute(
        name=slot_name,
        ports=", ".join(cpp_ports),
        pragma="\n".join(cpp_pragmas),
    )
    new_decl = decl_template.substitute(
        name=slot_name,
        ports=", ".join(cpp_ports),
    )

    return replace_function(
        top_cpp,
        top_name,
        new_decl,
        new_def,
    )


# Below are generated by AI


def remove_comments_and_strings(code: str) -> str:
    """
    Removes comments from C++ code, preserving strings and character literals.

    Args:
        code: The input C++ source code.

    Returns:
        A string with all comments replaced by spaces, leaving strings and chars intact.
    """

    def replacer(match: re.Match) -> str:
        s = match.group(0)
        if s.startswith("/"):
            return " " * len(s)  # preserve spacing for indexing
        return s  # keep string literals untouched

    pattern = r"""
        //.*?$|              # single-line comments
        /\*.*?\*/|           # multi-line comments
        "(?:\\.|[^"\\])*"|   # double-quoted strings
        '(?:\\.|[^'\\])*'    # single-quoted characters
    """
    return re.sub(pattern, replacer, code, flags=re.DOTALL | re.MULTILINE)


# ruff: noqa: PLR0912, C901
def find_function_definition(
    code: str, func_name: str
) -> tuple[int | None, int | None]:
    """
    Finds the start and end index of the function definition in the code.

    Args:
        code: The full source code.
        func_name: Name of the function to locate.

    Returns:
        A tuple (start_index, end_index) of the function body, or (None, None)
        if not found.
    """
    code_no_comments = remove_comments_and_strings(code)

    pattern = rf"\bvoid\s+{re.escape(func_name)}\s*\(([^)]*)\)\s*\{{"
    match = re.search(pattern, code_no_comments)
    if not match:
        return None, None

    start = match.start()
    open_braces = 0
    in_comment = False
    i = match.end() - 1

    while i < len(code):
        c = code[i]
        if code[i : i + 2] == "/*":
            in_comment = True
            i += 2
            continue
        if code[i : i + 2] == "*/" and in_comment:
            in_comment = False
            i += 2
            continue
        if code[i : i + 2] == "//" and not in_comment:
            i = code.find("\n", i)
            if i == -1:
                break
            continue
        if c in {'"', "'"} and not in_comment:
            quote = c
            i += 1
            while i < len(code) and code[i] != quote:
                if code[i] == "\\":
                    i += 1  # Skip escaped characters
                i += 1
            i += 1
            continue

        if not in_comment:
            if c == "{":
                open_braces += 1
            elif c == "}":
                open_braces -= 1
                if open_braces == 0:
                    return start, i + 1
        i += 1

    return None, None


def replace_function_declaration(code: str, func_name: str, new_decl: str) -> str:
    """
    Replaces the function declaration of the given function name with a new one.

    Args:
        code: The full source code.
        func_name: The function whose declaration is to be replaced.
        new_decl: The new declaration string (e.g., `void foo(int a);`)

    Returns:
        The updated source code with the declaration replaced.
    """
    pattern = rf"\bvoid\s+{re.escape(func_name)}\s*\([^)]*\)\s*;"
    return re.sub(pattern, new_decl, code)


def replace_function_definition(code: str, func_name: str, new_def: str) -> str:
    """
    Replaces the full function definition of the given function.

    Args:
        code: The full source code.
        func_name: The function whose definition is to be replaced.
        new_def: The new function definition code block.

    Returns:
        The updated source code with the definition replaced.

    Raises:
        ValueError: If the function definition is not found.
    """
    start, end = find_function_definition(code, func_name)
    if start is None or end is None:
        msg = f"Function definition for '{func_name}' not found."
        raise ValueError(msg)
    return code[:start] + new_def + code[end:]


def replace_function(code: str, func_name: str, new_decl: str, new_def: str) -> str:
    """
    Replaces both the declaration and the definition of a function in a C++ source file.

    Args:
        code: Original source code.
        func_name: The function to replace.
        new_decl: The new declaration string.
        new_def: The new definition string.

    Returns:
        The updated code with both declaration and definition replaced.
    """
    code = replace_function_declaration(code, func_name, new_decl)
    return replace_function_definition(code, func_name, new_def)
