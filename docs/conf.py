import os
import os.path
import subprocess
import sys

project = 'TAPA'
author = ', '.join([
    'Yuze Chi',
    'Licheng Guo',
])
copyright = '2021, ' + author

html_theme = 'sphinx_rtd_theme'

extensions = [
    'breathe',  # Handles Doxygen output.
    'myst_parser',  # Handles MarkDown
    'sphinx.ext.autodoc',  # Handles Python docstring.
    'sphinx.ext.autosectionlabel',  # Creates labels automatically.
    'sphinx.ext.napoleon',  # Handles Google-stype docstring.
    'sphinxarg.ext',  # Handles Python entry points.
]

breathe_default_project = 'TAPA'

if os.environ.get('READTHEDOCS') == 'True':
  docs_dir = os.path.dirname(__file__)
  build_dir = f'{docs_dir}/../build/docs'
  doxyfile_out = f'{build_dir}/Doxyfile'
  doxygen_dir = f'{build_dir}/doxygen'
  os.makedirs(build_dir, exist_ok=True)
  with open(f'{docs_dir}/Doxyfile.in', 'r') as doxyfile_in_fp:
    with open(doxyfile_out, 'w') as doxyfile_out_fp:
      for line in doxyfile_in_fp:
        line = line.replace('@DOXYGEN_OUTPUT_DIR@', doxygen_dir)
        line = line.replace('@DOXYGEN_INPUT_DIR@', f'{docs_dir}/..')
        doxyfile_out_fp.write(line)
  subprocess.call(['doxygen', doxyfile_out])
  breathe_projects = {breathe_default_project: f'{doxygen_dir}/xml'}

# Make sure to use the tapa package shipped with the documentation.
sys.path.insert(0, os.path.dirname(__file__) + '/../backend/python')

# Make sure the target is unique.
autosectionlabel_prefix_document = True

# Generate labels for #, ##, and ### in markdown.
myst_heading_anchors = 3

source_suffix = ['.rst', '.md']
