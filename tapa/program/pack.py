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
import tempfile
import zipfile

from tapa.program.abc import ProgramInterface
from tapa.program.directory import ProgramDirectoryInterface
from tapa.verilog.xilinx import pack

_logger = logging.getLogger().getChild(__name__)


class ProgramPackMixin(
    ProgramDirectoryInterface,
    ProgramInterface,
):
    """Mixin class providing RTL packing functionalities."""

    top: str

    def pack_xo(self, output_file: str) -> None:
        _logger.info("packaging RTL code")
        with contextlib.ExitStack() as stack:  # avoid nested with statement
            tmp_fp = stack.enter_context(tempfile.TemporaryFile())
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
            for info in sorted(packed_obj.infolist(), key=lambda x: x.filename):
                redacted_info = zipfile.ZipInfo(info.filename)
                redacted_info.compress_type = zipfile.ZIP_DEFLATED
                redacted_info.external_attr = info.external_attr
                output_fp.writestr(redacted_info, _redact(packed_obj, info))

        _logger.info("generated the v++ xo file at %s", output_file)


def _redact(xo: zipfile.ZipFile, info: zipfile.ZipInfo) -> bytes:
    content = xo.read(info)
    if not info.filename.endswith(".xml"):
        return content

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
