__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import doctest
import importlib
import os
import pkgutil
import unittest


def load_tests(loader, tests, ignore):  # noqa: ANN001, ANN201, ARG001
    package = importlib.import_module(os.environ["DOCTEST_PACKAGE"])
    for importer, module_name, is_package in pkgutil.walk_packages(
        path=package.__path__, prefix=package.__name__ + "."
    ):
        tests.addTests(doctest.DocTestSuite(importlib.import_module(module_name)))
    return tests


if __name__ == "__main__":
    unittest.main()
