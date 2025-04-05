"""Graph object class in TAPA."""

__copyright__ = """
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
"""
import copy
import re
from collections import defaultdict
from functools import lru_cache

from tapa.common.base import Base
from tapa.common.floorplan.gen_slot_cpp import gen_slot_cpp
from tapa.common.interconnect_instance import InterconnectInstance
from tapa.common.task_definition import TaskDefinition
from tapa.common.task_instance import TaskInstance
from tapa.util import get_instance_name


class Graph(Base):
    """TAPA task graph."""

    @lru_cache(None)
    def get_task_def(self, name: str) -> TaskDefinition:
        """Returns the task definition which is named `name`."""
        assert isinstance(self.obj["tasks"], dict)
        return TaskDefinition(name, self.obj["tasks"][name], self)

    @lru_cache(None)
    def get_top_task_def(self) -> TaskDefinition:
        """Returns the top-level task instance."""
        return self.get_task_def(self.get_top_task_name())

    @lru_cache(None)
    def get_top_task_inst(self) -> TaskInstance:
        """Returns the top-level task instance."""
        name = self.get_top_task_name()
        return TaskInstance(0, name, None, self, self.get_top_task_def())

    @lru_cache(None)
    def get_top_task_name(self) -> str:
        """Returns the name of the top-level task."""
        assert isinstance(self.obj["top"], str)
        return self.obj["top"]

    def get_flatten_graph(self) -> "Graph":
        """Returns the flatten graph with all leaf tasks as the top's children."""
        # Construct obj of the new flatten graph
        new_obj = self.to_dict()

        # Find all leaf task instances
        leaves = self.get_top_task_inst().get_leaf_tasks_insts()

        # obj['tasks']:
        # Reconstruct the task definitions of the graph
        #   are (1) all relevant definitions of the leaf tasks
        defs = {leaf.definition for leaf in leaves}
        new_obj["tasks"] = {d.name: d.to_dict() for d in defs}
        #   and (2) also the top task
        top_name = self.obj["top"]
        assert isinstance(top_name, str)
        assert isinstance(new_obj["tasks"], dict)
        new_obj["tasks"][top_name] = self.get_top_task_def().to_dict()

        # obj['tasks'][top_task]['tasks']:
        # Reconstruct the subtasks of the top level task
        #   insts = {definition: [instance, ...], ...}
        insts = {i: [j for j in leaves if j.definition is i] for i in defs}
        #   {definition.name: [instance.to_dict(), ...], ...}
        new_subtask_instantiations = {
            definition.name: [
                inst.to_dict(interconnect_global_name=True)
                for inst in insts_of_definition
            ]
            for definition, insts_of_definition in insts.items()
        }
        new_obj["tasks"][top_name]["tasks"] = new_subtask_instantiations

        # obj['tasks'][top_task]['fifos']:
        # Reconstruct the local interconnects of the top level task
        #   -> [instance, ...]
        interconnect_insts = self.get_top_task_inst().recursive_get_interconnect_insts()
        #   -> {instance.gid: instance.definition.to_dict()}
        new_obj["tasks"][top_name]["fifos"] = {
            i.global_name: i.to_dict(
                insts_override=new_subtask_instantiations,
                interconnect_global_name=True,
            )
            for i in interconnect_insts
        }

        return Graph(self.name, new_obj)

    def get_floorplan_slot(
        self, slot_name: str, task_inst_in_slot: list[str], top: Base
    ) -> TaskDefinition:
        """Return the task grouping the tasks floorplanned in the given slot."""
        # Construct obj of the slot by modifying the top task
        new_obj = self.get_top_task_def().to_dict()

        new_obj["level"] = "upper"

        # Find all task instances
        insts = self.get_top_task_inst().get_subtasks_insts()
        assert all(inst in [inst.name for inst in insts] for inst in task_inst_in_slot)
        for inst in insts:
            # make sure top has been flattened
            assert isinstance(inst.definition, TaskDefinition)
            assert inst.definition.get_level() == TaskDefinition.Level.LEAF, (
                "Top task must be flattened for floorplanning"
            )

        # filter out tasks that are not in the slot
        new_insts = [inst for inst in insts if inst.name in task_inst_in_slot]

        assert new_insts, [inst.name for inst in insts]

        # obj['tasks']:
        # construct the task insts of the slot
        top_to_slot_inst_idx_map = defaultdict(dict)
        top_tasks = self.get_top_task_def().to_dict()["tasks"]
        assert isinstance(top_tasks, dict)

        new_obj["tasks"] = defaultdict(list)
        for inst in new_insts:
            assert isinstance(top_tasks[inst.definition.name], list)

            top_idx = top_tasks[inst.definition.name].index(inst.obj)
            top_to_slot_inst_idx_map[inst.definition.name][top_idx] = len(
                new_obj["tasks"][inst.definition.name]
            )

            new_obj["tasks"][inst.definition.name].append(inst.obj)

        # obj['fifos']:
        # construct the fifos of the slot
        fifos = self.get_top_task_inst().get_interconnect_insts()
        fifo_ports: list[str] = []
        new_obj["fifos"], fifo_ports = _get_slot_fifos(
            fifos,
            top_to_slot_inst_idx_map,
            task_inst_in_slot,
        )

        # obj['ports']:
        # Reconstruct the ports of the slot
        # Remove all ports that are not used by the insts in the slot
        used_args: set[str] = set()
        for inst in new_insts:
            assert isinstance(inst.obj["args"], dict)
            args = inst.obj["args"]
            assert isinstance(args, dict)
            for arg in args.values():
                assert isinstance(arg, dict)
                assert isinstance(arg["arg"], str)
                used_args.add(arg["arg"])
        assert isinstance(new_obj["ports"], list)
        new_ports = [port for port in new_obj["ports"] if port["name"] in used_args]

        # Add fifos connecting the slot and the outside as ports
        # Find the port on inst that connects to the fifo and copy
        # the port to the slot
        new_ports += _get_used_ports(new_insts, fifo_ports)

        new_obj["ports"] = new_ports

        top_code = new_obj["code"]
        assert isinstance(top_code, str)
        new_obj["code"] = gen_slot_cpp(
            slot_name,
            self.get_top_task_name(),
            new_ports,
            top_code,
        )

        return TaskDefinition(slot_name, new_obj, top)

    def get_floorplan_top(
        self,
        slot_defs: dict[str, TaskDefinition],
        task_inst_to_slot: dict[str, str],
    ) -> TaskDefinition:
        """Return the new top level by grouping slot instances."""
        top_obj = self.get_top_task_def().to_dict()
        assert top_obj["level"] != "lower"
        new_top_obj = copy.deepcopy(top_obj)

        # obj['tasks']:
        # construct slot instances
        new_top_insts = defaultdict(list)
        for slot_name, slot_def in slot_defs.items():
            assert isinstance(slot_def.obj["ports"], list)
            ports: dict[str, dict] = {p["name"]: p for p in slot_def.obj["ports"]}
            args = {}
            slot_subtasks = slot_def.obj["tasks"]
            assert isinstance(slot_subtasks, dict)
            for port_name in ports:
                # format port[idx] to port_idx for array
                port_name_formated = re.sub(
                    r"\[([^\]]+)\]$", lambda m: f"_{m.group(1)}", port_name
                )
                args[port_name_formated] = {
                    "arg": port_name,
                    "cat": _infer_arg_cat_from_subinst(port_name, slot_subtasks),
                }
            new_top_insts[slot_name].append({"args": args, "step": 0})
        new_top_obj["tasks"] = new_top_insts

        # obj['fifos']:
        # remove all fifos except external fifo and the ones connecting the slots
        in_slot_fifos = []
        for slot_def in slot_defs.values():
            assert isinstance(slot_def.obj["fifos"], dict)
            for fifo_name, fifo in slot_def.obj["fifos"].items():
                assert isinstance(fifo, dict)
                # keep external fifos
                if "depth" not in fifo:
                    continue
                in_slot_fifos.append(fifo_name)

        assert isinstance(top_obj["fifos"], dict)
        new_top_obj["fifos"] = {}
        for name, fifo in top_obj["fifos"].items():
            if name in in_slot_fifos:
                continue
            updated_fifo = copy.deepcopy(fifo)
            if "consumed_by" in updated_fifo:
                consumer_name = fifo["consumed_by"][0]
                consumer_top_idx = fifo["consumed_by"][1]
                updated_fifo["consumed_by"] = (
                    task_inst_to_slot[
                        get_instance_name((consumer_name, consumer_top_idx))
                    ],
                    0,
                )

            if "produced_by" in updated_fifo:
                producer_name = fifo["produced_by"][0]
                producer_top_idx = fifo["produced_by"][1]
                updated_fifo["produced_by"] = (
                    task_inst_to_slot[
                        get_instance_name((producer_name, producer_top_idx))
                    ],
                    0,
                )

            new_top_obj["fifos"][name] = updated_fifo

        return TaskDefinition(self.get_top_task_name(), new_top_obj, self)

    def get_floorplan_graph(self, slot_to_insts: dict[str, list[str]]) -> "Graph":
        """Generate floorplanned graph."""
        new_obj = self.to_dict()

        assert isinstance(new_obj["tasks"], dict)
        slot_defs = {}
        for slot_name, insts in slot_to_insts.items():
            slot_def = self.get_floorplan_slot(slot_name, insts, self)
            assert slot_name not in new_obj["tasks"]
            new_obj["tasks"][slot_name] = slot_def.to_dict()
            slot_defs[slot_name] = slot_def

        inst_to_slot = {}
        for slot_name, insts in slot_to_insts.items():
            for inst in insts:
                assert isinstance(inst, str)
                inst_to_slot[inst] = slot_name

        top_name = self.get_top_task_name()
        assert top_name in new_obj["tasks"]
        new_obj["tasks"][top_name] = self.get_floorplan_top(
            slot_defs, inst_to_slot
        ).to_dict()

        return Graph(self.name, new_obj)


