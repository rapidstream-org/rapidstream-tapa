"""Generate TAPA abgraph."""

__copyright__ = """
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

from tapa.abgraph.ab_graph import ABEdge, ABGraph, ABVertex, Area, convert_area
from tapa.core import Program

TAPA_PORT_PREFIX = "__tapa_port_"

MMAP_WIDTH = (405, 43)


def get_top_level_ab_graph(program: Program) -> ABGraph:
    """Generates the top level ab graph."""
    task_area = collect_task_area(program)
    fifo_width = collect_fifo_width(program)
    port_width = collect_port_width(program)
    graph = get_basic_ab_graph(program, task_area, fifo_width)
    graph = add_port_iface_connections(program, graph, port_width)
    return add_scalar_connections(program, graph, port_width)


def collect_fifo_width(program: Program) -> dict[str, int]:
    """Collect the width of the top level FIFOs."""
    fifo_width = {}
    for fifo_name in program.top_task.fifos:
        node_width = program.get_fifo_width(program.top_task, fifo_name)
        num_width = (
            int(node_width.left.left.value) - int(node_width.left.right.value)
        ) + 1
        fifo_width[fifo_name] = num_width

    return fifo_width


def collect_port_width(program: Program) -> dict[str, int | tuple[int, int]]:
    """Collect the width of the top level ports."""
    port_width = {}
    for port in program.top_task.ports.values():
        if port.cat.is_mmap:
            # if the port is mmap, both input and output width are not negligible
            port_width[port.name] = MMAP_WIDTH
        elif port.is_streams:
            port_width[port.name] = int(port.width) * len(
                get_streams_fifos(program.top_task.module, port.name)
            )
        else:
            port_width[port.name] = int(port.width)

    return port_width


def collect_task_area(program: Program) -> dict[str, dict[str, int]]:
    """Collect the area of tasks."""
    areas = {}
    for task_name in program.top_task.tasks:
        areas[task_name] = program.get_task(task_name).total_area

    return areas


def get_basic_ab_graph(
    program: Program,
    areas: dict[str, dict[str, int]],
    fifo_width: dict[str, int],
) -> ABGraph:
    """Generates the basic ab graph."""
    top = program.top_task
    vertices = {}
    edges = []

    for inst in top.instances:
        vertices[inst.name] = ABVertex(
            name=inst.name,
            area=convert_area(areas[inst.task.name]),
            sub_cells=(),
            target_slot=None,
            reserved_slot=None,
            current_slot=None,
        )

    for fifo_name, fifo_inst in top.fifos.items():
        # each fifo has a producer and a consumer, but the fifo_inst only tells
        # the task name, not the instance name. So we need to check all instances
        # port connection to find the instance name.
        consumer_task = fifo_inst["consumed_by"][0]
        consumer_task_inst = program.get_inst_by_port_arg_name(
            consumer_task, top, fifo_name
        ).name

        producer_task = fifo_inst["produced_by"][0]
        producer_task_inst = program.get_inst_by_port_arg_name(
            producer_task, top, fifo_name
        ).name

        edges.append(
            ABEdge(
                source_vertex=vertices[producer_task_inst],
                target_vertex=vertices[consumer_task_inst],
                width=fifo_width[fifo_name],
                index=len(edges),
            )
        )

    return ABGraph(vs=list(vertices.values()), es=edges)


def add_port_iface_connections(
    program: Program, graph: ABGraph, port_width: dict[str, int]
) -> ABGraph:
    """Add port interface connections to the ab graph.

    For each top level port interfaces, add a dummy vertex and
    corresponding edges to the graph.
    """
    vertices = {v.name: v for v in graph.vs}
    edges = graph.es.copy()

    top = program.top_task
    for port in top.ports.values():
        if not (port.cat.is_stream or port.cat.is_mmap):
            continue
        dummy_vertex = ABVertex(
            name=port.name,
            area=Area(lut=0, ff=0, bram_18k=0, dsp=0, uram=0),
            sub_cells=(),
            target_slot=None,
            reserved_slot=None,
            current_slot=None,
        )
        vertices[TAPA_PORT_PREFIX + port.name] = dummy_vertex

        connected_task = program.get_inst_by_port_arg_name(None, top, port.name).name

        # for mmap ports, both input and output width
        # are not negligible, so we need to add two edges
        if port.cat.is_mmap or port.cat.is_istream:
            if port.cat.is_mmap:
                width = port_width[port.name][1]
            else:
                width = port_width[port.name]
            edges.append(
                ABEdge(
                    source_vertex=dummy_vertex,
                    target_vertex=vertices[connected_task],
                    width=width,
                    index=len(edges),
                )
            )

        if port.cat.is_mmap or port.cat.is_ostream:
            if port.cat.is_mmap:
                width = port_width[port.name][0]
            else:
                width = port_width[port.name]
            edges.append(
                ABEdge(
                    source_vertex=vertices[connected_task],
                    target_vertex=dummy_vertex,
                    width=width,
                    index=len(edges),
                )
            )

    return ABGraph(vs=list(vertices.values()), es=edges)


def add_scalar_connections(
    program: Program, graph: ABGraph, port_width: dict[str, int]
) -> ABGraph:
    """"""
    vertices = {v.name: v for v in graph.vs}
    edges = graph.es.copy()

    top = program.top_task
    fsm_vertex = ABVertex(
        name=top.fsm_module.name,
        area=Area(lut=0, ff=0, bram_18k=0, dsp=0, uram=0),
        sub_cells=(),
        target_slot=None,
        reserved_slot=None,
        current_slot=None,
    )
    for port in top.ports.values():
        if not port.cat.is_scalar:
            continue
        vertices[top.fsm_module.name] = fsm_vertex
        connected_task = program.get_inst_by_port_arg_name(None, top, port.name).name

        # scalar always points from the FSM to the task
        edges.append(
            ABEdge(
                source_vertex=fsm_vertex,
                target_vertex=vertices[connected_task],
                width=port_width[port.name],
                index=len(edges),
            )
        )

    return ABGraph(vs=list(vertices.values()), es=edges)
