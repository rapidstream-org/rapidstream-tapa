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

TAPA Compiler (tapa)
--------------------

.. _api tapa:

.. click:: tapa.__main__:entry_point
  :prog: tapa
  :nested: full
