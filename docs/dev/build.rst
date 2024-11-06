Building from Source
====================

.. note::

   This guide is for **developers** contributing to or extending TAPA,
   or **advanced users** building TAPA from source for custom OS support.
   For FPGA accelerator development with TAPA, refer to the
   :ref:`User Documentation`.

.. tip::

   If your OS isn't officially supported and you're not a developer,
   consider using a virtual machine or file a
   `feature request on GitHub <https://github.com/rapidstream-org/rapidstream-tapa/issues>`_.

System Prerequisites
--------------------

To build TAPA from source, you need:

- `Bazel <https://bazel.build>`_ 7.3.2 or later
- `Binutils <https://www.gnu.org/software/binutils/>`_ 2.30 or later
- `Git <https://git-scm.com>`_
- `Libstdc++ <https://gcc.gnu.org/libstdc++/>`_ matching the most recent GCC
  version installed on your system
- `Python <https://www.python.org>`_ 3.6 or later
- :ref:`Other TAPA dependencies <user/installation:System Prerequisites>`

Install these tools using your OS package manager. For Ubuntu:

.. code-block:: bash

   # Install bazel
   sudo apt-get install apt-transport-https ca-certificates gnupg
   curl -fsSL https://bazel.build/bazel-release.pub.gpg \
     | gpg --dearmor | sudo tee /usr/share/keyrings/bazel-archive-keyring.gpg
   echo "deb [arch=amd64 signed-by=/usr/share/keyrings/bazel-archive-keyring.gpg] \
       https://storage.googleapis.com/bazel-apt stable jdk1.8" \
     | sudo tee /etc/apt/sources.list.d/bazel.list
   sudo apt-get install bazel

   # Install other tools
   sudo apt-get install binutils git python3

.. tip::

   For Bazel installation on other OS, see the
   `Bazel documentation <https://docs.bazel.build/versions/main/install.html>`_.

.. note::

   The `Dockerfile in the TAPA repository <https://github.com/rapidstream-org/rapidstream-tapa/blob/main/.github/docker/build-env/Dockerfile.Dependencies>`_
   provides a complete build environment. Use it for containerized builds or
   run the Ubuntu commands to install required tools.

Clone the Repository
--------------------

To get started with building TAPA from source, you'll need to clone the repository from GitHub:

.. code-block:: bash

   git clone https://github.com/rapidstream-org/rapidstream-tapa.git

If you are contributing to TAPA, fork the repository and clone your fork
instead. When you're ready to contribute, create a new branch for your
changes, commit your work, and open a pull request to contribute your
changes back to the main repository.

Build TAPA from Source
----------------------

To build TAPA, navigate to the root directory of the cloned repository and execute the following command:

.. code-block:: bash

   bazel build //...

This command compiles all TAPA targets, including the compiler, runtime
library, and tests.

For building a specific target, replace ``//...`` with the desired target
name. For instance, to build only the TAPA compiler:

.. code-block:: bash

   bazel build //tapa

.. note::

   To view all available targets, run ``bazel query //...``.

To skip building for the tests, you could use:

.. code-block:: bash

   bazel build //... -- -//tests/...


After the build process completes, you can find the compiled binaries in the
``bazel-bin`` directory. For example, the TAPA compiler binary is located at
``bazel-bin/tapa/tapa``.

.. note::

   The build process duration may vary depending on your system's performance.
   LLVM, a significant dependency used by TAPA for code generation, requires
   considerable time to build. Bazel will cache it after the initial build.

Use the Built TAPA
------------------

.. important::

   Remember to source the Vivado settings script before running the TAPA compiler.

Once TAPA is built, you can use the compiled TAPA compiler to compile your
designs. For example:

.. code-block:: bash

   bazel-bin/tapa/tapa compile \
    -f tests/apps/bandwidth/bandwidth.cpp \
    --cflags -Itests/apps/bandwidth/ \
    -t Bandwidth \
    --clock-period 3 \
    --part-num xcu250-figd2104-2L-e

