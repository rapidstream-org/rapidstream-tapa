__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import logging
from glob import glob
from os import environ

import click

_logger = logging.getLogger(__name__)

GITHUB_JOB_SUMMARY = environ.get("GITHUB_STEP_SUMMARY", "/tmp/github-job-summary")


@click.command()
@click.option(
    "--run-dir",
    type=click.Path(file_okay=False, resolve_path=True),
    help="The path to a (set of) run(s).",
    required=True,
)
def report_qor(run_dir: str) -> None:
    """Report the QoRs of an implemented design."""
    report_freq(run_dir)


def report_freq(run_dir: str) -> None:
    """Report the Fmax of an implemented design."""
    _logger.warning("Regression metrics are stored in %s", GITHUB_JOB_SUMMARY)

    with open(GITHUB_JOB_SUMMARY, "a", encoding="utf-8") as log_f:
        for sol_dir in glob(f"{run_dir}/dse/solution_*"):
            log_f.write(f"\n\n## Solution: {sol_dir}\n\n")
            _logger.warning("## Solution: %s", sol_dir)

            # Get the timing report:
            # Use the one in the Vivado project since v++ only copies it to the
            # report directory when the timing is met. The project directory,
            # however, always has the timing report.
            timing_rpt_g = glob(
                f"{sol_dir}/**/link/vivado/vpl/**/*_timing_summary_postroute_physopted.rpt",
                recursive=True,
            )
            if len(timing_rpt_g) == 0:
                log_f.write("No timing report found.\n")
                _logger.critical("No timing report found for %s", sol_dir)
                continue
            if len(timing_rpt_g) > 1:
                log_f.write("Multiple timing reports found.\n")
                _logger.critical("Multiple timing reports found for %s", sol_dir)
                continue
            timing_rpt = timing_rpt_g[0]

            # Parse the Worst Negative Slack (WNS)
            with open(timing_rpt, encoding="utf-8") as timing_rpt_f:
                lines = timing_rpt_f.readlines()

                i = 0
                while i < len(lines):
                    # Parse the first line starting with `WNS(ns)`
                    # e.g.,
                    #   WNS(ns) ...
                    #   ------- ...
                    #   -0.556  ...
                    if lines[i].lstrip().startswith("WNS(ns)"):
                        i += 2
                        break
                    i += 1

                wns = float(lines[i].split()[0])

            # Get the target clock period from Vitis script
            run_vitis_sh_g = glob(f"{sol_dir}/run_vitis.sh")
            assert len(run_vitis_sh_g) == 1, (
                f"Expected one run_vitis.sh, now: {run_vitis_sh_g}."
            )
            run_vitis_sh = run_vitis_sh_g[0]

            with open(run_vitis_sh, encoding="utf-8") as vitis_script:
                lines = vitis_script.readlines()

                target = 0
                for line in lines:
                    # Parse the first line starting with `TARGET_FREQUENCY`
                    # the frequency unit is MHz
                    # e.g., TARGET_FREQUENCY=300
                    if line.strip().startswith("TARGET_FREQUENCY"):
                        target = float(line.split("=")[1])
                        break

                assert target != 0

            # Calculate the Fmax
            fmax = 1000 / ((1000 / target) - wns)
            log_f.write(f"Fmax: {fmax:.2f}\n")
            _logger.warning("Fmax: %.2f", fmax)


if __name__ == "__main__":
    report_qor().main(standalone_mode=False)  # pylint: disable=E1120
