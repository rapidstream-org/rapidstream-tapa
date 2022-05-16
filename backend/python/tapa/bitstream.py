import logging
import os

_logger = logging.getLogger().getChild(__name__)

VITIS_COMMAND_BASIC = [
  'v++ ${DEBUG} \\',
  '  --link \\',
  '  --output "${OUTPUT_DIR}/${TOP}_${PLATFORM}.xclbin" \\',
  '  --kernel ${TOP} \\',
  '  --platform ${PLATFORM} \\',
  '  --target ${TARGET} \\',
  '  --report_level 2 \\',
  '  --temp_dir "${OUTPUT_DIR}/${TOP}_${PLATFORM}.temp" \\',
  '  --optimize 3 \\',
  '  --connectivity.nk ${TOP}:1:${TOP} \\',
  '  --save-temps \\',
  '  "${XO}" \\',
  '  --vivado.synth.jobs ${MAX_SYNTH_JOBS} \\',
  '  --vivado.prop=run.impl_1.STEPS.PHYS_OPT_DESIGN.IS_ENABLED=1 \\',
  '  --vivado.prop=run.impl_1.STEPS.OPT_DESIGN.ARGS.DIRECTIVE=$STRATEGY \\',
  '  --vivado.prop=run.impl_1.STEPS.PLACE_DESIGN.ARGS.DIRECTIVE=$PLACEMENT_STRATEGY \\',
  '  --vivado.prop=run.impl_1.STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE=$STRATEGY \\',
  '  --vivado.prop=run.impl_1.STEPS.ROUTE_DESIGN.ARGS.DIRECTIVE=$STRATEGY \\',
]
FLOORPLAN_OPTION = ['  --vivado.prop=run.impl_1.STEPS.OPT_DESIGN.TCL.PRE=$CONSTRAINT \\']
CONFIG_OPTION = ['  --config "${CONFIG_FILE}" \\']
CLOCK_OPTION = ['  --kernel_frequency ${TARGET_FREQUENCY} \\']

CHECK_CONSTRAINT = """
# check that the floorplan tcl exists
if [ ! -f "$CONSTRAINT" ]; then
    echo "no constraint file found"
    exit
fi
"""

NEWLINE = ['']

def get_vitis_script(args) -> str:
  """ generate v++ commands to run implementation """
  script = []
  script.append('#!/bin/bash')

  vitis_command = VITIS_COMMAND_BASIC

  script.append('TARGET=hw')
  script.append('# TARGET=hw_emu')
  script.append('# DEBUG=-g')
  script += NEWLINE

  script.append(f'TOP={args.top}')
  script.append(f'XO=\'{os.path.abspath(args.output_file)}\'')

  if args.floorplan_output:
    script.append(f'CONSTRAINT=\'{os.path.abspath(args.floorplan_output.name)}\'')
    script.append(CHECK_CONSTRAINT)
    vitis_command += FLOORPLAN_OPTION

  new_config_path = f'{os.path.abspath(args.work_dir)}/link_config.ini'
  if args.enable_hbm_binding_adjustment and os.path.exists(new_config_path):
    _logger.info(f'use the new connectivity configuration at {new_config_path} in the v++ script')
    script.append(f'CONFIG_FILE=\'{new_config_path}\'')
    vitis_command += CONFIG_OPTION
  elif args.connectivity:
    orig_config_path = os.path.abspath(args.connectivity.name)
    _logger.info(f'use the original connectivity configuration at {orig_config_path} in the v++ script')
    script.append(f'CONFIG_FILE=\'{orig_config_path}\'')
    vitis_command += CONFIG_OPTION
  else:
    _logger.warning('No connectivity file is provided, skip this part in the v++ script.')

  # if not specified in tapac, use platform default
  if args.clock_period:
    freq_mhz = round(1000/float(args.clock_period))
    script.append(f'TARGET_FREQUENCY={freq_mhz}')
    vitis_command += CLOCK_OPTION
  else:
    script.append(f'>&2 echo "Using the default clock target of the platform."')

  # if platform not specified in tapac, need to manually add it
  if args.platform:
    platform = args.platform
    script.append(f'PLATFORM={platform}')
  else:
    script.append(f'PLATFORM=""')
    warning_msg = 'Please edit this file and set a valid PLATFORM= on line "${LINENO}"'
    script.append(f'if [ -z $PLATFORM ]; then echo {warning_msg}; exit; fi')
    script += NEWLINE

  script.append(r'OUTPUT_DIR="$(pwd)/vitis_run_${TARGET}"')
  script += NEWLINE

  script.append(r'MAX_SYNTH_JOBS=8')
  script.append(r'STRATEGY="Explore"')
  script.append(r'PLACEMENT_STRATEGY="EarlyBlockPlacement"')
  script += NEWLINE

  script += vitis_command
  script += NEWLINE

  return '\n'.join(script)
