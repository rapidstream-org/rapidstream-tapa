import io
import logging
import json
import os
import subprocess
import tempfile
from typing import List, Dict, Any

_logger = logging.getLogger().getChild(__name__)

DSE_TARGET_OPTIONS = (
  'min_area_limit',
  'max_area_limit',
  'min_slr_width_limit',
  'max_slr_width_limit',
)

TAPA_STEP_OPTIONS = (
  'run_tapacc',
  'run_hls',
  'generate_task_rtl',
  'run_floorplanning',
  'generate_top_rtl',
  'pack_xo',
)


def to_tapac_option(word: str) -> str:
  return '--' + word.replace('_', '-')


def get_basic_tapa_command(args_dict: Dict[str, Any], output_dir: str) -> List[str]:
  """Filter out certain options and convert format for subprocess tapac call"""
  cmd = ['tapac']

  for k, v in args_dict.items():
    if k in DSE_TARGET_OPTIONS + TAPA_STEP_OPTIONS:
      continue
    if k == 'work_dir':
      continue
    if k == 'run_floorplan_dse':
      continue
    # filter args with default empty list
    if isinstance(v, list) and not v:
      continue
    if k == 'input_file':
      continue
    if isinstance(v, bool) and not v:
      continue

    # convert args of file type
    if isinstance(v, io.IOBase):
      v = v.name

    # for store_true/store_false options
    if isinstance(v, bool):
      cmd += [to_tapac_option(k)]
    elif k == 'output_file':
      # handle the case if the provided output name is hierarchical
      output_name = str(v).split('/')[-1]
      cmd += ['-o', output_dir + '/' + output_name]
    elif k == 'floorplan_output':
      output_name = str(v).split('/')[-1]
      cmd += [to_tapac_option(k), output_dir + '/' +output_name]
    # these two are of "append" type, convert list to multiple invokation
    elif k == 'write_only_args' or k == 'read_only_args':
      for regexp in v:
        cmd += [to_tapac_option(k), regexp]
    # no conversion needed
    else:
      cmd += [to_tapac_option(k), str(v)]

  # the last option is the input source file
  cmd += [args_dict['input_file']]

  return cmd


def get_cmd_to_csynth_tasks(
    args_dict: Dict[str, Any],
    work_dir: str,
    output_dir: str,
) -> List[str]:
  """Before the actual DSE, run one pass of hls synthesis and enable_synth_util"""
  basic_cmd = get_basic_tapa_command(args_dict, output_dir)
  basic_cmd += [
    '--work-dir', work_dir,
    '--run-tapacc',
    '--run-hls',
    '--generate-task-rtl',
  ]
  return basic_cmd


def get_cmd_to_explore_floorplan(
    args_dict: Dict[str, Any],
    work_dir: str,
    output_dir: str,
    min_slr_limit: int,
    max_slr_limit: int,
    min_area_limit: float,
    max_area_limit: float,
) -> List[str]:
  """Command to get one design point. Reuse HLS synthesis and enable_synth_util"""
  basic_cmd = get_basic_tapa_command(args_dict, output_dir)
  basic_cmd += [
    '--work-dir', work_dir,
    '--generate-task-rtl',
    '--run-floorplanning',
    '--generate-top-rtl',
    '--pack-xo',
    '--min-slr-width-limit', str(min_slr_limit),
    '--max-slr-width-limit', str(max_slr_limit),
    '--min-area-limit', str(min_area_limit),
    '--max-area-limit', str(max_area_limit),
  ]
  return basic_cmd


def run_floorplan_dse(args):
  """Self-invoke tapac to generate multiple floorplan outputs"""
  # remove the file opened in argparse
  args.floorplan_output.close()
  os.remove(args.floorplan_output.name)

  args_dict = vars(args)
  args_dict = {k:v for k, v in args_dict.items() if v is not None}

  top_work_dir = args.work_dir
  try:
    os.makedirs(top_work_dir, exist_ok=True)
  except:
    _logger.error('Fail to make directory at %s. Check the --work-dir option.', top_work_dir)
    exit(1)

  dse_log('DSE preparation: synthesize each task into RTL')

  hls_dir = os.path.abspath(os.path.join(top_work_dir, 'csynth'))
  with tempfile.TemporaryDirectory() as temp_dir:
    hls_cmd = get_cmd_to_csynth_tasks(args_dict, hls_dir, temp_dir)
    try:
      subprocess.run(hls_cmd,
                      stdout=subprocess.PIPE,
                      check=True,
                      universal_newlines=True)
    except subprocess.CalledProcessError as e:
      _logger.error('Fail to run command: %s', ' '.join(hls_cmd) )
      exit(1)

  # start exploring the pareto optimal curve
  min_slr_limit = args.min_slr_width_limit
  max_slr_limit = args.max_slr_width_limit
  min_area_limit = args.min_area_limit
  max_area_limit = args.max_area_limit
  id = 1

  while min_slr_limit < max_slr_limit:
    dse_log(f'Start generating design point {id} with --max-slr-width-limit {max_slr_limit}')

    output_dir = f'{top_work_dir}/run-{id}'
    run_dir = f'{output_dir}/run'

    os.makedirs(run_dir, exist_ok=True)

    # reuse the HLS and RTL synthesis results
    try:
      subprocess.run(['rsync', '-az', hls_dir+'/', run_dir],
                    stdout=subprocess.PIPE,
                    check=True,
                    universal_newlines=True)
    except:
      _logger.error('Failed to run rsync from %s to %s. Check if rsync is installed', hls_dir, run_dir)
      break

    # run floorplanning
    run_cmd = get_cmd_to_explore_floorplan(
      args_dict,
      run_dir,
      output_dir,
      min_slr_limit,
      max_slr_limit,
      min_area_limit,
      max_area_limit
    )
    try:
      subprocess.run(run_cmd,
                    stdout=subprocess.PIPE,
                    check=True,
                    universal_newlines=True)
    except:
      _logger.error('Fail to run command: %s', ' '.join(run_cmd))
      exit(1)

    # extract the floorplan metrics
    with open(f'{top_work_dir}/run-{id}/run/autobridge/post-floorplan-config.json', 'r') as config_json:
      config = json.load(config_json)
      if config['floorplan_status'] != 'SUCCEED':
        _logger.info(f'Failed to generate design point {id} with --max-slr-width-limit {max_slr_limit}')
        break

      actual_slr_width = config['actual_slr_width_usage']
      actual_area_usage = config['actual_area_usage']

    # in the next iteration we decrease the slr limit, so the area usage
    # could only be higher
    min_area_limit = max(min_area_limit, actual_area_usage)

    # the slr limit in the next iteration will be at least explore_step smaller
    max_slr_limit = min(max_slr_limit - args.floorplan_dse_step, actual_slr_width)

    id += 1

  dse_log('Floorplan DSE Finished.')


def dse_log(msg: str):
  _logger.info('')
  _logger.info('-----------------------------------------------------------------------------')
  _logger.info(msg)
  _logger.info('-----------------------------------------------------------------------------')
  _logger.info('')
