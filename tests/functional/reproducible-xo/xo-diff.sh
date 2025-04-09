#!/bin/bash

# Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

# Tests if two zip files are identitical. Used for regression testing of
# reproducible xo generation. NOTE: $PWD/a/ and $PWD/b/ will be deleted.

set -ex

# Print details of each file for debugging purposes.
zipinfo "$1"
zipinfo "$2"

# Extract each file into their own directories.
rm -rf a/ b/
mkdir -p a/ b/
unzip -q "$1" -d a/
unzip -q "$2" -d b/

# Compare the content first and don't bother comparing the zip files if their
# contents are already different.
diff --report-identical-files --recursive a/ b/
diff --report-identical-files "$1" "$2"
