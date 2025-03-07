"""Graph based on Pydantic for the partitioning algorithm."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""

import networkx as nx
from pydantic import BaseModel, ConfigDict

from tapa.abgraph.device.common import RESOURCES, Coor
from tapa.abgraph.device.virtual_device import Area
from tapa.abgraph.slot_utilities import get_coor_from_slot_name


class ABSlot(BaseModel):
    """Represents a slot in the AutoBridge partitioning algorithm."""

    model_config = ConfigDict(frozen=True)

    name: str
    slot_coor: Coor

    total_resources: Area
    usable_resources: Area
    usage_limit: dict[str, float]

    grid_centroid_x: float
    grid_centroid_y: float

    def has_overlap(self, other_slot: str) -> bool:
        """Check if the current node overlaps with a slot."""
        return self.slot_coor.has_overlap(get_coor_from_slot_name(other_slot))

    def __hash__(self) -> int:
        """Get the hash."""
        # ruff: noqa: SLF001
        return hash(self.slot_coor._key())


class ABCut(BaseModel):
    """Represents a cut in the AutoBridge partitioning algorithm."""

    name: str
    lhs: list[ABSlot]
    rhs: list[ABSlot]
    capacity: int

    def __hash__(self) -> int:
        """Get the hash."""
        return hash(self.name)

    def __repr__(self) -> str:
        """Get a printable version."""
        return self.name

    def __eq__(self, other: object) -> bool:
        """If equal."""
        return (
            self.__hash__() == other.__hash__() if isinstance(other, ABCut) else False
        )


class ABVertex(BaseModel):
    """Represents a vertex in the AutoBridge graph."""

    name: str
    sub_cells: tuple[str, ...]
    area: Area
    target_slot: str | None
    reserved_slot: str | None

    current_slot: Coor | None = None

    def _key(self) -> str:
        """Return a key for the vertex."""
        return self.name

    def __hash__(self) -> int:
        """Return a hash for the vertex."""
        return hash(self._key())

    def __eq__(self, other: object) -> bool:
        """Return whether two vertices are equal."""
        if not isinstance(other, ABVertex):
            return NotImplemented
        return self._key() == other._key()

    def __lt__(self, other: object) -> bool:
        """Return whether one vertex is less than another."""
        if not isinstance(other, ABVertex):
            return NotImplemented
        return self._key() < other._key()


class ABEdge(BaseModel):
    """Represents an edge in the AutoBridge graph."""

    model_config = ConfigDict(frozen=True)

    source_vertex: ABVertex
    target_vertex: ABVertex

    index: int
    width: int


class ABGraph(BaseModel):
    """Represents a graph in the AutoBridge partitioning algorithm."""

    vs: list[ABVertex]
    es: list[ABEdge]

    def ecount(self) -> int:
        """Return the number of edges in the graph."""
        return len(self.es)


def get_ab_graphx(graph: nx.Graph) -> ABGraph:
    """Convert an networkx graph to an AutoBridge graph."""
    edges = []

    vertices = [get_ab_vertexx(v, graph) for v in graph.nodes]

    for idx, e in enumerate(graph.edges):
        src, tgt = e
        edges.append(get_ab_edgex(src, tgt, graph, idx))

    return ABGraph(vs=vertices, es=edges)


def get_ab_vertexx(v: int | str, g: nx.Graph) -> ABVertex:
    """Convert an networkx vertex to an AutoBridge vertex."""
    return ABVertex(
        name=g.nodes[v]["name"],
        sub_cells=tuple(g.nodes[v]["sub_cells"]),
        area=convert_area(g.nodes[v]["area"]),
        target_slot=g.nodes[v]["target_slot"],
        reserved_slot=g.nodes[v]["reserved_slot"],
    )


def get_ab_edgex(src: int | str, tgt: int | str, g: nx.Graph, edge_idx: int) -> ABEdge:
    """Convert an networkx edge to an AutoBridge edge."""
    return ABEdge(
        source_vertex=get_ab_vertexx(src, g),
        target_vertex=get_ab_vertexx(tgt, g),
        index=edge_idx,
        width=g.get_edge_data(src, tgt)["width"],
    )


def convert_area(area_dict: dict[str, float] | dict[str, int]) -> Area:
    """Convert a dictionary to an Area object."""
    assert all(k in RESOURCES for k in area_dict)

    return Area(
        lut=int(area_dict["LUT"]),
        ff=int(area_dict["FF"]),
        bram_18k=int(area_dict["BRAM_18K"]),
        dsp=int(area_dict["DSP"]),
        uram=int(area_dict["URAM"]),
    )
