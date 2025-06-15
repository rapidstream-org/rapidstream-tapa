#!/bin/bash

# Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

# Tests that nuitka binary runs.

set -ex

"$1" --help
"$1" --version
