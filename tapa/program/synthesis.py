__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import itertools
import logging
import os
import sys
from concurrent import futures

import psutil

from tapa.backend.report.xilinx.rtl.generator import ReportDirUtil
from tapa.backend.report.xilinx.rtl.parser import (
    HierarchicalUtilization,
    parse_hierarchical_utilization_report,
)
from tapa.program.abc import ProgramInterface

_logger = logging.getLogger().getChild(__name__)


class ProgramSynthesisMixin(ProgramInterface):
    """Mixin class providing RTL synthesis functionalities."""

    def generate_post_synth_util(
        self,
        part_num: str,
        jobs: int | None,
    ) -> None:
        """Generates post-synthesis resource utilization for each task.

        Args:
            part_num (str): Part number of the target device.
            jobs (int | None): Number of parallel jobs. If None, infer from core count.
        """

        def worker(module_name: str, idx: int) -> HierarchicalUtilization:
            _logger.debug("synthesizing %s", module_name)
            rpt_path = self.get_post_syn_rpt_path(module_name)

            rpt_path_mtime = 0.0
            if os.path.isfile(rpt_path):
                rpt_path_mtime = os.path.getmtime(rpt_path)

            # generate report if and only if C++ source is newer than report.
            if os.path.getmtime(self.get_cpp_path(module_name)) > rpt_path_mtime:
                os.nice(idx % 19)
                with ReportDirUtil(
                    self.rtl_dir,
                    rpt_path,
                    module_name,
                    part_num,
                    synth_kwargs={"mode": "out_of_context"},
                ) as proc:
                    stdout, stderr = proc.communicate()

                # err if output report does not exist or is not newer than previous
                if (
                    not os.path.isfile(rpt_path)
                    or os.path.getmtime(rpt_path) <= rpt_path_mtime
                ):
                    sys.stdout.write(stdout.decode("utf-8"))
                    sys.stderr.write(stderr.decode("utf-8"))
                    msg = f"failed to generate report for {module_name}"
                    raise ValueError(msg)

            with open(rpt_path, encoding="utf-8") as rpt_file:
                return parse_hierarchical_utilization_report(rpt_file)

        worker_num = jobs or psutil.cpu_count(logical=False) or 8
        _logger.info("generating post-synthesis resource utilization reports")
        _logger.info(
            "this step runs logic synthesis of each task "
            "for accurate area info and may take a while"
        )
        _logger.info(
            "spawn %d workers for parallel logic synthesis",
            worker_num,
        )
        with futures.ThreadPoolExecutor(max_workers=worker_num) as executor:
            for utilization in executor.map(
                worker,
                {x.task.name for x in self.top_task.instances},
                itertools.count(0),
            ):
                # override self_area populated from HLS report
                bram = int(utilization["RAMB36"]) * 2 + int(utilization["RAMB18"])
                self.get_task(utilization.instance).total_area = {
                    "BRAM_18K": bram,
                    "DSP": int(utilization["DSP Blocks"]),
                    "FF": int(utilization["FFs"]),
                    "LUT": int(utilization["Total LUTs"]),
                    "URAM": int(utilization["URAM"]),
                }
