Common Mistakes and Tips
------------------------

.. note::

   When encountering issues in your TAPA project, the first step is to
   carefully analyze the log printed by TAPA. The log usually contains
   detailed information about the error.

Passing Streams
^^^^^^^^^^^^^^^

TAPA requires streams to be passed by reference to ensure proper communication
between tasks, as the stream object itself is used to communicate between
tasks, instead of the data it contains:

.. code-block:: cpp

   // incorrect
   void Task(tapa::stream<int> in, tapa::stream<int> out) { /* ... */ }

   // correct
   void Task(tapa::istream<int>& in, tapa::ostream<int>& out) { /* ... */ }

Passing Memory Maps
^^^^^^^^^^^^^^^^^^^

TAPA has different passing conventions for ``mmap`` and ``async_mmap`` types,
as ``mmap`` is essentially a pointer to a memory region, while ``async_mmap``
is a set of streams that controls the memory access:

.. code-block:: cpp

   // incorrect
   void Task(tapa::mmap<int> &mem, tapa::async_mmap<int> amem) { /* ... */ }

   // correct
   void Task(tapa::mmap<int> mem, tapa::async_mmap<int> &amem) { /* ... */ }

Using Streams/MMAPs Arrays
^^^^^^^^^^^^^^^^^^^^^^^^^^

When defining arrays of streams or memory maps, use the ``tapa::streams<>`` and
``tapa::mmaps<>`` classes instead of arrays of streams or memory maps:

.. code-block:: cpp

   // incorrect
   tapa::stream<int> data_q[4];

   // correct
   tapa::streams<int, 4> data_q;

   // incorrect
   tapa::mmap<int> mem[4], // ...

   // correct
   tapa::mmaps<int, 4> mem, // ...

To access individual streams or memory maps in the array, use the ``.invoke``
method to distribute the array elements to tasks, instead subscripts:

.. code-block:: cpp

   // incorrect
   tapa::task().invoke(Task, data_q[0], mem[0])
               .invoke(Task, data_q[1], mem[1]);

   // correct
   tapa::task().invoke<tapa::join, 2>(Task, data_q, mem);

Ensuring Correct Task Invocations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When invoking tasks, make sure to pass the correct parameters in the right
order. The output of the C++ compiler can be confusing if the parameters are
not passed correctly, due to modern C++ template usages. When the compiler
complains about the usage of ``tapa::invoke`` or ``tapa::task().invoke``,
it is good to first:

- Match the number of parameters in the invocation with the task definition.
- Ensure the types of passed parameters match the task's parameter types.
- Double-check the order of parameters.

Organizing Task Definitions
^^^^^^^^^^^^^^^^^^^^^^^^^^^

TAPA requires all tasks to be defined in the same file as the top-level
function. Otherwise, the compiler will not be able to find the task. For
example, the following code will not compile:

.. code-block:: cpp

   // task1.cpp
   void Task1(/* ... */) { /* ... */ }

   // task2.cpp
   void Task2(/* ... */) { /* ... */ }

   // top_level.cpp
   void TopLevel(/* ... */) {
     tapa::task().invoke(Task1, /* ... */).invoke(Task2, /* ... */);
   }

Instead, you should define all tasks in the same file as the top-level task.
For better organization, you can define tasks in header files and include them
in the main file:

.. code-block:: cpp

   // task1.hpp
   void Task1(/* ... */) { /* ... */ }

   // task2.hpp
   void Task2(/* ... */) { /* ... */ }

   // top_level.cpp
   #include "task1.hpp"
   #include "task2.hpp"

   void TopLevel(/* ... */) {
     tapa::task().invoke(Task1, /* ... */).invoke(Task2, /* ... */);
   }

Structuring Upper Tasks in TAPA
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Tasks that instantiate other tasks in TAPA should only contain stream
definitions and task invocations. No computation should be done in an
upper task. For example, the following code won't compile, as ``n * 2``
is computed in the ``TopLevel`` task:

.. code-block:: cpp

   void TopLevel(int n, tapa::mmap<int> mem) {
     tapa::stream<int> s("stream_name");
     tapa::task()
       .invoke(Task1, s, mem, n * 2)
       .invoke(Task2, s, n * 2);
   }

Instead, the computation should be done in the task itself:

.. code-block:: cpp

   void Task2(tapa::istream<int>& in, int n) {
     n = n * 2;
     // ...
   }

   void TopLevel(int n, tapa::mmap<int> mem) {
     tapa::stream<int> s("stream_name");
     tapa::task()
       .invoke(Task1, s, mem, n)
       .invoke(Task2, s, n);
   }

Working with Templates in Tasks
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

While TAPA doesn't support templated tasks directly, you can use template
functions within tasks:

.. code-block:: cpp

   template <typename T>
   void AdderTask(tapa::istream<T>& in, tapa::ostream<T>& out) {
     T a = in.read();
     T b = in.read();
     out.write(a + b);
   }

   void IntAdderTask(tapa::istream<int>& in, tapa::ostream<int>& out) {
     AdderTask<int>(in, out);
   }

.. warning::

   Only leaf tasks that does not invoke other tasks can be templated.

Correctly Defining Stream Arrays
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When defining an array of streams, use ``tapa::streams<>`` instead of
array of ``tapa::stream<>``:

.. code-block:: cpp

   // incorrect
   tapa::stream<int> data_q[4];

   // correct
   tapa::streams<int, 4> data_q;

Avoiding Static Variables
^^^^^^^^^^^^^^^^^^^^^^^^^

Be aware that static variables behave differently in software simulation
versus hardware synthesis, as each task instance will have its own local copy
of the variable in the generated hardware, compared to global sharing in
software. For example:

.. code-block:: cpp

   void Task() {
     static int counter = 0;
     counter++;
   }

   tapa::task().invoke(Task).invoke(Task);

In this example, the counter will be 2 in software simulation, as there is
only one instance of the task. However, in hardware, each task instance will
have its own copy of the counter, so the counter will be 1.

Other Debugging Tips
^^^^^^^^^^^^^^^^^^^^

Always ensure your design passes software simulation before moving to RTL
simulation or on-board testing, as it compiles faster and catches many
common errors. Additionally, software debugging approaches like GDB can be
used to debug software simulation.

For example, you may use AddressSanitizer to catch illegal memory access
during software simulation. To do so, compile your code with the
``-fsanitize=address -g`` options:

.. code-block:: bash

   tapa g++ vadd.cpp vadd-host.cpp -fsanitize=address -g -o vadd

This will help catch issues like buffer overflows.

If you encounter issues with memory access in simulation, you may use the
``tapa::mmap`` instead of ``tapa::async_mmap`` to simplify the memory access
model before moving to the more complex ``tapa::async_mmap``:

.. tip::

  If you encounter persistent issues, don't hesitate to open an issue in the
  TAPA repository with detailed reproduction steps.
