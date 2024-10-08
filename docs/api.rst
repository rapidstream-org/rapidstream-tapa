The TAPA Library (libtapa)
--------------------------

The Task Instantiation Library
::::::::::::::::::::::::::::::

.. doxygenstruct:: tapa::task
  :members:

.. doxygenstruct:: tapa::seq
  :members:

The Streaming Library
:::::::::::::::::::::

* A *blocking operation* blocks if the stream is not available (empty or full)
  until the stream becomes available.
* A *non-blocking operation* always returns immediately.

* A *destructive operation* changes the state of the stream.
* A *non-destructive operation* does not change the state of the stream.

.. doxygenclass:: tapa::istream
  :members:

.. doxygenclass:: tapa::istreams
  :members:

.. doxygenclass:: tapa::ostream
  :members:

.. doxygenclass:: tapa::ostreams
  :members:

.. doxygenclass:: tapa::stream
  :members:

.. doxygenclass:: tapa::streams
  :members:

The MMAP Library
::::::::::::::::

.. doxygenclass:: tapa::async_mmap
  :members:

.. doxygenclass:: tapa::mmap
  :members:

.. doxygenclass:: tapa::mmaps
  :members:

The Utility Library
:::::::::::::::::::

.. doxygenfunction:: tapa::widthof()
.. doxygenfunction:: tapa::widthof(T)

The HLS-Compat Library
::::::::::::::::::::::

The HLS-compat library provides a set of APIs compatible with Vitis HLS stream
behavior to ease migration from Vitis HLS.

.. warning::

  ``tapa::hls_compat`` APIs are software simulation only and are NOT
  synthesizable.
  Before synthesis, remove ``#include <tapa/host/compat.h>`` and replace any
  ``tapa::hls_compat`` API with their synthesizable equivalent.

.. doxygentypedef:: tapa::hls_compat::stream

.. doxygentypedef:: tapa::hls_compat::stream_interface

.. doxygenstruct:: tapa::hls_compat::task

The TAPA Compiler (tapa)
------------------------

.. click:: tapa.__main__:entry_point
  :prog: tapa
  :nested: full
