In addition to the cmake-based workflow, we can also run each step manually to exert maximum control.
This directory contains the reference scripts for a manual step-by-step execution.

- `generate_xo.sh` takes in the source C++ code and produces a Xilinx xo object and the floorplan constraint file

- `generate_bitstream.sh` takes in the xo object and the constraint file to run physical implementation.

- `reference_config_script_flow.ini` is a reference configuration file stating the mapping from logical ports to physical ports. Note that the format is different from its counterpart in the cmake flow. In the script-based flow, we need to add a `_1` suffix to the top name.

We use the `apps/vadd` example to illustrate how to run the program.

- Use `g++` to compile the host executable, for example:

```
g++ -o vadd -O2 vadd.cpp vadd-host.cpp -ltapa -lfrt -lglog -lgflags -lOpenCL
```

- Run simulation:

  - csim: 
    
    `./vadd`

  - cosim: 
    
    `./vadd --bitstream vadd.hw_emu.xclbin [other parameters]`
  
  - on-board execution: 
  
    `./vadd --bitstream vadd.hw.xclbin [other parameters]`

- We recently develop a faster cosim flow. 

  - To install:

  ```
  git clone https://github.com/UCLA-VAST/tapa-fast-cosim.git
  python3 -m pip install --editable tapa-fast-cosim
  ```

  - To run, replace the `.xclbin` object by the `.xo` object generate by tapac:

    `./vadd --bitstream [vadd.xo] [other parameters]`
