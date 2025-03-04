"""Unit tests for tapa.util."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from unittest import mock

import tapa.util


@mock.patch("shutil.which")
def test_clang_format_is_skipped_on_large_code(which_mock: mock.Mock) -> None:
    unformatted_code = "unformatted_code"
    formatted_code = "formatted_code"
    expected_which_argument = "clang-format-20"
    tapa.util.Options.clang_format_quota_in_bytes = len(unformatted_code)
    which_mock.return_value = "/path/to/clang-format"

    # First call returns formatted code.
    with mock.patch("subprocess.check_output") as check_output_mock:
        check_output_mock.return_value = formatted_code

        assert tapa.util.clang_format(unformatted_code) == formatted_code

        which_mock.assert_called_with(expected_which_argument)
        check_output_mock.assert_called_once()

    # Second call returns unformatted code because it runs out of quota.
    with mock.patch("subprocess.check_output") as check_output_mock:
        which_mock.return_value = "/path/to/clang-format"

        assert tapa.util.clang_format(unformatted_code) == unformatted_code

        which_mock.assert_called_with(expected_which_argument)
        check_output_mock.assert_not_called()
