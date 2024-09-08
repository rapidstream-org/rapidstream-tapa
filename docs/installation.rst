Quick Installation
===================

The RapidStream-TAPA toolchain can be installed with a single command. After the installation completes successfully, restart your terminal session, or run ``source ~/.profile``.
You can upgrade your installation at any time by repeating the installation command.


.. code-block:: bash

  # for Ubuntu and Debian
  sudo apt-get install g++ iverilog ocl-icd-libopencl1

  # or for Fedora, RHEL, and Amazon Linux
  # sudo yum install gcc-c++ iverilog libxcrypt-compat ocl-icd

  # Install TAPA
  sh -c "$(curl -fsSL tapa.rapidstream.sh)"

  # Install RapidStream
  sh -c "$(curl -fsSL rapidstream.sh)"

Check if ``tapa`` and ``rapidstream`` is available after installation:

.. code-block:: bash

  tapa --help
  rapidstream --help

Please restart your shell to finish the installation. Alternatively, you can run the following command to apply the changes:

.. code-block:: bash

  export PATH="$PATH:${HOME}/.rapidstream-tapa/usr/bin"



Other Installation Options
============================

We provide Docker Image for easy evaluation.

.. code-block:: bash

  docker pull rapidstream/rapidstream:stable-latest

Installing Gurobi is optional but could potentially make RapidStream runs faster.
Gurobi is free for academia.

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


Several installation options are available for different Linux distributions. You can choose the one that fits your Linux distribution as follows:

- Debian Package (.deb) for Ubuntu, Debian, and other Debian-based operating systems.

.. code-block:: bash

  wget rapidstream.sh/deb -O /tmp/rapidstream.deb
  dpkg -i /tmp/rapidstream.deb

- RPM Package (.rpm) for Red Hat, CentOS, SUSE, Amazon Linux, and other RPM-based operating systems.

.. code-block:: bash

  rpm -i https://rapidstream.sh/rpm


- AppImage for a portable executable file.

.. code-block:: bash

  wget rapidstream.sh/app -O rapidstream
  chmod +x ./rapidstream
  ./rapidstream




Get A Free License for RapidStream
===================================

There are two main components in the toolchain:

- The TAPA frontend is fully open-sourced.
- The RapidStream backend is released as binary with free licenses. Simply submit a request
  at https://rapidstream-da.com/contact-us to get a free license for the rapidstream backend.

To configure the license for RapidStream tools, the license file can be placed in any of
the following predetermined locations or other locations specified by the `RAPIDSTREAM_LICENSE_FILE` environment variable.

- ~/.rapidstream.lic

- ~/.rapidstream/rapidstream.lic

- /opt/licenses/rapidstream.lic
