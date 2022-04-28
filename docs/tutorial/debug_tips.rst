Debug Tips
--------------

- Calm down and read all warning/error messages in the log :)
- Check if all streams are passed by reference.
- Check if ``tapa::mmap`` is passed by value and ``tapa::async_mmap`` is passed by reference.
- Check if you pass the correct parameters to each task Invocation.
- All tasks must be defined in the same file.
- No computation is allowed in an ``upper`` task. An ``upper`` task should only contain: (1) stream definitions (2) task instance definitions through ``tapa::invoke()``.
- We do not support templated tasks yet, but sub-functions within a task can be a template function. So you could write a wrapper function if you do need to use template.
- Make sure that software simulation passes before trying RTL simulation or on-board testing.
- If possible, first use ``tapa::mmap`` to get things right before adapting to ``tapa::async_mmap``.
- In software simulation, add ``-fsanitize=address -g`` options to ``g++`` to catch illegal memory access.
- If you want to define an array of ``tapa::stream``, use ``tapa::streams<> foo;`` instead of ``tapa::stream<> foo[N];``
- If you define ``static`` variables in a task and invoke the task multiple times, the behavior of the software and the hardware will be different. In the generated hardware, each task instance will have its own local copy of the variable and there will be no global sharing.
- AutoBridge will generate a ``.dot`` file, use it to visualize the topology of your design.
- If none of the above works, feel free to open an issue at the TAPA repo with instructions to reproduce your error.
