
We will cover the basic usages of TAPA in this section.
If you are migrating from Vitis HLS,
:ref:`another tutorial <tutorial/migrate_from_vitis_hls:migrate from vitis hls>`
presents two nice examples.

Before you start, make sure you have
:ref:`installed TAPA <installation:Quick Installation>` properly.

Hello World: Vector Add
:::::::::::::::::::::::

Let's start with the following TAPA C++ source code:

.. literalinclude:: ../tests/apps/vadd/vadd.cpp
   :language: cpp


The above code adds two variable-length ``float`` vectors, ``a`` and ``b``,
to produce a new vector ``c`` with the same length.
This is done with four C++ functions:
``Add``, ``Mmap2Stream``, ``Stream2Mmap``, and ``VecAdd``.
The last function *invokes* the first three.
Let's discuss them one-by-one.

The ``Add`` function performs the element-wide addition operations.
It takes 4 arguments:
``tapa::istream<float>& a`` and ``tapa::istream<float>& b`` are
:ref:`input streams <api:istream>`.
``tapa::ostream<float>& c`` is an :ref:`output stream <api:ostream>`.
``uint64_t n`` is a scalar input for the length of the vectors.
This function reads each of ``a`` and ``b`` for ``n`` times, adds them together,
and writes the sum to ``c``.

The ``Mmap2Stream`` function reads an input vector from DRAM and writes it to
a stream.
It takes 3 arguments:
``tapa::mmap<const float> mmap`` is a :ref:`memory-mapped <api:mmap>` interface.
On FPGA, this is usually the on-board DRAM.
``uint64_t n`` is a scalar input for the length of the vectors.
``tapa::ostream<stream>& c`` is an :ref:`output stream <api:ostream>`.

The ``Stream2Mmap`` function is the reverse of ``Mmap2Stream``.

The above three functions each defines a task, which can run *in parallel*.
Tasks that do not instantiation other tasks are called *lower-level* tasks.

The ``VecAdd`` function instantiates the three lower-level tasks and defines
the communication channels between them.
It takes 3 arguments:
3 mmap interfaces for the 3 vectors and one scalar for the vector length.
3 :ref:`communication channels <api:stream>` are defined in ``VecAdd``.
The 3 lower-level tasks are instantiated 4 times in total because there are
2 input vectors, each of which needs its own ``Mmap2Stream``.
The ``VecAdd`` function is an *upper-level* task.
It is also the *top-level* task that defines the interface between the kernel
and the host.
Once the 4 children task instances are instantiated,
they will run in parallel and their parent will wait until all children finish.



Run Software Simulation
:::::::::::::::::::::::

Next, let's actually run the program.
We will need a host program to driver the kernel.

.. code-block:: cpp

  void VecAdd(tapa::mmap<const float> a_array, tapa::mmap<const float> b_array,
              tapa::mmap<float> c_array, uint64_t n);

  int main()
  ...
  vector<float> a(n);
  vector<float> b(n);
  vector<float> c(n);
  ...
  tapa::invoke(VecAdd, FLAGS_bitstream, tapa::read_only_mmap<const float>(a),
               tapa::read_only_mmap<const float>(b),
               tapa::write_only_mmap<float>(c), n);
  ...

The top-level kernel function is called using ``tapa::invoke`` so that
software simulation, hardware simulation, and on-board execution can share the
same piece of code.
``tapa::invoke`` takes a variable number of arguments.
The first argument is the top-level kernel function,
which needs to be declared ahead of time.
The second argument is the path to the desired bitstream.
If this argument is an empty string, software simulation will run.
The rest of the arguments are passed to the top-level kernel function.
In our vector add example,
we allocate 3 ``vector<float>`` for ``a``, ``b``, and ``c``, respectively.
``a`` and ``b`` are passed to ``tapa::invoke`` as ``tapa::read_only_mmap``
because the kernel reads but not writes them.
Similarly, ``c`` is passed as ``tapa::write_only_mmap`` because the kernel
writes but not reads them.
Scalar values like the vector length ``n`` are passed to the kernel directly.

.. note::

  ``tapa::read_only_mmap`` and ``tapa::write_only_mmap`` only specify
  the host-kernel communication behavior.
  They do not control nor check whether the kernel really reads or writes
  a memory-mapped argument.
  Under the hood, ``tapa::read_only_mmap`` just tells the host program that
  this memory space should be transferred to the FPGA before launching the
  kernel, and it does not need to be transferred back to the host when the
  kernel finishes.
  ``tapa::write_only_mmap`` just tells the host program that this memory space
  do not need to be transferred to the FPGA before launching, but it should be
  transferred back to the host when the kernel finishes.
  Two other possible alternatives are ``tapa::read_write_mmap`` and
  ``tapa::placeholder_mmap``.

.. note::

  Scalar values are always read-only to the kernel.

The vector add example is one of the application examples shipped with TAPA. Assuming you have
installed tapa in the default location (``${HOME}/.rapidstream-tapa``), you can compile and run the
example as follows:

.. code-block:: bash

  git clone https://github.com/rapidstream-org/rapidstream-tapa
  cd tests/apps/vadd

  g++ \
    -std=c++17 \
    *.cpp \
    -o vadd \
    -I${HOME}/.rapidstream-tapa/usr/include/ \
    -I/opt/tools/xilinx/Vitis_HLS/2024.1/include/ \
    -Wl,-rpath,$(readlink -f ~/.rapidstream-tapa/usr/lib) \
    -L ${HOME}/.rapidstream-tapa/usr/lib/ \
    -ltapa -lcontext -lthread -lfrt -lglog -lgflags -l:libOpenCL.so.1 -ltinyxml2 -lstdc++fs

  ./vadd

You should be able to see something like:

.. code-block:: text

  elapsed time: 0.334634 s
  PASS!

By default, the host program will add two :math:`2^{20}`-element vectors.
This can be changed by supplying an argument to the command line:

.. code-block:: bash

  ./vadd 1000

This time the elapsed time should be much shorter:

.. code-block:: text

  elapsed time: 0.0126179 s
  PASS!

The above runs software simulation of the program,
which helps you quickly verify the correctness.



Synthesize into RTL
:::::::::::::::::::::::


The next step is to synthesize it.
The first step would be to run high-level synthesize (HLS):

.. code-block:: bash

  tapa \
    --work-dir build \
  compile \
    --top VecAdd \
    --part-num xcu250-figd2104-2L-e \
    --clock-period 3.33 \
    -o build/VecAdd.xo \
    -f vadd.cpp

This will take a couple of minutes.
HLS reports will be available in the working directory
``build/report``.



Run Hardware Simulation with TAPA Simulator
:::::::::::::::::::::::::::::::::::::::::::::

Simply replace the ``vadd.$platform.hw_emu.xclbin`` by the previously generated
``xo`` object. See this :ref:`tutorial <tutorial/fast_cosim:TAPA RTL
Simulation>` for more details.

.. code-block:: bash

  ./vadd --bitstream=vadd.$platform.hw.xo 1000
