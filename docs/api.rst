API Reference
=============

.. toctree::
  :titlesonly:

The TAPA Library (libtapa)
--------------------------

The Task Instantiation Library
::::::::::::::::::::::::::::::

task
^^^^
.. doxygenstruct:: tapa::task
  :members:

The Streaming Library
:::::::::::::::::::::

* A *blocking operation* blocks if the stream is not available (empty or full)
  until the stream becomes available.
* A *non-blocking operation* always returns immediately.

* A *destructive operation* changes the state of the stream.
* A *non-destructive operation* does not change the state of the stream.

channel
^^^^^^^
.. doxygentypedef:: tapa::channel

channels
^^^^^^^^
.. doxygentypedef:: tapa::channels

istream
^^^^^^^
.. doxygenclass:: tapa::istream
  :members:

istreams
^^^^^^^^
.. doxygenclass:: tapa::istreams
  :members:

ostream
^^^^^^^
.. doxygenclass:: tapa::ostream
  :members:

ostreams
^^^^^^^^
.. doxygenclass:: tapa::ostreams
  :members:

stream
^^^^^^
.. doxygenclass:: tapa::stream
  :members:

streams
^^^^^^^
.. doxygenclass:: tapa::streams
  :members:

The MMAP Library
::::::::::::::::

async_mmap
^^^^^^^^^^
.. doxygenclass:: tapa::async_mmap
  :members:

mmap
^^^^
.. doxygenclass:: tapa::mmap
  :members:

mmaps
^^^^^
.. doxygenclass:: tapa::mmaps
  :members:

The Utility Library
:::::::::::::::::::

widthof
^^^^^^^
.. doxygenfunction:: tapa::widthof()
.. doxygenfunction:: tapa::widthof(T)

The TAPA Compiler (tapac)
-------------------------

.. argparse::
  :module: tapa.tapac
  :func: create_parser
  :prog: tapac
