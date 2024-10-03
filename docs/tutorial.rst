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
