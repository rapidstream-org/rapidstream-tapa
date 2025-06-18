__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import contextlib
import glob
import logging
import os
import os.path
import re
import shutil
import tempfile
import zipfile
from pathlib import Path

from yaml import safe_dump

from tapa.graphir.types import Project
from tapa.program.abc import ProgramInterface
from tapa.program.directory import ProgramDirectoryInterface
from tapa.verilog.graphir_exporter.main import export_design
from tapa.verilog.xilinx import pack

_logger = logging.getLogger().getChild(__name__)
graphir_export_path = "graphir_hdl"


class ProgramPackMixin(
    ProgramDirectoryInterface,
    ProgramInterface,
):
    """Mixin class providing RTL packing functionalities."""

    top: str

    def pack_xo(self, output_file: str, graphir_path: Path | None = None) -> None:
        """Pack program to xo.

        If graphir_path is provided, it will be exported and included in the xo file.
        """
        _logger.info("packaging RTL code")
        with contextlib.ExitStack() as stack:  # avoid nested with statement
            tmp_fp = stack.enter_context(tempfile.TemporaryFile())

            if graphir_path:
                with open(graphir_path, encoding="utf-8") as f:
                    project = Project.model_validate_json(f.read())

                full_export_path = Path(self.work_dir) / graphir_export_path
                export_design(
                    project,
                    str(full_export_path),
                )

                # merge the graphir export into the RTL directory
                for filename in os.listdir(full_export_path):
                    src_file = os.path.join(full_export_path, filename)
                    dst_file = os.path.join(self.rtl_dir, filename)
                    if os.path.isfile(src_file):
                        shutil.copy2(src_file, dst_file)

            pack(
                top_name=self.top,
                ports=self.top_task.ports.values(),
                rtl_dir=self.rtl_dir,
                part_num=self._get_part_num(self.top),
                output_file=tmp_fp,
            )
            tmp_fp.seek(0)

            _logger.info("packaging HLS report")
            packed_obj = stack.enter_context(zipfile.ZipFile(tmp_fp, "a"))
            output_fp = stack.enter_context(zipfile.ZipFile(output_file, "w"))
            for filename in self.report_paths:
                arcname = os.path.basename(filename)
                _logger.debug("  packing %s", arcname)
                packed_obj.write(filename, arcname)

            report_glob = os.path.join(glob.escape(self.report_dir), "*_csynth.xml")
            for filename in glob.iglob(report_glob):
                arcname = os.path.join("report", os.path.basename(filename))
                _logger.debug("  packing %s", arcname)
                packed_obj.write(filename, arcname)

            # redact timestamp, source location etc. to make xo reproducible
            _redact_and_zip(packed_obj, output_fp)

        _logger.info("generated the v++ xo file at %s", output_file)

    def pack_zip(self, output: str, **contexts: object) -> None:
        """Pack the generated RTL into a zip file.

        Args:
            output: The output zip file.
            contexts: Dict mapping name to context suitable for serialization.
        """
        _logger.info("packing the design into a zip file: %s", output)

        with (
            tempfile.TemporaryFile() as tmp_file,
            zipfile.ZipFile(tmp_file, "w") as tmp_zipf,
        ):
            _logger.info("adding the RTL to the zip file")
            all_files = Path(self.rtl_dir).glob("**")
            for file in all_files:
                if file.is_file():
                    tmp_zipf.write(file, f"rtl/{file.relative_to(self.rtl_dir)}")
                    _logger.debug("added %s to the zip file", file)

            _logger.info("adding the TAPA information to the zip file")
            for filename in [self.report_paths.yaml]:
                file = Path(filename)
                tmp_zipf.write(file, f"{file.name}")
                _logger.debug("added %s to the zip file", file)

            for name, content in contexts.items():
                tmp_zipf.writestr(f"{name}.yaml", safe_dump(content))
                _logger.debug("added %s.yaml to the zip name", name)

            _logger.info("adding the HLS reports to the zip file")
            report_files = Path(self.report_dir).glob("**/*_csynth.rpt")
            for file in report_files:
                tmp_zipf.write(file, f"report/{file.relative_to(self.report_dir)}")
                _logger.debug("added %s to the zip file", file)

            # redact timestamp, source location etc. to make zip reproducible
            _logger.info("generating the final zip file")
            with zipfile.ZipFile(output, "w") as output_zipf:
                _redact_and_zip(tmp_zipf, output_zipf)

        _logger.info("packed the design into a zip file: %s", output)


def _redact_and_zip(zip_in: zipfile.ZipFile, zip_out: zipfile.ZipFile) -> None:
    for info in sorted(zip_in.infolist(), key=lambda x: x.filename):
        redacted_info = zipfile.ZipInfo(info.filename)
        redacted_info.compress_type = zipfile.ZIP_DEFLATED
        redacted_info.external_attr = info.external_attr
        zip_out.writestr(redacted_info, _redact(zip_in, info))


def _redact(xo: zipfile.ZipFile, info: zipfile.ZipInfo) -> bytes:
    content = xo.read(info)
    if info.filename.endswith(".rpt"):
        return _redact_rpt(content)
    if info.filename.endswith(".xml"):
        return _redact_xml(content)
    return content


def _redact_rpt(content: bytes) -> bytes:
    text = content.decode()

    # Redact the timestamp stored in rpt. The same timestamp is used by files in
    # the xo zip file and is the earliest timestamp supported by the zip format.
    text = re.sub(
        r"Date:           ... ... .. ..:..:.. ....",
        r"Date:           Tue Jan 01 00:00:00 1980",
        text,
    )

    return text.encode()


def _redact_xml(content: bytes) -> bytes:
    # Although we only redact xml files, regex substitution seems more readable
    # than parsing and mutating xml. We'll add tests to prevent regression in
    # case of file format change.
    text = content.decode()

    # Redact the timestamp stored in xml. The same timestamp is used by files in
    # the xo zip file and is the earliest timestamp supported by the zip format.
    text = re.sub(
        r"<xilinx:coreCreationDateTime>....-..-..T..:..:..Z</xilinx:coreCreationDateTime>",
        r"<xilinx:coreCreationDateTime>1980-01-01T00:00:00Z</xilinx:coreCreationDateTime>",
        text,
    )

    # Redact the absolute path but keep the path relative to the work directory.
    text = re.sub(
        r"<SourceLocation>.*/(cpp/.*)</SourceLocation>",
        r"<SourceLocation>\1</SourceLocation>",
        text,
    )

    # Redact the random(?) project ID.
    text = re.sub(
        r'ProjectID="................................"',
        r'ProjectID="0123456789abcdef0123456789abcdef"',
        text,
    )

    return text.encode()