def _infer_arg_cat_from_subinst(port_name: str, tasks: dict[str, dict]) -> str:
    """Infer port arg category from child instance connecting to the port."""
    cat: set[str] = set()
    for task_insts in tasks.values():
        for inst in task_insts:
            assert isinstance(inst["args"], dict)
            args = inst["args"]
            assert isinstance(args, dict)
            for arg in args.values():
                if arg["arg"] == port_name:
                    cat.add(arg["cat"])
    assert len(cat) == 1
    return cat.pop()


def _get_used_ports(new_insts: list[TaskInstance], fifo_ports: list[str]) -> list:
    """Find the port on inst that connects to the fifo."""
    new_ports = []
    for inst in new_insts:
        assert isinstance(inst.obj["args"], dict)
        args = inst.obj["args"]
        assert isinstance(args, dict)
        for port_name, arg in args.items():
            port_name_no_idx = re.sub(r"\[[^\]]+\]$", "", port_name)
            if arg["arg"] not in fifo_ports:
                continue
            ports = inst.definition.obj["ports"]
            assert isinstance(ports, list)
            for port in ports:
                if port["name"] != port_name_no_idx:
                    continue
                new_port = port.copy()
                new_port["name"] = arg["arg"]
                new_ports.append(new_port)

    assert len(new_ports) == len(fifo_ports), f"{new_ports}, {fifo_ports}"
    return new_ports


