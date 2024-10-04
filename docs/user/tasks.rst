Tasks and Hierarchical Design
=============================

.. note::

   TAPA allows flexible task invocation with features such as detached
   tasks and hierarchical design. This section provides an overview of
   TAPA tasks and their advanced features.

Detached Tasks
--------------

TAPA allows you to *detach* a task on invocation instead of joining it to
the parent. This feature is useful when terminating each kernel function
is unnecessary, such as for purely data-driven tasks that don't need to be
terminated on program completion. Detached tasks in TAPA are similar to
``std::thread::detach`` in the
`C++ STL <https://en.cppreference.com/w/cpp/thread/thread/detach>`_.

To detach a task, use the ``tapa::detach`` keyword in the task invocation:

.. code-block:: cpp

  void InnerStage(int b, istreams<pkt_t, kN / 2>& in_q0,
                  istreams<pkt_t, kN / 2>& in_q1, ostreams<pkt_t, kN> out_q) {
    task().invoke<tapa::detach, kN / 2>(Switch2x2, b, in_q0, in_q1, out_q);
  }

Detached tasks offer several advantages:

1. **Design efficiency**: The state of detached tasks doesn't need to be
   maintained at runtime, saving resources. Fan-out signals are also
   avoided, reducing the complexity of the design.
2. **Programming flexibility**: Unlike Intel FPGA SDK's
   `"autorun" kernels <https://www.intel.com/content/www/us/en/programmable/documentation/mwh1391807965224.html#ewa1456413600674>`_
   or Xilinx Vitis'
   `"free-running" kernels <https://www.xilinx.com/html_docs/xilinx2020_2/vitis_doc/streamingconnections.html#ariaid-title5>`_,
   detached tasks in TAPA can have arguments other than global communication
   channels.
3. **Simplified design**: Detached tasks eliminate the need to propagate
   termination signals, reducing unnecessary complexity.

.. note::

   By default, TAPA tasks are joined on invocation. Therefore, the parent
   task waits for the child task to complete before terminating. To detach
   the task, use the ``tapa::detach`` keyword in the task invocation.

Hierarchical Design
-------------------

TAPA supports hierarchical task design, allowing tasks to be recursively
defined. A task can be a parent to its children tasks, which can themselves
be parents to their own children. This feature enables the creation of
complex, modular designs.

Here's an example of a hierarchical design using TAPA:

.. code-block:: cpp

  void Stage(int b, istreams<pkt_t, kN>& in_q, ostreams<pkt_t, kN> out_q) {
    task().invoke<detach>(InnerStage, b, in_q, in_q, out_q);
  }

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

In this example, the top-level task instantiates a data producer, a data
consumer, and three wrapper stages. Each stage can further instantiate its
own child tasks ``InnerStage``, creating a hierarchical structure.

The network example shipped with TAPA demonstrates both of these features:

1. It uses detached tasks for 2×2 switch boxes, which are instantiated and
   detached in the inner wrapper stage.
2. It employs a hierarchical design where 2×2 switch boxes are instantiated
   in an inner wrapper stage, and each inner stage is then wrapped in a stage.

.. note::

   TAPA allows tasks to be detached on invocation and supports hierarchical
   design, enabling the creation of complex, modular designs.