Remember to rerun the ``bazel build`` command whenever you make changes to the
TAPA compiler or runtime library to ensure you're using the latest version.

Run TAPA Tests
--------------

To run all TAPA tests, including unit tests and integration tests, use the
following command in the repository's root directory:

.. code-block:: bash

   bazel test //...

For running a specific test, replace ``//...`` with the test name. For example,
to test only a specific app:

.. code-block:: bash

   bazel test //tests/apps/vadd:vadd-xosim

Build Binary Distribution
-------------------------

To create a binary distribution of TAPA, navigate to the root directory of the
cloned repository and execute the following command:

.. code-block:: bash

   bazel build --config=release //:tapa-pkg-tar

Find the generated binary distribution in the ``bazel-bin`` directory,
as a tarball named ``tapa-pkg-tar.tar``.

Install the Binary Distribution
-------------------------------

To install the binary distribution, extract the tarball to a directory of your
choice:

.. code-block:: bash

   tar -xvf bazel-bin/tapa-pkg-tar.tar -C /path/to/install

Access the TAPA compiler binary at ``/path/to/install/usr/bin/tapa``.

Containerized Build (Advanced)
------------------------------

For those who prefer a containerized build environment, TAPA offers a GitHub
Actions workflow that can be run locally using ``act``. This approach ensures
a consistent build environment across different systems.

Prerequisites
^^^^^^^^^^^^^

1. Install ``act`` by following the instructions in the
   `act repository <https://nektosact.com>`_.

2. Ensure Docker is installed on your system, as ``act`` requires it to run
   the workflow.

.. note::

   RapidStream organization developers using RapidStream servers can skip
   the configuration steps below, as the necessary setup is already in place.

Configuration
^^^^^^^^^^^^^

Before running ``act``, set up the following configuration files:

1. Create a ``.secrets`` file in the repository root with the following content:

   .. code-block:: text

      UBUNTU_PRO_TOKEN=[YOUR_UBUNTU_PRO_TOKEN]
      MAC_ADDRESS=de:ed:be:ef:ca:fe

   Replace ``[YOUR_UBUNTU_PRO_TOKEN]`` with your Ubuntu Pro token (available
   free for personal use) and ``de:ed:be:ef:ca:fe`` with your Vivado license
   MAC address.

2. Update the ``.actrc`` file in the repository root:

   .. code-block:: text

      --secret-file .secrets

3. If your Vivado license and installation locations differ from the defaults
   (``/share/software/licenses/xilinx-ci.lic`` and
   ``/share/software/tools`` respectively), update
   ``.github/actions/run-docker/action.yml`` accordingly.

.. note::

   Developers from the RapidStream organization can start from here.

Running Containerized Tests
^^^^^^^^^^^^^^^^^^^^^^^^^^^

To test TAPA in the containerized environment:

.. code-block:: bash

   act -j test

This method often provides more consistent results than local testing due to
the isolated environment. It also benefits from a shared Bazel cache between
runs, potentially speeding up the build process.

.. note::

   Build artifacts are not saved to the local ``bazel-bin`` directory in
   containerized builds. For debugging, you may need to build TAPA in your
   local environment. However, you can still add test cases and use ``act``
   for testing your changes.

Creating a Binary Distribution
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To create a binary distribution of TAPA:

.. code-block:: bash

   act -j build

The resulting binary distribution is saved in the ``artifacts.out`` directory
in the repository root (e.g., ``artifacts.out/1/tapa/tapa.tar.gz`` for the
first build).

Installing the Binary Distribution
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To install the binary distribution:

1. Extract the tarball to your preferred directory, or
2. Use the provided ``install.sh`` script to install TAPA to the default
   location:

   .. code-block:: bash

      RAPIDSTREAM_LOCAL_PACKAGE=./artifacts.out/1/tapa/tapa.tar.gz ./install.sh