def _update_fifo_inst_idx(
    fifo: dict, top_to_slot_inst_idx_map: dict[str, dict]
) -> dict:
    """Update top fifo consumer and producer index to slot index."""
    fifo_obj = copy.deepcopy(fifo)

    if "consumed_by" in fifo_obj:
        consumer_name = fifo_obj["consumed_by"][0]
        consumer_top_idx = fifo_obj["consumed_by"][1]
        fifo_obj["consumed_by"] = (
            consumer_name,
            top_to_slot_inst_idx_map[consumer_name][consumer_top_idx],
        )

    if "produced_by" in fifo_obj:
        producer_name = fifo_obj["produced_by"][0]
        producer_top_idx = fifo_obj["produced_by"][1]
        assert producer_top_idx in top_to_slot_inst_idx_map[producer_name]
        fifo_obj["produced_by"] = (
            producer_name,
            top_to_slot_inst_idx_map[producer_name][producer_top_idx],
        )

    return fifo_obj


def _get_slot_fifos(
    fifos: list[InterconnectInstance],
    top_to_slot_inst_idx_map: dict[str, dict],
    task_inst_in_slot: list[str],
) -> tuple[dict, list[str]]:
    """Get the fifos of the slot.

    returns:
    - new_fifos: dict of updated slot fifos
    - fifo_ports: list of external fifo names.
    """
    new_fifos: dict[str, dict] = defaultdict(dict)
    fifo_ports: list[str] = []
    for fifo in fifos:
        assert fifo.name
        fifo_obj = fifo.to_dict()
        # Remove original external fifos
        if "depth" not in fifo_obj:
            continue
        src = fifo_obj["consumed_by"]
        dst = fifo_obj["produced_by"]
        # For fifo connecting task insts inside the slot, keep it
        if (
            get_instance_name((src[0], src[1])) in task_inst_in_slot
            and get_instance_name((dst[0], dst[1])) in task_inst_in_slot
        ):
            new_fifos[fifo.name] = fifo_obj
        # For fifo connecting a task inst inside and an inst outside the slot,
        # modify to an external fifo and put it to ports later
        elif get_instance_name((src[0], src[1])) in task_inst_in_slot:
            new_fifos[fifo.name] = {
                "consumed_by": src,
            }
            fifo_ports.append(fifo.name)
        elif get_instance_name((dst[0], dst[1])) in task_inst_in_slot:
            new_fifos[fifo.name] = {
                "produced_by": dst,
            }
            fifo_ports.append(fifo.name)

    new_fifos_obj = {}
    for fifo_name, fifo_obj in new_fifos.items():
        new_fifos_obj[fifo_name] = _update_fifo_inst_idx(
            fifo_obj, top_to_slot_inst_idx_map
        )

    return new_fifos_obj, fifo_ports
