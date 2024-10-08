Releasing TAPA Builds
=====================

.. note::

   This section explains how to release TAPA builds. It is intended for
   **maintainers** with write access to the TAPA repository.

Start a new GitHub action of
`"Build and Publish Stable Version <https://github.com/rapidstream-org/rapidstream-tapa/actions/workflows/publish-stable.yml>`_
from the GitHub Actions tab. This action will build and publish the stable
version of TAPA to RapidStream Release Registry and automatically mark the
release as the latest stable version.

Only the administrators of the RapidStream organization can approve the
release. Once the action is triggered, the administrators will be notified
and asked to approve the release.
