AREA_OF_ASYNC_MMAP = {
    32: {
        'BRAM': 0,
        'DSP': 0,
        'FF': 377,
        'LUT': 786,
        'URAM': 0,
    },
    64: {
        'BRAM': 0,
        'DSP': 0,
        'FF': 375,
        'LUT': 848,
        'URAM': 0,
    },
    128: {
        'BRAM': 0,
        'DSP': 0,
        'FF': 373,
        'LUT': 971,
        'URAM': 0,
    },
    256: {
        'BRAM': 0,
        'DSP': 0,
        'FF': 371,
        'LUT': 1225,
        'URAM': 0,
    },
    512: {
        'BRAM': 0,
        'DSP': 0,
        'FF': 369,
        'LUT': 1735,
        'URAM': 0,
    },
    1024: {
        'BRAM': 0,
        'DSP': 0,
        'FF': 367,
        'LUT': 2755,
        'URAM': 0,
    },
}

# FIXME: currently assume 512 bit width
AREA_PER_HBM_CHANNEL = {
    'LUT': 5000,
    'FF': 6500,
    'BRAM': 0,
    'URAM': 0,
    'DSP': 0,
}

ZERO_AREA = {
    'LUT': 0,
    'FF': 0,
    'BRAM': 0,
    'URAM': 0,
    'DSP': 0,
}

# default pipeline level for control signals
DEFAULT_REGISTER_LEVEL = 3

SUPPORTED_PART_NUM_PREFIXS = (
  'xcu280-',
  'xcu250-',
)

def get_zero_area():
  return ZERO_AREA

def get_hbm_controller_area():
  """ area of one hbm controller """
  return AREA_PER_HBM_CHANNEL

def get_async_mmap_area(data_channel_width: int):
  return AREA_OF_ASYNC_MMAP[_next_power_of_2(data_channel_width)]

def _next_power_of_2(x):
  return 1 if x == 0 else 2**(x - 1).bit_length()

def get_ctrl_instance_region(part_num: str) -> str:
  if part_num.startswith('xcu250-') or part_num.startswith('xcu280-'):
    return 'COARSE_X1Y0'
  raise NotImplementedError(f'unknown {part_num}')

def get_port_region(part_num: str, port_cat: str, port_id: int) -> str:
  """
  return the physical location of a given port
  refer to the Vitis platforminfo command
  """
  if part_num.startswith('xcu280-'):
    if port_cat == 'HBM' and 0 <= port_id < 32:
      return f'COARSE_X{port_id // 16}Y0'
    if port_cat == 'DDR' and 0 <= port_id < 2:
      return f'COARSE_X1Y{port_id}'
    if port_cat == 'PLRAM' and 0 <= port_id < 6:
      return f'COARSE_X1Y{int(port_id/2)}'

  elif part_num.startswith('xcu250-'):
    if port_cat == 'DDR' and 0 <= port_id < 4:
      return f'COARSE_X1Y{port_id}'
    if port_cat == 'PLRAM' and 0 <= port_id < 4:
      return f'COARSE_X1Y{port_id}'

  raise NotImplementedError(f'unknown port_cat {port_cat}, port_id {port_id} for {part_num}')

def get_slr_count(part_num: str):
  if part_num.startswith('xcu280-'):
    return 3
  elif part_num.startswith('xcu250-'):
    return 4
  else:
    raise NotImplementedError('unknown part_num %s', part_num)

def is_part_num_supported(part_num: str):
  return any(part_num.startswith(prefix) for prefix in SUPPORTED_PART_NUM_PREFIXS)
