# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

rapidstream-tapaopt \
    --work-dir run \
    --tapa-xo-path ./sextans1613-u55c-333MHz.xo \
    --device-config ./u55c_device.json \
    --floorplan-config ./floorplan_config.json \
    --connectivity-ini ./link_config.ini \
    --implementation-config ./impl_config.json \
    --pp-scheme single_h_double_v \
    --run-impl \
    # --extract-metrics \
    # --skip-preprocess \
    # --skip-partition \
    # --skip-add-pipeline \
    # --skip-export \
    # --setup-single-slot-eval \
    # --single-reg \
