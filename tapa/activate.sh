# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

# Setup Python virtual environment and install the dependencies of tapa
# `source tapa/activate.sh` to activate the virtual environment

# Switch to the workspace of Bazel
cd $(bazel info workspace)

# Create virtual environment if not exist
[ -d .venv-tapa ] || python -m venv .venv-tapa

# Activate the virtual environment
source .venv-tapa/bin/activate

# Install the dependencies of tapa
python -m pip install -r tapa/requirements_lock.txt

# Export activated virtual environments
export VIRTUAL_ENV
export PATH
export PYTHONHOME
export PS1
