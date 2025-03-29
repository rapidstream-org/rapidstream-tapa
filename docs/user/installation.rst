Installation
============

.. note::

   This guide walks you through installing RapidStream TAPA, and
   optionally, the RapidStream toolchain.

One-Step Installation
~~~~~~~~~~~~~~~~~~~~~

Install the RapidStream TAPA toolchain with a single command. After
installation, restart your terminal or follow the instructions to apply the
changes.

.. code-block:: bash

  # Install TAPA
  sh -c "$(curl -fsSL tapa.rapidstream.sh)"

  # Optional: Install RapidStream
  sh -c "$(curl -fsSL rapidstream.sh)"

.. note::

   You may rerun the installation script to update TAPA and RapidStream.

Verify the installation by running:

.. code-block:: bash

  tapa --version
  rapidstream-tapaopt --version

System Prerequisites
~~~~~~~~~~~~~~~~~~~~

RapidStream TAPA requires the following dependencies:

+-------------------+-----------------+----------------------------------------------+
| Dependency        | Version         | Notes                                        |
+===================+=================+==============================================+
| GNU C++ Compiler  | 7.5.0 or newer  | For simulation and deployment only           |
+-------------------+-----------------+----------------------------------------------+
| OpenCL            | 1.2             | For simulation and deployment only           |
+-------------------+-----------------+----------------------------------------------+
| Xilinx Vitis      | 2022.1 or newer |                                              |
+-------------------+-----------------+----------------------------------------------+

RapidStream TAPA has been tested on the following operating systems. Use the
appropriate package manager to install the required dependencies if using a
different OS.

Ubuntu / Debian
^^^^^^^^^^^^^^^

.. note::

   For **Ubuntu 18.04 and newer**, or **Debian 10 and newer**. Older versions
   require building TAPA from source.

.. code-block:: bash

  sudo apt-get install g++ ocl-icd-libopencl1

RHEL / Amazon Linux
^^^^^^^^^^^^^^^^^^^

.. note::

   For **Red Hat Enterprise Linux 9 and newer**, derivatives like **AlmaLinux
   9 and newer** and **Rocky Linux 9 and newer**, or **Amazon Linux 2023**.
   Older versions require building TAPA from source.

.. code-block:: bash

  sudo yum install gcc-c++ libxcrypt-compat ocl-icd

Fedora
^^^^^^

.. note::

   For **Fedora 34 and newer**. Fedora 39 and newer may have minor issues due
   to system C library changes and Vitis HLS tool incompatibility.

.. code-block:: bash

  sudo yum install gcc-c++ libxcrypt-compat ocl-icd

RapidStream License
~~~~~~~~~~~~~~~~~~~

While TAPA compiler is open-source, RapidStream requires a free license.
Request one at https://rapidstream-da.com/contact-us to access the full
RapidStream TAPA flow.

.. note::

   Without a license, you can still use the TAPA compiler without physical
   optimizations. The operating frequency will not be as high as with a
   license.

Place the license file in one of these locations or set the
``RAPIDSTREAM_LICENSE_FILE`` environment variable:

- ``~/.rapidstream.lic``
- ``/opt/licenses/rapidstream.lic``
