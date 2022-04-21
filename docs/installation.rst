Install TAPA from Binary
------------------------

TAPA is developed and tested in Ubuntu.
If you use Ubuntu as well (highly recommended),
you can easily install TAPA from pre-built packages:

.. code-block:: bash

  curl -L https://git.io/JnERa | bash

.. note::

  The installation script will prompt for ``sudo`` authentication.
  This is required to install packages using ``apt-get``.
  If you are concerned, feel free to review the
  `installation script <https://git.io/JnERa>`_.
  The pre-built packages are hosted on
  `GitHub <https://github.com/Blaok/tapa/tree/gh-pages>`_
  and `PyPI <https://pypi.org/project/tapa/>`_.

.. tip::

  Ubuntu 18.04 is highly recommended.
  Ubuntu 20.04 is supported but undertested due to limited vendor-tool support.
  Ubuntu 16.04 can only run software simulation using threads, not coroutines.

.. tip::

  TAPA is in active development.
  New versions are tagged and built every couple of weeks.
  You can run the installation script again to obtain the latest pre-built
  version when a new version is available.

Install TAPA from Source
------------------------

If you would like to contribute code to TAPA,
or insist on using other Linux distributions,
you'll need to build TAPA from source.

Build Prerequisites
+++++++++++++++++++

* Add ``${HOME}/.local/bin`` to your ``PATH``:

  * ``PATH="${HOME}/.local/bin:${PATH}"``

* CMake 3.13+

  * ``python3 -m pip install cmake``

* A C++ 11 compiler

  * ``sudo apt install build-essential``
* Google glog library

  * ``sudo apt install libgoogle-glog-dev``

* Boost coroutine library

  * ``sudo apt install libboost-coroutine-dev``

* `FPGA Runtime <https://github.com/Blaok/fpga-runtime>`_

  * ``curl -L https://git.io/JuAxz | bash``


Additional Build Prerequisites for Testing
++++++++++++++++++++++++++++++++++++++++++

* Google gflags library

  * ``sudo apt install libgflags-dev``

Runtime Dependency
++++++++++++++++++

* Python 3.6+
* Icarus Verilog

  * ``sudo apt install iverilog``

* Clang 8 and its headers

  *  ``sudo apt install clang-8 libclang-8-dev``

* Vitis 2020.2
* `Xilinx Runtime <https://github.com/Xilinx/XRT>`_

Build and Installation
++++++++++++++++++++++

1. Download source and install the Python part.

.. code-block:: bash

  git clone https://github.com/UCLA-VAST/tapa
  python3 -m pip install tapa/backend/python

.. tip::

  Since the Python part of TAPA is being actively upgraded, you could install the python package as editable.
  In this case, it is easy to upgrade your local installation: simply pull the latest change from Github.

.. code-block:: bash

  python3 -m pip install --editable tapa/backend/python

2. Create a build directory

.. code-block:: bash

  cd tapa
  mkdir build
  cd build

3. Build TAPA and run tests. Note: the command below will allow up to 8 parallel jobs for ``make``. This number should be adjusted according to your available cores and memory.

.. code-block:: bash

  cmake ..
  make -j8
  make -j8 test

.. tip::

  If you have `Ninja <https://ninja-build.org>`_ installed, you should do the
  following instead:

  .. code-block:: bash

    cmake .. -GNinja
    ninja
    ninja test

4. Install TAPA.

.. code-block:: bash

  sudo ln -sf "${PWD}"/backend/tapacc /usr/local/bin/
  sudo ln -sf "${PWD}"/../src/tapa{,.h} /usr/local/include/
  sudo ln -sf "${PWD}"/libtapa.{a,so} /usr/local/lib/

Additional Build Prerequisites for Documentation
++++++++++++++++++++++++++++++++++++++++++++++++

* Doxygen

  * ``sudo apt install doxygen``

* Sphinx and Breathe

  * ``python3 -m pip install -r docs/requirements.txt``

Install Gurobi (Recommended)
----------------------------

Installing Gurobi is optional but highly recommended.
In the floorplanning step,
TAPA/AutoBridge relies on `Python MIP <https://www.python-mip.com/>`_ to solve
Integer Linear Programming (ILP) problems.
By default, Python MIP uses an open-source solver.
The commercial Gurobi solver is much faster than the open-source solver,
and it is free for academia.

* Register and download the Gurobi Optimizer at
  https://www.gurobi.com/downloads/gurobi-optimizer-eula/

* Unzip the package to your desired directory

* Obtain an academic license at
  https://www.gurobi.com/downloads/end-user-license-agreement-academic/

* Set environment variables ``GUROBI_HOME`` and ``GRB_LICENSE_FILE``

  .. code-block:: bash

    export GUROBI_HOME=[WHERE-YOU-INSTALL]
    export GRB_LICENSE_FILE=[ADDRESS-OF-YOUR-LICENSE-FILE]
    export PATH="${PATH}:${GUROBI_HOME}/bin"
    export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${GUROBI_HOME}/lib"

Verify Installation
-------------------

Check if ``tapac`` is available after installation:

.. code-block:: bash

  tapac --help

If ``tapac`` is not found,
you may need to add ``${HOME}/.local/bin`` to your ``PATH``:

.. code-block:: bash

  PATH="${HOME}/.local/bin:${PATH}"

Troubleshooting
---------------

CMake Returns an Error
++++++++++++++++++++++

Please check ``cmake --version``.
CMake 3.13 or higher is required,
which can be easily installed via ``pip install cmake``.
If you have installed an appropriate version of CMake but still encounter
problems, please check ``which cmake`` to see the full path of CMake in use.
If your ``PATH`` is polluted by environmental setup scripts,
please make sure you *prepend* the path containing ``cmake``
(e.g., ``${HOME}/.local/bin``) to ``PATH``
*after* all such scripts are sourced.
