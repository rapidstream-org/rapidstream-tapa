# ruff: noqa: INP001

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from zipfile import ZipFile

import pytest
from python.runfiles import Runfiles  # type: ignore[reportMissingImports]

_TESTDATA_PATH = "_main/tests/functional/report/enable-synth-util.xo"
_RUNFILES = Runfiles.Create()
assert _RUNFILES is not None
_XO_PATH = _RUNFILES.Rlocation(_TESTDATA_PATH)
assert _XO_PATH is not None
_XO_ZIPFILE = ZipFile(_XO_PATH)


@pytest.mark.parametrize(
    "report_file",
    [
        "report.json",
        "report.yaml",
        "report/Add/Add_csynth.xml",
        "report/Mmap2Stream/Impl_csynth.xml",
        "report/Mmap2Stream/Mmap2Stream_csynth.xml",
        "report/Stream2Mmap/Impl_csynth.xml",
        "report/Stream2Mmap/Stream2Mmap_csynth.xml",
        "report/VecAdd/VecAdd_csynth.xml",
    ],
)
def test_report_file_exists(report_file: str) -> None:
    assert report_file in _XO_ZIPFILE.namelist()
