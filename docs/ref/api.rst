TAPA Library (libtapa)
----------------------

Task Library
::::::::::::

.. doxygenstruct:: tapa::task
  :members:

.. doxygenstruct:: tapa::seq
  :members:

Stream Library
::::::::::::::

* A *blocking operation* blocks if the stream is not available (empty or full)
  until the stream becomes available.
* A *non-blocking operation* always returns immediately.

* A *destructive operation* changes the state of the stream.
* A *non-destructive operation* does not change the state of the stream.

.. _api istream:

.. doxygenclass:: tapa::istream
  :members:

.. _api istreams:

.. doxygenclass:: tapa::istreams
  :members:

.. _api ostream:

.. doxygenclass:: tapa::ostream
  :members:

.. _api ostreams:

.. doxygenclass:: tapa::ostreams
  :members:

.. _api stream:

.. doxygenclass:: tapa::stream
  :members:

.. _api streams:

.. doxygenclass:: tapa::streams
  :members:

MMAP Library
::::::::::::

.. _api async_mmap:

.. doxygenclass:: tapa::async_mmap
  :members:

.. _api mmap:

.. doxygenclass:: tapa::mmap
  :members:

.. _api mmaps:

.. doxygenclass:: tapa::mmaps
  :members:

Utility Library
:::::::::::::::

.. _api widthof:

.. doxygenfunction:: tapa::widthof()
.. doxygenfunction:: tapa::widthof(T)

HLS-Compat Library
::::::::::::::::::

The HLS-compat library provides a set of APIs compatible with Vitis HLS stream
behavior to ease migration from Vitis HLS.

.. warning::

  ``tapa::hls_compat`` APIs are software simulation only and are NOT
  synthesizable.
  Before synthesis, remove ``#include <tapa/host/compat.h>`` and replace any
  ``tapa::hls_compat`` API with their synthesizable equivalent.

.. _api hls_compat_stream:

.. doxygentypedef:: tapa::hls_compat::stream

.. _api hls_compat_stream_interface:

.. doxygentypedef:: tapa::hls_compat::stream_interface

.. _api hls_compat_task:

.. doxygenstruct:: tapa::hls_compat::task

TAPA Compiler (tapa)
--------------------

.. _api tapa:

.. click:: tapa.__main__:entry_point
  :prog: tapa
  :nested: full
