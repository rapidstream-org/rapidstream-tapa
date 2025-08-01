# ruff: noqa: INP001

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import json
from zipfile import ZipFile

import pytest
import yaml
from python.runfiles import Runfiles  # type: ignore[reportMissingImports]

_TESTDATA_PATH = "_main/tests/functional/report/enable-synth-util.xo"
_RUNFILES = Runfiles.Create()
assert _RUNFILES is not None
_XO_PATH = _RUNFILES.Rlocation(_TESTDATA_PATH)
assert _XO_PATH is not None
_XO_ZIPFILE = ZipFile(_XO_PATH)


@pytest.mark.parametrize("module", ["Add", "Mmap2Stream", "Stream2Mmap"])
@pytest.mark.parametrize(
    "report",
    [
        json.load(_XO_ZIPFILE.open("report.json")),
        yaml.load(_XO_ZIPFILE.open("report.yaml"), yaml.Loader),
    ],
    ids=["json", "yaml"],
)
def test_module_has_synth_report(module: str, report: dict) -> None:
    assert report["area"]["breakdown"][module]["area"]["source"] == "synth"
