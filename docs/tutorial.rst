
Flexible Interaction with Streams
--------------------------------------

Peeking a Stream
::::::::::::::::

This section covers the usage of non-destructive read (a.k.a. peek) on a stream.

TAPA provides the functionality to read a token from a stream without actually
removing it from the stream.
This can be helpful when the computation operations depend on the content of the
input token, for example, in a switch network.

The network example shipped with TAPA uses the peeking API.
That example implements 3-stage 8×8
`Omega network <https://www.mathcs.emory.edu/~cheung/Courses/355/Syllabus/90-parallel/Omega.html>`_.
The basic component of such a multi-stage switch network is a 2×2 switch box,
which routes an input packet based on one bit in the destination address
(which is part of the packet).
For example, if a packet from ``pkt_in_q0`` has destination ``2 = 0b010``,
and we are interested in bit 1 (0-based),
this packet should be written to ``pkt_out_q[1]``.
If another packet from ``pkt_in_q1`` has destination ``7 = 0b111``,
we will have to choose only one of them to write to ``pkt_out_q[1]`` because
only one token can be written in each clock cycle.
This means we need to make the decision before we remove the token from the
input channel (stream).
In the following code, we first
:ref:`peek <classtapa_1_1istream_1a6df8ab2e1caaaf2e32844b7cc716cf11>`
the input stream.
Then, we use the peeked destinations to determine which input(s) can actually be
consumed.
Finally, we schedule the read operations based on these decisions.

