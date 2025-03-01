"""Unit tests for tapa.task."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.task import Task


def test_add_rs_pragmas_to_fsm_skips_unused_ports() -> None:
    task = Task(
        name="foo",
        code="",
        level="upper",
        ports=[
            {
                "cat": "scalar",
                "name": "n",
                "type": "int64_t",
                "width": 64,
            },
        ],
    )
    task.instances = ()

    task.add_rs_pragmas_to_fsm()

    ap_ctrl_pragma_lines = [
        line
        for line in task.fsm_module.code.splitlines()
        if line.strip().startswith("// pragma RS ap-ctrl ")
    ]
    assert len(ap_ctrl_pragma_lines) == 1
    assert "scalar" not in ap_ctrl_pragma_lines[0]
