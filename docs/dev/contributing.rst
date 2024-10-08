Contributing to TAPA
====================

Pull Request Process
--------------------

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

Testing
~~~~~~~

- Add appropriate unit tests for new features or bug fixes.
- Ensure all existing tests pass before submitting your changes.
- Run the full test suite using the following command:

  .. code-block:: bash

     bazel test //...

Reporting Issues
----------------

- Use the GitHub issue tracker to report bugs or suggest new features.
- Provide a clear and concise description of the issue or feature request.
- Include steps to reproduce the issue, if applicable.
- Attach relevant log files or screenshots, if available.

Community Guidelines
--------------------

- Be respectful and considerate in all interactions with other contributors.
- Provide constructive feedback on pull requests and issues.
