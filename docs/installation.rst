Install TAPA
============

The TAPA DSL and compilation tools can be installed with a single command. After the installation completes successfully, restart your terminal session, or run ``source ~/.profile``.
You can upgrade your installation at any time by repeating the installation command.


.. code-block:: bash

  sh -c "$(curl -fsSL tapa.rapidstream.sh)"


Check if ``tapa`` is available after installation:

.. code-block:: bash

  tapa --help

Please restart your shell to finish the installation. Alternatively, you can run the following command to apply the changes:

.. code-block:: bash

  export PATH="$PATH:${HOME}/.rapidstream-tapa/usr/bin"


Install RapidStream
===================


RapidStream can be installed with a single command. After the installation completes successfully, restart your terminal session, or run ``source ~/.profile``.
You can upgrade your installation at any time by repeating the installation command.


.. code-block:: bash

  sh -c "$(curl -fsSL rapidstream.sh)"


Alternatively, we provide Docker Image for easy evaluation.

.. code-block:: bash

  docker pull rapidstream/rapidstream:stable-latest



Install Through Package Managers
--------------------------------

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


Install Gurobi (Optional)
-------------------------

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



Get A Free License
===================

The TAPA frontend is fully open-sourced. For the RapidStream backend, we provide binary and free licenses. 

Simply submit a request at https://rapidstream-da.com/contact-us to get a free license. 

To configure the license for RapidStream tools, the license file can be placed in any of 
the following predetermined locations or other locations specified by the `RAPIDSTREAM_LICENSE_FILE` environment variable.

- ~/.rapidstream.lic

- ~/.rapidstream/rapidstream.lic

- /opt/licenses/rapidstream.lic

