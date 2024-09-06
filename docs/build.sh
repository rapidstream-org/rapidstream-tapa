# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

python3 \
    -m sphinx \
    --fail-on-warning \
    --keep-going \
    --show-traceback \
    --define language=en \
    . ../build/docs/html
