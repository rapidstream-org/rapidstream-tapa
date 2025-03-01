"""TAPA Tasks."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import collections
import decimal
import enum
import logging
import operator
from typing import ClassVar, NamedTuple

from pyverilog.vparser.ast import (
    Assign,
    Constant,
    Decl,
    Identifier,
    IntConst,
    ParamArg,
    Partselect,
    Plus,
    Width,
    Wire,
)

from tapa import __version__
from tapa.backend.xilinx import M_AXI_PREFIX
from tapa.instance import Instance, Port
from tapa.util import get_addr_width, get_indexed_name, range_or_none
from tapa.verilog.ast_utils import make_port_arg
from tapa.verilog.axi_xbar import generate as axi_xbar_generate
from tapa.verilog.util import wire_name
from tapa.verilog.xilinx.axis import (
    AXIS_CONSTANTS,
    STREAM_TO_AXIS,
    get_axis_port_width_int,
)
from tapa.verilog.xilinx.const import (
    HANDSHAKE_CLK,
    HANDSHAKE_OUTPUT_PORTS,
    HANDSHAKE_RST,
    HANDSHAKE_RST_N,
    HANDSHAKE_START,
    ISTREAM_SUFFIXES,
    OSTREAM_SUFFIXES,
    RST,
    STREAM_PORT_DIRECTION,
    get_stream_width,
)
from tapa.verilog.xilinx.m_axi import M_AXI_PORTS, get_m_axi_port_width
from tapa.verilog.xilinx.module import Module

_logger = logging.getLogger().getChild(__name__)


class MMapConnection(NamedTuple):
    id_width: int
    thread_count: int
    args: tuple[Instance.Arg, ...]
    chan_count: str | None
    chan_size: str | None


class Task:  # noqa: PLR0904
    """Describes a TAPA task.

    Attributes
    ----------
      level: Task.Level, upper or lower.
      name: str, name of the task, function name as defined in the source code.
      code: str, HLS C++ code of this task.
      tasks: A dict mapping child task names to json instance description objects.
      fifos: A dict mapping child fifo names to json FIFO description objects.
      ports: A dict mapping port names to Port objects for the current task.
      module: rtl.Module, should be attached after RTL code is generated.
      fsm_module: rtl.Module of the finite state machine (upper-level only).

    Properties:
      is_upper: bool, True if this task is an upper-level task.
      is_lower: bool, True if this task is an lower-level task.

    Properties unique to upper tasks:
      instances: A tuple of Instance objects, children instances of this task.
      args: A dict mapping arg names to lists of Arg objects that belong to the
          children instances of this task.
      mmaps: A dict mapping mmap arg names to MMapConnection objects.

    """

    class Level(enum.Enum):
        LOWER = 0
        UPPER = 1

    def __init__(  # noqa: PLR0917,PLR0913
        self,
        name: str,
        code: str,
        level: "Task.Level | str",
        tasks: dict[str, object] | None = None,
        fifos: dict[str, dict[str, dict[str, object]]] | None = None,
        ports: list[dict[str, str]] | None = None,
        target_type: str | None = None,
    ) -> None:
        if isinstance(level, str):
            if level == "lower":
                level = Task.Level.LOWER
            elif level == "upper":
                level = Task.Level.UPPER
        if not isinstance(level, Task.Level):
            raise TypeError("unexpected `level`: " + level)
        self.level = level
        self.name: str = name
        self.code: str = code
        self.tasks = {}
        self.fifos = {}
        self.target_type = target_type
        port_dict = {i.name: i for i in map(Port, ports or [])}
        if self.is_upper:
            self.tasks = dict(
                sorted(
                    (item for item in (tasks or {}).items()),
                    key=operator.itemgetter(0),
                ),
            )
            self.fifos = dict(
                sorted(
                    (item for item in (fifos or {}).items()),
                    key=operator.itemgetter(0),
                ),
            )
            self.ports = port_dict
            self.fsm_module = Module(name=f"{self.name}_fsm")
        elif ports:
            # Nonsynthesizable tasks need ports to generate template
            self.ports = port_dict
        self.module = Module(name=self.name)
        self._instances: tuple[Instance, ...] | None = None
        self._args: dict[str, list[Instance.Arg]] | None = None
        self._mmaps: dict[str, MMapConnection] | None = None
        self._self_area = {}
        self._total_area = {}
        self._clock_period = decimal.Decimal(0)

    @property
    def is_upper(self) -> bool:
        return self.level == Task.Level.UPPER

    @property
    def is_lower(self) -> bool:
        return self.level == Task.Level.LOWER

    @property
    def instances(self) -> tuple[Instance, ...]:
        if self._instances is not None:
            return self._instances
        msg = f"children of task {self.name} not populated"
        raise ValueError(msg)

    @instances.setter
    def instances(self, instances: tuple[Instance, ...]) -> None:
        self._instances = instances
        self._args = collections.defaultdict(list)

        mmaps: dict[str, list[Instance.Arg]] = collections.defaultdict(list)
        for instance in instances:
            for arg in instance.args:
                self._args[arg.name].append(arg)
                if arg.cat.is_mmap:
                    mmaps[arg.name].append(arg)

        self._mmaps = {}
        for arg_name, args in mmaps.items():
            # width of the ID port is the sum of the widest slave port plus bits
            # required to multiplex the slaves
            id_width = max(
                arg.instance.task.get_id_width(arg.port) or 1 for arg in args
            )
            id_width += (len(args) - 1).bit_length()
            thread_count = sum(
                arg.instance.task.get_thread_count(arg.port) for arg in args
            )
            mmap = MMapConnection(
                id_width,
                thread_count,
                tuple(args),
                self.ports[args[0].name].chan_count,
                self.ports[args[0].name].chan_size,
            )
            assert all(self.ports[x.name].chan_count == mmap.chan_count for x in args)
            assert all(self.ports[x.name].chan_size == mmap.chan_size for x in args)
            self._mmaps[arg_name] = mmap

            for arg in args:
                arg.chan_count = mmap.chan_count
                arg.chan_size = mmap.chan_size

            if len(args) > 1:
                _logger.debug(
                    "mmap argument '%s.%s'"
                    " (id_width=%d, thread_count=%d, chan_size=%s, chan_count=%s)"
                    " is shared by %d ports:",
                    self.name,
                    arg_name,
                    mmap.id_width,
                    mmap.thread_count,
                    mmap.chan_count,
                    mmap.chan_size,
                    len(args),
                )
                for arg in args:
                    arg.shared = True
                    _logger.debug("  %s.%s", arg.instance.name, arg.port)
            else:
                _logger.debug(
                    "mmap argument '%s.%s'"
                    " (id_width=%d, thread_count=%d, chan_size=%s, chan_count=%s)"
                    " is connected to port '%s.%s'",
                    self.name,
                    arg_name,
                    mmap.id_width,
                    mmap.thread_count,
                    mmap.chan_count,
                    mmap.chan_size,
                    args[0].instance.name,
                    args[0].port,
                )

    @property
    def args(self) -> dict[str, list[Instance.Arg]]:
        if self._args is not None:
            return self._args
        msg = f"children of task {self.name} not populated"
        raise ValueError(msg)

    @property
    def mmaps(self) -> dict[str, MMapConnection]:
        if self._mmaps is not None:
            return self._mmaps
        msg = f"children of task {self.name} not populated"
        raise ValueError(msg)

    @property
    def self_area(self) -> dict[str, int]:
        if self._self_area:
            return self._self_area
        msg = f"area of task {self.name} not populated"
        raise ValueError(msg)

    @self_area.setter
    def self_area(self, area: dict[str, int]) -> None:
        if self._self_area:
            msg = f"area of task {self.name} already populated"
            raise ValueError(msg)
        self._self_area = area

    @property
    def total_area(self) -> dict[str, int]:
        if self._total_area:
            return self._total_area

        area = dict(self.self_area)
        for instance in self.instances:
            for key in area:
                area[key] += instance.task.total_area[key]
        return area

    @total_area.setter
    def total_area(self, area: dict[str, int]) -> None:
        if self._total_area:
            msg = f"total area of task {self.name} already populated"
            raise ValueError(msg)
        self._total_area = area

    @property
    def clock_period(self) -> decimal.Decimal:
        if self.is_upper:
            return max(
                self._clock_period,
                *(x.clock_period for x in {y.task for y in self.instances}),
            )
        if self._clock_period:
            return self._clock_period
        msg = f"clock period of task {self.name} not populated"
        _logger.warning(msg)
        return decimal.Decimal(0)

    @clock_period.setter
    def clock_period(self, clock_period: decimal.Decimal) -> None:
        if self._clock_period:
            msg = f"clock period of task {self.name} already populated"
            raise ValueError(msg)
        self._clock_period = clock_period

    @property
    def report(self) -> dict[str, dict]:
        performance = {
            "source": "hls",
            "clock_period": str(self.clock_period),
        }

        area = {
            "source": "synth" if self._total_area else "hls",
            "total": self.total_area,
        }

        if self.is_upper:
            performance["critical_path"] = {}
            area["breakdown"] = {}
            for instance in self.instances:
                task_report = instance.task.report

                if self.clock_period == instance.task.clock_period:
                    performance["critical_path"].setdefault(
                        instance.task.name,
                        task_report["performance"],
                    )

                area["breakdown"].setdefault(
                    instance.task.name,
                    {"count": 0, "area": task_report["area"]},
                )["count"] += 1

        return {
            "schema": __version__,
            "name": self.name,
            "performance": performance,
            "area": area,
        }

    def get_id_width(self, port: str) -> int | None:
        if port in self.mmaps:
            return self.mmaps[port].id_width or None
        return None

    def get_thread_count(self, port: str) -> int:
        if port in self.mmaps:
            return self.mmaps[port].thread_count
        return 1

    _DIR2CAT: ClassVar = {"produced_by": "ostream", "consumed_by": "istream"}

    def get_connection_to(
        self,
        fifo_name: str,
        direction: str,
    ) -> tuple[str, int, str]:
        """Get port information to which a given FIFO is connected."""
        if direction not in self._DIR2CAT:
            msg = f"invalid direction: {direction}"
            raise ValueError(msg)
        if direction not in self.fifos[fifo_name]:
            msg = f"{fifo_name} is not {direction} any task"
            raise ValueError(msg)
        task_name, task_idx = self.fifos[fifo_name][direction]
        for port, arg in self.tasks[task_name][task_idx]["args"].items():
            if arg["cat"] == self._DIR2CAT[direction] and arg["arg"] == fifo_name:
                return task_name, task_idx, port
        msg = f"task {self.name} has inconsistent metadata"
        raise ValueError(msg)

    def get_fifo_directions(self, fifo_name: str) -> list[str]:
        return [
            direction
            for direction in ["consumed_by", "produced_by"]
            if direction in self.fifos[fifo_name]
        ]

    @staticmethod
    def get_fifo_suffixes(direction: str) -> list[str]:
        suffixes = {
            "consumed_by": ISTREAM_SUFFIXES,
            "produced_by": OSTREAM_SUFFIXES,
        }
        return suffixes[direction]

    def is_fifo_external(self, fifo_name: str) -> bool:
        return "depth" not in self.fifos[fifo_name]

    def assign_directional(self, a: object, b: object, a_direction: str) -> None:
        if a_direction == "input":
            self.module.add_logics([Assign(left=a, right=b)])
        elif a_direction == "output":
            self.module.add_logics([Assign(left=b, right=a)])

    def convert_axis_to_fifo(self, axis_name: str) -> str:
        assert len(self.get_fifo_directions(axis_name)) == 1, (
            "axis interfaces should have one direction"
        )
        direction_axis = {
            "consumed_by": "produced_by",
            "produced_by": "consumed_by",
        }[self.get_fifo_directions(axis_name)[0]]
        data_width = self.ports[axis_name].width

        # add FIFO registerings to provide timing isolation
        fifo_name = "tapa_fifo_" + axis_name
        self.module.add_fifo_instance(
            name=fifo_name,
            rst=RST,
            width=Plus(IntConst(data_width), IntConst(1)),
            depth=2,
        )

        # add FIFO's wires
        for suffix in STREAM_PORT_DIRECTION:
            w_name = wire_name(fifo_name, suffix)
            wire_width = get_stream_width(suffix, data_width)
            self.module.add_signals([Wire(name=w_name, width=wire_width)])

        # add constant outputs for AXIS output ports
        if direction_axis == "consumed_by":
            for axis_suffix, bit in AXIS_CONSTANTS.items():
                port_name = self.module.find_port(axis_name, axis_suffix)
                width = get_axis_port_width_int(axis_suffix, data_width)

                self.module.add_logics(
                    [
                        Assign(
                            left=Identifier(port_name),
                            right=IntConst(f"{width}'b{str(bit) * width}"),
                        ),
                    ],
                )

        # connect the FIFO to the AXIS interface
        for suffix in self.get_fifo_suffixes(direction_axis):
            w_name = wire_name(fifo_name, suffix)

            offset = 0
            for axis_suffix in STREAM_TO_AXIS[suffix]:
                port_name = self.module.find_port(axis_name, axis_suffix)
                width = get_axis_port_width_int(axis_suffix, data_width)

                if len(STREAM_TO_AXIS[suffix]) > 1:
                    wire = Partselect(
                        Identifier(w_name),
                        IntConst(str(offset + width - 1)),
                        IntConst(str(offset)),
                    )
                else:
                    wire = Identifier(w_name)

                self.assign_directional(
                    Identifier(port_name),
                    wire,
                    STREAM_PORT_DIRECTION[suffix],
                )

                offset += width

        return fifo_name

    def connect_fifo_externally(self, internal_name: str, axis: bool) -> None:
        assert len(self.get_fifo_directions(internal_name)) == 1, (
            "externally connected fifos should have one direction"
        )
        direction = self.get_fifo_directions(internal_name)[0]
        external_name = (
            self.convert_axis_to_fifo(internal_name) if axis else internal_name
        )

        # connect fifo with external ports
        for suffix in self.get_fifo_suffixes(direction):
            if external_name == internal_name:
                rhs = self.module.get_port_of(external_name, suffix).name
            else:
                rhs = wire_name(external_name, suffix)
            internal_signal = Identifier(wire_name(internal_name, suffix))
            self.assign_directional(
                internal_signal,
                Identifier(rhs),
                STREAM_PORT_DIRECTION[suffix],
            )

    def add_m_axi(  # noqa: C901,PLR0912,PLR0914
        self,
        width_table: dict[str, int],
        files: dict[str, str],
    ) -> None:
        for arg_name, mmap in self.mmaps.items():  # noqa: PLR1702
            m_axi_id_width, m_axi_thread_count, args, chan_count, chan_size = mmap
            # add m_axi ports to the arg list
            for idx in range_or_none(chan_count):
                self.module.add_m_axi(
                    name=get_indexed_name(arg_name, idx),
                    data_width=width_table[arg_name],
                    id_width=m_axi_id_width or None,
                )
            if len(args) == 1 and chan_count is None:
                continue

            # add AXI interconnect if necessary
            assert m_axi_id_width is not None
            assert (m_axi_thread_count > 1) == (len(args) > 1)

            # S_ID_WIDTH must be at least 1
            s_axi_id_width = max(
                arg.instance.task.get_id_width(arg.port) or 1 for arg in args
            )

            portargs = [
                make_port_arg(port="clk", arg=HANDSHAKE_CLK),
                make_port_arg(port="rst", arg=HANDSHAKE_RST),
            ]

            # upstream m_axi
            for idx in range_or_none(chan_count):
                for axi_chan, axi_ports in M_AXI_PORTS.items():
                    for axi_port, direction in axi_ports:
                        name = get_indexed_name(arg_name, idx)
                        axi_arg_name = f"{M_AXI_PREFIX}{name}_{axi_chan}{axi_port}"
                        axi_arg_name_raw = axi_arg_name
                        if idx is not None and axi_port == "ADDR":
                            # set mmap offset for hmap
                            axi_arg_name_raw += "_raw"
                            self.module.add_signals(
                                [
                                    Wire(
                                        name=axi_arg_name_raw,
                                        width=Width(
                                            msb=Constant(63),
                                            lsb=Constant(0),
                                        ),
                                    ),
                                ],
                            )
                            # assume power-of-2 address allocation
                            assert chan_size is not None
                            addr_width = get_addr_width(
                                chan_size,
                                width_table[arg_name],
                            )
                            self.module.add_logics(
                                [
                                    Assign(
                                        left=Identifier(axi_arg_name),
                                        right=Identifier(
                                            f"{{{name}[63:{addr_width}], "
                                            f"{axi_arg_name_raw}[{addr_width - 1}:0]}}",
                                        ),
                                    ),
                                ],
                            )
                        portargs.append(
                            make_port_arg(
                                port=f"m{idx or 0:02d}_axi_"
                                f"{axi_chan.lower()}{axi_port.lower()}",
                                arg=axi_arg_name_raw,
                            ),
                        )

            # downstrean s_axi
            for idx, arg in enumerate(args):
                wires = []
                id_width = arg.instance.task.get_id_width(arg.port)
                for axi_chan, axi_ports in M_AXI_PORTS.items():
                    for axi_port, direction in axi_ports:
                        wire_name = (
                            f"{M_AXI_PREFIX}{arg.mmap_name}_{axi_chan}{axi_port}"
                        )
                        wires.append(
                            Wire(
                                name=wire_name,
                                width=get_m_axi_port_width(
                                    port=axi_port,
                                    data_width=width_table[arg_name],
                                    id_width=id_width,
                                ),
                            ),
                        )
                        if axi_port == "ID":
                            id_width = id_width or 1
                            if id_width != s_axi_id_width and direction == "output":
                                # make sure inputs to s_axi of interconnect won't be X
                                wire_name = (
                                    f"{{{s_axi_id_width - id_width}'d0, {wire_name}}}"
                                )
                        portargs.append(
                            make_port_arg(
                                port=f"s{idx:02d}_axi_{axi_chan.lower()}{axi_port.lower()}",
                                arg=wire_name,
                            ),
                        )
                self.module.add_signals(wires)

            data_width = max(width_table[arg_name], 32)
            assert data_width in {32, 64, 128, 256, 512, 1024}
            module_name = f"axi_crossbar_{len(args)}x{chan_count or 1}"
            if f"{module_name}.v" not in files:
                files[f"{module_name}.v"] = axi_xbar_generate(
                    ports=(len(args), chan_count or 1),
                    name=module_name,
                )

            paramargs = [
                ParamArg("DATA_WIDTH", Constant(data_width)),
                ParamArg("ADDR_WIDTH", Constant(64)),
                ParamArg("S_ID_WIDTH", Constant(s_axi_id_width)),
                ParamArg("M_ID_WIDTH", Constant(m_axi_id_width)),
            ]
            for idx in range(chan_count or 1):
                addr_width = get_addr_width(chan_size, width_table[arg_name])
                paramargs.extend(
                    [
                        ParamArg(f"M{idx:02d}_ADDR_WIDTH", Constant(addr_width)),
                        ParamArg(f"M{idx:02d}_ISSUE", Constant(16)),
                    ],
                )
            paramargs.extend(
                ParamArg(
                    f"S{idx:02d}_THREADS",
                    Constant(arg.instance.task.get_thread_count(arg.port)),
                )
                for idx, arg in enumerate(args)
            )
            self.module.add_instance(
                module_name=module_name,
                instance_name=f"{module_name}__{arg_name}",
                ports=portargs,
                params=paramargs,
            )

    def add_rs_pragmas_to_fsm(self) -> None:
        """Add RapidStream pragmas to the FSM module."""
        port_map_str = " ".join(
            f"{x}={x}" for x in (HANDSHAKE_START, *HANDSHAKE_OUTPUT_PORTS)
        )
        scalar_regex_str = "|".join(
            x.name
            for x in self.ports.values()
            if x.name in self.fsm_module.ports  # skip unused ports
            and (not x.cat.is_stream and not x.is_streams)  # TODO: refactor port.cat
        )
        scalar_pragma = f" scalar=({scalar_regex_str})" if scalar_regex_str else ""
        pragma_list = [
            f"clk port={HANDSHAKE_CLK}",
            f"rst port={HANDSHAKE_RST_N} active=low",
            f"ap-ctrl {port_map_str}{scalar_pragma}",
        ]
        for instance in self.instances:
            port_list = [HANDSHAKE_START]
            if not instance.is_autorun:
                port_list.extend(HANDSHAKE_OUTPUT_PORTS)
            port_map_str = " ".join(
                f"{x}={wire_name(instance.name, x)}" for x in port_list
            )
            if all(arg.cat.is_stream or "'d" in arg.name for arg in instance.args):
                scalar_pragma = ""
            else:
                scalar_pragma = f" scalar={instance.get_instance_arg('.*')}"
            pragma_list.append(f"ap-ctrl {port_map_str}{scalar_pragma}")
        self.fsm_module.add_ports(
            [Decl([Identifier(f"// pragma RS {pragma}\n") for pragma in pragma_list])]
        )