.. code-block:: cpp

  void Switch2x2(int b, istream<pkt_t>& pkt_in_q0, istream<pkt_t>& pkt_in_q1,
                 ostreams<pkt_t, 2>& pkt_out_q) {
  ...
  for (bool is_valid_0, is_valid_1;;) {
    const auto pkt_0 = pkt_in_q0.peek(valid_0);
    const auto pkt_1 = pkt_in_q1.peek(valid_1);
    ... // decide which input(s) can be consumed
    if (...) pkt_in_q0.read(nullptr);
    if (...) pkt_in_q1.read(nullptr);
  }

End-of-Transaction
::::::::::::::::::

This section covers the usage of end-of-transaction tokens.

`SODA <https://github.com/UCLA-VAST/soda>`_ is a highly parallel
microarchitecture for stencil applications.
It is implemented using the
`dataflow optimization <https://www.xilinx.com/html_docs/xilinx2021_1/vitis_doc/vitis_hls_optimization_techniques.html#bmx1539734225930>`_.
However, this requires that each kernel is terminated properly.
This can be done by broadcasting the loop trip-count to each kernel function,
but doing so requires an adder in each function.
Since each kernel module can be very small,
the adder can consume a lot of resources.

TAPA provides a less expensive solution is to this problem.
With TAPA, a kernel can send a special token to denote the end of transaction
(``EoT``).
For example, in the jacobi stencil example shipped with TAPA,
the producer, ``Mmap2Stream``, sends an ``EoT`` token by
:ref:`closing <classtapa_1_1ostream_1a10405849fa9a12a02e2fc0d33b305d22>` the
stream.

.. code-block:: cpp

  void Mmap2Stream(tapa::mmap<const float> mmap, uint64_t n,
                   tapa::ostream<tapa::vec_t<float, 2>>& stream) {
    [[tapa::pipeline(2)]] for (uint64_t i = 0; i < n; ++i) {
      tapa::vec_t<float, 2> tmp;
      tmp.set(0, mmap[i * 2]);
      tmp.set(1, mmap[i * 2 + 1]);
      stream.write(tmp);
    }
    stream.close();
  }

A downstream module,
``Stream2Mmap`` in the example, can then detect the program termination by
:ref:`testing for the EoT token <classtapa_1_1istream_1aa776b6b4c8cfd5a287c6699077152dcd>`.

.. code-block:: cpp

  void Stream2Mmap(tapa::istream<tapa::vec_t<float, 2>>& stream,
                  tapa::mmap<float> mmap) {
    [[tapa::pipeline(2)]] for (uint64_t i = 0;;) {
      bool eot;
      if (stream.try_eot(eot)) {
        if (eot) break;
        auto packed = stream.read(nullptr);
        mmap[i * 2] = packed[0];
        mmap[i * 2 + 1] = packed[1];
        ++i;
      }
    }
  }


More on Defining Tasks
--------------------------

Detached Task
:::::::::::::

This section covers the usage of detached tasks.

Sometimes terminating each kernel function is an overkill.
For example, a task function may be purely data-driven and we don't have to
terminate it on program termination.
In that case,
TAPA allows you to *detach* a task on invocation instead of joining it to
the parent.
This resembles ``std::thread::detach``
in the `C++ STL <https://en.cppreference.com/w/cpp/thread/thread/detach>`_.
The network example shipped with TAPA leverages this feature to simplify design;
the 2×2 switch boxes are instantiated and detached.

.. code-block:: cpp

  void InnerStage(int b, istreams<pkt_t, kN / 2>& in_q0,
                  istreams<pkt_t, kN / 2>& in_q1, ostreams<pkt_t, kN> out_q) {
    task().invoke<detach, kN / 2>(Switch2x2, b, in_q0, in_q1, out_q);
  }

Hierarchical Design
:::::::::::::::::::

This section covers the usage of hierarchical design.

TAPA tasks are recursively defined.
The children tasks instantiated by an upper-level task can itself be a parent of
its children.
The network example shipped with TAPA leverages this feature to simplify design;
the 2×2 switch boxes are instantiated in an inner wrapper stage;
each inner stage is then wrapped in a stage.
The top-level task only instantiates a data producer, a data consumer,
and 3 wrapper stages.

.. code-block:: cpp

  void Network(mmap<vec_t<pkt_t, kN>> mmap_in, mmap<vec_t<pkt_t, kN>> mmap_out,
               uint64_t n) {
    streams<pkt_t, kN, 4096> q0("q0");
    streams<pkt_t, kN, 4096> q1("q1");
    streams<pkt_t, kN, 4096> q2("q2");
    streams<pkt_t, kN, 4096> q3("q3");

    task()
        .invoke(Produce, mmap_in, n, q0)
        .invoke(Stage, 2, q0, q1)
        .invoke(Stage, 1, q1, q2)
        .invoke(Stage, 0, q2, q3)
        .invoke(Consume, mmap_out, n, q3);
  }

Less Repetitive Code with Stream/MMAP Array
--------------------------------------------


This section covers the usage of stream/mmap arrays
(:ref:`api:streams`/:ref:`api:mmaps`).

Often times a singleton ``stream`` or ``mmap`` is insufficient for
parameterized designs.
For example, the network example shipped with TAPA defines a 8×8 switch network.
What if we want to use a 16×16 network? Or 4×4?
Apparently we would like the network size parameterized so that such a
design-space exploration does not take too much manual effort.
This is possible in TAPA through arrays of stream/mmap and batch invocation.

The concepts of arrays of ``tapa::(i/o)stream`` and ``tapa::mmap`` and
batch invocation explain themselves.
The relevant APIs are well documented in the
:ref:`API references <api:the tapa library (libtapa)>`.
How task invocation interacts with the arrays is worth explaining.
Using the network example,
the ``InnerStage`` function instantiates the 2×2 switch boxes ``kN / 2`` times.
Instead of writing ``kN / 2`` ``invoke`` functions,
this can be done using a single ``invoke``.
It will effectively instantiate the same ``Switch2x2`` task ``kN / 2`` times.
Every time a task is instantiated,
an (actual) argument ``streams`` or ``mmaps`` array will be accessed sequentially.
If the (formal) parameter is a singleton ``istream``, ``ostream``, ``mmap``,
``async_mmap``,
only one element in the array will be accessed.
If the (formal) parameter is an array of ``istreams``, ``ostreams``,
or ``mmaps``,
the number of element accessed is determined by
the array length of the (formal) parameter.
Once accessed,
the accessed element will be marked "accessed" and the next time the following
element will be accessed.

Let's go back to the example.
``InnerStage`` instantiates ``Switch2x2`` ``kN / 2`` times.
The first argument is the scalar input ``b``,
which is broadcast to each instance of ``Switch2x2``.
The second argument is an ``istreams<pkt_t, kN / 2>`` array.
The ``kN / 2`` ``Switch2x2`` instances each takes one ``istream<pkt_t>``.
The third argument is accessed similarly.
The fourth argument is an ``ostreams<pkt_t, kN>`` array.
The ``kN / 2`` ``Switch2x2`` instances each takes one ``ostreams<pkt_t, 2>``,
which is effectively two ``ostream<pkt_t>``.

The same argument can be passed to different parameters in the same invocation.
In such a case, the array elements are accessed sequentially
first from left to right in the argument list then repeated many times
if it is a batch invocation.
``Stage`` leverages this feature in the network example.
The first appearance of ``in_q`` accesses the first ``kN / 2`` elements,
and the second accesses the second half.

.. code-block:: cpp

  void Switch2x2(int b, istream<pkt_t>& pkt_in_q0, istream<pkt_t>& pkt_in_q1,
                 ostreams<pkt_t, 2>& pkt_out_q) {
  }

  void InnerStage(int b, istreams<pkt_t, kN / 2>& in_q0,
                  istreams<pkt_t, kN / 2>& in_q1, ostreams<pkt_t, kN> out_q) {
    task().invoke<detach, kN / 2>(Switch2x2, b, in_q0, in_q1, out_q);
  }

  void Stage(int b, istreams<pkt_t, kN>& in_q, ostreams<pkt_t, kN> out_q) {
    task().invoke<detach>(InnerStage, b, in_q, in_q, out_q);
  }
