"""Work directory definition of TAPA."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import os
import os.path
from abc import abstractmethod
from typing import NamedTuple

from tapa.util import get_module_name
from tapa.verilog.xilinx.const import RTL_SUFFIX


class ReportPaths(NamedTuple):
    json: str
    yaml: str


class ProgramDirectoryInterface:
    """Interface class of the work directory structure definition."""

    work_dir: str  # Concrete class must instantiate this attribute

    @property
    @abstractmethod
    def rtl_dir(self) -> str:
        pass

    @property
    @abstractmethod
    def template_dir(self) -> str:
        pass

    @property
    @abstractmethod
    def report_dir(self) -> str:
        pass

    @property
    @abstractmethod
    def cpp_dir(self) -> str:
        pass

    @property
    @abstractmethod
    def report_paths(self) -> ReportPaths:
        pass

    @abstractmethod
    def get_cpp_path(self, name: str) -> str:
        pass

    @abstractmethod
    def get_common_path(self) -> str:
        pass

    @abstractmethod
    def get_header_path(self, name: str) -> str:
        pass

    @abstractmethod
    def get_post_syn_rpt_path(self, module_name: str) -> str:
        pass

    @abstractmethod
    def get_tar_path(self, name: str) -> str:
        pass

    @abstractmethod
    def get_rtl_path(self, name: str) -> str:
        pass

    @abstractmethod
    def get_rtl_template_path(self, name: str) -> str:
        pass


class ProgramDirectoryMixin(ProgramDirectoryInterface):
    """Mixin class providing the work directory structure definition."""

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
    def report_paths(self) -> ReportPaths:
        """Returns all formats of TAPA report as a namedtuple."""
        return ReportPaths(
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

    def get_tar_path(self, name: str) -> str:
        os.makedirs(os.path.join(self.work_dir, "tar"), exist_ok=True)
        return os.path.join(self.work_dir, "tar", name + ".tar")

    def get_rtl_path(self, name: str) -> str:
        return os.path.join(self.rtl_dir, get_module_name(name) + RTL_SUFFIX)

    def get_rtl_template_path(self, name: str) -> str:
        os.makedirs(self.template_dir, exist_ok=True)
        return os.path.join(self.template_dir, name + RTL_SUFFIX)
