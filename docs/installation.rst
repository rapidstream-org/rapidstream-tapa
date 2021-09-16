Installation
============

Install from Binary
-------------------

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

Install from Source
-------------------

If you would like to contribute code to TAPA,
or insist on using other Linux distributions,
you'll need to build TAPA from source.

Build Prerequisites
+++++++++++++++++++

* CMake 3.13+

  * ``pip install cmake``

* A C++ 11 compiler

  * ``sudo apt install build-essential``
* Google glog library

  * ``sudo apt install libgoogle-glog-dev``

* Boost coroutine library

  * ``sudo apt install libboost-coroutine-dev``

* `FPGA Runtime <https://github.com/Blaok/fpga-runtime>`_

  * ``curl -L https://git.io/JuAxz | bash``

Additional Build Prerequisites for Documentation
++++++++++++++++++++++++++++++++++++++++++++++++

* Doxygen

  * ``sudo apt install doxygen``

* Sphinx and Breathe

  * ``pip install -r docs/requirements.txt``

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

1. Download source and create a build directory.

.. code-block:: bash

  git clone https://github.com/UCLA-VAST/tapa
  cd tapa
  mkdir build
  cd build

2. Build TAPA and run tests.

.. code-block:: bash

  cmake ..
  make -j8
  make -j8 test


.. tip::

  The above steps will allow up to 8 parallel jobs for ``make``.
  This number should be adjusted according to your available cores and memory.

.. tip::

  If you have `Ninja <https://ninja-build.org>`_ installed, you should do the
  following instead:

  .. code-block:: bash

    cmake .. -GNinja
    ninja
    ninja test

3. Install TAPA.

.. code-block:: bash

  pip install ../backend/python
  sudo make install
  sudo install backend/tapacc /usr/local/bin/
  sudo cp -r ../src/tapa ../src/tapa.h /usr/local/include/
  sudo cp -r libtapa.a libtapa.so /usr/local/lib/

.. tip::

  If you are contributing code to TAPA,
  you may want the installation to update itself as you develop.
  This can be done as follows:

.. code-block:: bash

  pip install --editable ../backend/python
  sudo ln -sf "${PWD}"/backend/tapacc /usr/local/bin/
  sudo ln -sf "${PWD}"/../src/tapa{,.h} /usr/local/include/
  sudo ln -sf "${PWD}"/libtapa.{a,so} /usr/local/lib/

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

``cmake`` Returns an Error
++++++++++++++++++++++++++

Please check ``cmake --version``.
CMake 3.13 or higher is required,
which can be easily installed via ``pip install cmake``.
If you have installed an appropriate version of CMake but still encounter
problems, please check ``which cmake`` to see the full path of CMake in use.
If your ``PATH`` is polluted by environmental setup scripts,
please make sure you *prepend* the path containing ``cmake``
(e.g., ``${HOME}/.local/bin``) to ``PATH``
*after* all such scripts are sourced.
