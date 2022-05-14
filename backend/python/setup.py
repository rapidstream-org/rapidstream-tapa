"""Extending High-Level Synthesis for Task-Parallel Programs.

See:
https://github.com/UCLA-VAST/tapa
"""

from setuptools import find_packages, setup

with open('README.md', encoding='utf-8') as f:
  long_description = f.read()

with open('tapa/VERSION') as f:
  version = f.read().strip()

setup(
    name='tapa',
    version=version,
    description='Extending High-Level Synthesis for Task-Parallel Programs',
    long_description=long_description,
    long_description_content_type='text/markdown',
    url='https://github.com/UCLA-VAST/tapa',
    author='Blaok Chi',
    classifiers=[
        'Development Status :: 2 - Pre-Alpha',
        'Intended Audience :: Developers',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3 :: Only',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Topic :: System :: Hardware',
    ],
    packages=find_packages(),
    python_requires='>=3.6',
    install_requires=[
        'absl-py',
        'autobridge>=0.0.20220512.dev1',
        'coloredlogs>=9.3',
        'haoda>=0.0.20220507.dev1',
        'pyverilog>=1.2.0',
        'pyyaml>=5.1',
        'tapa-fast-cosim>=0.0.20220514.dev1',
        'toposort',
        'click>=7.1.2',
    ],
    entry_points={
        'console_scripts': [
            'tapa=tapa.tapa:entry_point', 'tapac=tapa.tapac:main',
            'tapav=tapa.tapav:main'
        ],
    },
    include_package_data=True,
)
