"""Work directory definition of TAPA."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import os
import os.path
from typing import NamedTuple

from tapa.util import get_module_name
from tapa.verilog.xilinx.const import RTL_SUFFIX


class ProgramDirectoryMixin:
    """Mixin class providing the work directory structure definition."""

    work_dir: str  # Concrete class must instantiate this attribute

    @property
    def rtl_dir(self) -> str:
        return os.path.join(self.work_dir, "hdl")

    @property
    def template_dir(self) -> str:
        return os.path.join(self.work_dir, "template")

    @property
    def report_dir(self) -> str:
        return os.path.join(self.work_dir, "report")

    @property
    def cpp_dir(self) -> str:
        cpp_dir = os.path.join(self.work_dir, "cpp")
        os.makedirs(cpp_dir, exist_ok=True)
        return cpp_dir

    @property
    def report(self):
        # ruff: noqa: ANN201
        """Returns all formats of TAPA report as a namedtuple."""

        class Report(NamedTuple):
            json: str
            yaml: str

        return Report(
            json=os.path.join(self.work_dir, "report.json"),
            yaml=os.path.join(self.work_dir, "report.yaml"),
        )

    def get_cpp_path(self, name: str) -> str:
        return os.path.join(self.cpp_dir, name + ".cpp")

    def get_common_path(self) -> str:
        return os.path.join(self.cpp_dir, "common.h")

    def get_header_path(self, name: str) -> str:
        return os.path.join(self.cpp_dir, name + ".h")

    def get_post_syn_rpt_path(self, module_name: str) -> str:
        return os.path.join(self.report_dir, f"{module_name}.hier.util.rpt")

    def get_tar(self, name: str) -> str:
        os.makedirs(os.path.join(self.work_dir, "tar"), exist_ok=True)
        return os.path.join(self.work_dir, "tar", name + ".tar")

    def get_rtl(self, name: str, prefix: bool = True) -> str:
        return os.path.join(
            self.rtl_dir,
            (get_module_name(name) if prefix else name) + RTL_SUFFIX,
        )

    def get_rtl_template(self, name: str) -> str:
        os.makedirs(self.template_dir, exist_ok=True)
        return os.path.join(
            self.template_dir,
            name + RTL_SUFFIX,
        )
