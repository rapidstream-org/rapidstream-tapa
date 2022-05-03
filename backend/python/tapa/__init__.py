import os.path

with open(os.path.join(os.path.dirname(__file__), 'VERSION')) as _fp:
  __version__ = _fp.read().strip()
