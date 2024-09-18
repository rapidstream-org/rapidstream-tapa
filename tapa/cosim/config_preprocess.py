__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import glob
import json
import logging
import os
import re
import sys

_logger = logging.getLogger().getChild(__name__)


def _update_relative_path(config: dict, config_path: str) -> None:
    """convert the relative path in the config file"""
    config_dir = "/".join(config_path.split("/")[:-1])

    curr_path = config["xo_path"]
    if not curr_path.startswith("/") and not curr_path.startswith("~"):
        config["xo_path"] = f"{config_dir}/{curr_path}"

    for axi_name, curr_path in config["axi_to_data_file"].items():
        if not curr_path.startswith("/") and not curr_path.startswith("~"):
            config["axi_to_data_file"][axi_name] = f"{config_dir}/{curr_path}"


def _parse_xo_update_config(config: dict, tb_output_dir: str) -> None:
    """
    Only supports TAPA xo. Vitis XO has different hierarchy and RTL coding style
    """
    xo_path = config["xo_path"]

    tmp_path = f"{tb_output_dir}/tapa_fast_cosim_{os.getuid()}/"
    os.system(f"rm -rf {tmp_path}/")
    os.system(f"mkdir -p {tmp_path}/")
    os.system(f"cp {xo_path} {tmp_path}/target.xo")
    os.system(f"unzip -q {tmp_path}/target.xo -d {tmp_path}/")

    # only supports tapa xo
    try:
        config["verilog_path"] = glob.glob(f"{tmp_path}/ip_repo/*/src")[0]
    except:  # noqa: E722
        _logger.error(
            "Fail to extract the xo. Please provide a correct TAPA XO. "
            "Note that Vitis XOs are not supported"
        )
        sys.exit(1)

    # extract other kernel information
    kernel_file_path = glob.glob(f"{tmp_path}/*/kernel.xml")[0]
    with open(kernel_file_path, encoding="utf-8") as fp:
        kernel_file = fp.read()

    config["top_name"] = re.search(r'kernel name="(\w+)"', kernel_file).group(1)

    # convert argument index in the config file to actual names
    arg_name_id_pair_list = re.findall(
        r'arg name="(\w+)" addressQualifier="\d" id="(\d+)"', kernel_file
    )
    id_to_name: dict[int, str] = {int(x): name for name, x in arg_name_id_pair_list}

    # update scalar arguments
    def change_id_to_name(id_to_val: dict[str, str]) -> dict[str, str]:
        return {
            id_to_name[int(scalar_arg_id)]: val
            for scalar_arg_id, val in id_to_val.items()
        }

    for entry in ("scalar_to_val", "axi_to_data_file", "axi_to_c_array_size"):
        config[entry] = change_id_to_name(config[entry] or {})


def _check_scalar_val_format(config: dict) -> None:
    for scalar, val in config["scalar_to_val"].items():
        assert val.startswith("'h"), (
            "scalar value should be written in hex format, lsb on the right, "
            "with the suffix 'h according to Verilog syntax. "
            f"Violation: {scalar}: {val}"
        )
        assert (
            len(val) <= 2 + 16
        ), f"scalar value should be at most 64 bit. Violation: {scalar}: {val}"


def preprocess_config(config_path: str, tb_output_dir: str) -> dict:
    with open(config_path, encoding="utf-8") as fp:
        config = json.load(fp)
    _update_relative_path(config, config_path)
    _parse_xo_update_config(config, tb_output_dir)
    _check_scalar_val_format(config)

    return config
