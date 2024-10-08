Developing TAPA
===============

.. note::

   This section is intended for **developers** who want to contribute to TAPA.
   It explains the development process, the code structure, and the guidelines
   for contributing to the TAPA framework.

Development Environment
-----------------------

TAPA enforces a consistent coding style and provides tools to ensure code
quality. Follow these steps to set up your development environment.

Install Pre-Commit Hooks
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   pip install pre-commit
   pre-commit install

.. note::

   The latest version of pre-commit is required, which depends on a newer
   Python version. Some hooks may fail if your Python version is outdated.

Pre-commit hooks run automatically before each commit to ensure code compliance
with style guidelines. To manually run the checks:

.. code-block:: bash

   pre-commit run --all-files

Install Python Dependencies for IDEs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

While Bazel automatically installs required Python dependencies during build and
test, you can manually install them for IDE access:

.. code-block:: bash

   pip install -r tapa/requirements_lock.txt

Setting C++ Compiler Options for IDEs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Generate a ``compile_commands.json`` file to configure your IDE with Bazel's
compiler options:

.. code-block:: bash

   bazel run //:refresh_compile_commands

Code Structure
--------------

The TAPA codebase is organized into several key directories:

- ``bazel/``: Contains Bazel build configurations.

  It defines how the TAPA compiler is used in the Bazel build system, and
  provides additional utilities for building and testing TAPA.

- ``docs/``: Includes documentation files.

  The documentation is written in reStructuredText and built using Sphinx.

- ``fpga-runtime/``: Provides the FPGA runtime library.

  The FPGA runtime library is used to interact with simulator or FPGA based
  on provided bitstream. It uses fast lightweight simulator for cosimulation
  with XO object file, and interacts with XRT library for Vitis simulation or
  on-board testing with XCLBIN file.

- ``tapa-cpp/``: Customizes the Clang C++ preprocessor for TAPA.

  The TAPA C++ preprocessor reprocesses TAPA C++ code before passing to
  ``tapacc`` compiler. It supports TAPA-specific features, such as
  ``[[tapa::pipeline]]`` annotations.

- ``tapa-lib/``: Houses the TAPA runtime library.

  The TAPA runtime library provides core functionality for TAPA tasks,
  streams, and memory maps. It implements platform-specific features
  (e.g., software simulation queues, hardware FIFOs).

- ``tapa-llvm-project/``: Contains the LLVM project with TAPA-specific patches.

  TAPA uses LLVM Clang to generate system interconnect and transformed C++
  code for each task. The LLVM project is customized with TAPA-specific
  features, such as C++ annotations.

- ``tapa-system-includes/``: Creates a custom system include directory for TAPA.

  This Bazel build target collects system include files for ``tapa-cpp``
  and ``tapacc`` compilers. It includes standard C++ headers, TAPA
  dependencies, and TAPA-specific headers for the compilers to run on every OS.

- ``tapa/``: Contains the core TAPA compiler and runtime library.

  The TAPA compiler serves as the entry point for the TAPA framework. It
  invokes ``tapa-cpp`` and ``tapacc`` compilers, synthesizes tasks into RTL
  using HLS tools, and generates system interconnect and XO object file for
  FPGA.

- ``tapacc/``: Implements the TAPA C++ compiler to translate TAPA tasks to JSON.

  The TAPA C++ compiler is a Clang-based compiler for TAPA tasks. It analyzes
  tasks and streams, generating JSON representation of tasks and dataflow.

- ``tests/``: Includes test cases for the TAPA compiler and runtime library.

  The folder includes various TAPA applications. It includes microbenchmarks
  under ``apps/`` for basic functionality testing, and ``regression/`` for
  performance evaluation of TAPA compiled designs

Guides for Contributors
-----------------------

Pull Request Process
~~~~~~~~~~~~~~~~~~~~

1. Fork the TAPA repository and create a new branch for your feature or bug fix.
2. Ensure all tests pass and pre-commit hooks run successfully.
3. Write a clear and concise description of your changes in the pull request.
4. Request a review from the TAPA maintainers.

Continuous Integration
~~~~~~~~~~~~~~~~~~~~~~

TAPA uses GitHub Actions for continuous integration. The CI pipeline:

1. Builds and tests TAPA using Ubuntu 18.04 for every commit.
2. Performs code quality checks using pre-commit hooks on every commit.
3. Runs the full test suite on different platforms for every main branch push.

Documentation
~~~~~~~~~~~~~

- Update the documentation in the ``docs/`` directory for any new features
  or changes.
- Use RST format for documentation files.
- Run the following command in the ``docs/`` directory to build and preview
  documentation changes locally:

  .. code-block:: bash

     bash build.sh

Update Python Dependencies
~~~~~~~~~~~~~~~~~~~~~~~~~~

To update the Python dependencies, use the following command:

.. code-block:: bash

   bazel run //tapa:requirements.update

This command will update the ``requirements_lock.txt`` file in the
``tapa/`` directory.

Testing
~~~~~~~

- Add appropriate unit tests for new features or bug fixes.
- Ensure all existing tests pass before submitting your changes.
- Run the full test suite using the following command:

  .. code-block:: bash

     bazel test //...

Reporting Issues
~~~~~~~~~~~~~~~~

- Use the GitHub issue tracker to report bugs or suggest new features.
- Provide a clear and concise description of the issue or feature request.
- Include steps to reproduce the issue, if applicable.
- Attach relevant log files or screenshots, if available.

Community Guidelines
~~~~~~~~~~~~~~~~~~~~

- Be respectful and considerate in all interactions with other contributors.
- Provide constructive feedback on pull requests and issues.
