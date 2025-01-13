"""Custom rule to test Python modules with pytest."""

# Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

load("@rules_python//python:defs.bzl", _py_test = "py_test")
load("@tapa_deps//:requirements.bzl", "requirement")

def py_test(name, srcs, deps = [], args = [], **kwargs):
    _py_test(
        name = name,
        main = "//bazel:pytest_wrapper.py",
        srcs = srcs + ["//bazel:pytest_wrapper.py"],
        deps = deps + [requirement("pytest")],
        args = args + ["$(location %s)" % x for x in srcs],
        **kwargs
    )
