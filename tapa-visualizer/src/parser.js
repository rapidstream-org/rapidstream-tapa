/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { getComboId } from "./helper.js";

/** Color for node with more than 1 connection */
const altNodeColor = "#0F5132"; // Bootstrap $green-700

/* Color for edge connecting parent and children */
const altEdgeColor = "#479f76"; //  Bootstrap $green-400

/** @type {<K, V>(map: Map<K, Set<V>>, key: K, value: V) => void} */
const addToMappedSet = (map, key, value) => {
  const set = map.get(key);
  set ? set.add(value) : map.set(key, new Set([value]));
};

/** [1,2,3,5] -> "1~3,5"
 * @type {(indexArr: number[]) => string} */
const getIndexRange = indexes => {
  // Only 1 index, just return it as string
  if (indexes.length === 1) return `${indexes[0]}`;

  // More than 1 index, sort & parse to "a~b,c,d~e"
  indexes.sort((a, b) => a - b);

  // continuous groups of indexs
  const groups = [[indexes[0]]];
  for (let i = 1; i < indexes.length; i++) {
    indexes[i - 1] + 1 === indexes[i]
    ? groups.at(-1)?.push(indexes[i])
    : groups.push([indexes[i]]);
  }

  return groups.map(
    group => group.length === 1 ? group[0] : `${group[0]}~${group.at(-1)}`
  ).join(",");
};

/** @type {Placements} */
const placements = {
  x: [
    [0.5],
    [0.25, 0.75],
    [0, 0.5, 1],
    [0, 0.25, 0.75, 1],
    [0, 0.25, 0.5, 0.75, 1],
  ],
  // y:
  istream: 0,
  ostream: 1,
};

/** @type {5} */
const ioPortsLength = 5; // placements[istream / ostream].length;

/** subTask -> ioPorts
 * @type {(subTask: SubTask, ioPorts: IOPorts) => void} */
const setIOPorts = (subTask, ioPorts) => {
  for (const argName in subTask.args) {
    const { arg, cat } = subTask.args[argName];
    switch (cat) {
      case "istream": ioPorts.istream.push([argName, arg]); break;
      case "ostream": ioPorts.ostream.push([argName, arg]); break;
    }
  }
};

/** ioPorts -> style.ports
 * @type {(ioPorts: IOPorts, style: NodeStyle) => void} */
const setPortsStyle = (ioPorts, style) => {
  /** @type {import("@antv/g6").NodePortStyleProps[]} */
  let ports = [];
  /** @type {["istream", "ostream"]} */
  (["istream", "ostream"]).forEach(stream => {
    const amount = ioPorts[stream].length;
    if (amount <= ioPortsLength) {
      ports = ports.concat(
        ioPorts[stream].map(
          ([name, key], i) => {
            /** @type {import("@antv/g6").Placement} */
            const placement = [placements.x[amount - 1][i], placements[stream]];
            return { name, key, placement };
          }
        ),
      );
    }
  });
  style.ports = ports;
}

/** @type {Readonly<Required<GetGraphDataOptions>>} */
const defaultOptions = {
  separate: false,
  expand: false,
  port: false,
};

/** @type {(json: GraphJSON, options: GetGraphDataOptions) => GraphData} */
export const getGraphData = (json, options = defaultOptions) => {

  /** Rename `port` option to a meaningful one */
  const { separate, port: showPorts } = options;
  /** Convert `expand` option to combo's `collapsed` style */
  const collapsed = !options.expand;

  /** @type {GraphData} */
  const graphData = {
    nodes: [],
    edges: [],
    combos: [],
  };

  const { nodes, edges, combos } = graphData;

  /** Gather upper tasks to determine if there is only 1 upper task
   * @type {Map<string, UpperTask>} */
  const upperTasks = new Map();
  for (const taskName in json.tasks) {
    const task = json.tasks[taskName];
    if (task.level === "upper") {
      upperTasks.set(taskName, task);
    }
  }

  // Use different node color method for differnt amount of upper task
  const colorByTaskLevel = upperTasks.size > 1;

  /** If there is only 1 upper task, color node with 2+ connection
   * @type {(edge: import("@antv/g6").EdgeData) => void} */
  const addEdge = (
    colorByTaskLevel
      ? edge => edges.push(edge)
      : (() => {
        /** Connection counts for nodes, 0: `<= 1`, 1: `> 1`
         * @type {Map<string, 0 | 1>} */
        const counts = new Map();

        return (edge) => {
          [edge.source, edge.target].forEach(id => {
            if (id.startsWith("combo:")) return;
            switch (counts.get(id)) {
              case undefined: // set to 0 if undefined
                counts.set(id, 0);
                break;
              case 0: { // fill and set to 1
                const node = nodes.find(node => node.id === id);
                if (node) node.style = { ...node.style, fill: altNodeColor };
                counts.set(id, 1);
                break;
              }
              case 1: // skip filled node
                break;
            }
          });
          edges.push(edge);
        };
      })()
  );

  const topTaskName = json.top;
  /** @type {Task | undefined} */
  const topTask = json.tasks[topTaskName];
  if (!topTask) {
    return graphData;
  } else if (topTask.level === "lower") {
    nodes.push({
      id: topTaskName,
      data: { task: topTask },
    })
    return graphData;
  }

  combos.push({
    id: getComboId(topTaskName),
    data: topTask,
  });

  // Loop 1: sub-tasks -> combos and nodes
  upperTasks.forEach((upperTask, upperTaskName) => {
    for (const subTaskName in upperTask.tasks) {
      /** Sub-tasks: `${subTaskName}/0`, `${subTaskName}/1`, ... */
      const subTasks = upperTask.tasks[subTaskName];

      /** Sub-tasks' task
       * @type {Task | undefined} */
      const task = json.tasks[subTaskName];

      /** upperTask's combo id */
      let combo = getComboId(upperTaskName);

      /** Node style, not for combo;
       * ports don't gone by itself, set default ports to override existing ones
       * @type {NonNullable<NodeData["style"]>} */
      const style = { ports: [] };

      if (task?.level === "upper") {
        // Upper 1: Add combo for upper task
        const newCombo = {
          id: getComboId(subTaskName),
          combo,
          data: task,
          style: { collapsed },
        };
        // Insert combo after its parent for z-index order
        const i = combos.findIndex(({ id }) => id === combo);
        i !== -1
          ? combos.splice(i + 1, 0, newCombo)
          : combos.push(newCombo);

        // Upper 2: If there is more than 1 upper task, color node by task level
        if (colorByTaskLevel) style.fill = altNodeColor;

        // Put combo's node under it
        // TODO: make it an option?
        // combo = getComboId(subTaskName);
      }

      // Add node for subTask
      if (separate) {
        subTasks.forEach((subTask, i) => {
          if (showPorts) {
          /** @type {IOPorts} */
            const ioPorts = { istream: [], ostream: [] };
            setIOPorts(subTask, ioPorts);
            setPortsStyle(ioPorts, style);
          }

          const id = `${subTaskName}/${i}`;
          nodes.push({ id, combo, style, data: { task, subTask } });
        });
      } else {
        if (showPorts) {
          /** @type {IOPorts} */
          const ioPorts = { istream: [], ostream: [] };
          subTasks.forEach(subTask => setIOPorts(subTask, ioPorts));
          setPortsStyle(ioPorts, style);
        }

        nodes.push({ id: subTaskName, combo, style, data: { task, subTasks } });
      }

    }
  });

  /** fifo groups, for fifos like fifo_xx[0], fifo_xx[1]...
   * @type {Map<string, Set<number>>} */
  const fifoGroups = new Map();

  /** @type {(fifoname: string, source: string, target: string) => boolean} */
  const matchFifoGroup = (fifoName, source, target) => {
    const matchResult = fifoName.match(/^(.*)\[(\d+)\]$/);
    const matched = matchResult !== null;
    if (matched) {
      // Add to fifo group map
      const name = matchResult[1];
      const key = [name, source, target].join("\n");
      const index = Number.parseInt(matchResult[2]);
      addToMappedSet(fifoGroups, key, index);
    }
    return matched;
  }

  /** get port's key for missing produced_by or consumed_by
   * @type {(node: import("@antv/g6").NodeData | undefined, fifoName: string) => string} */
  const getPortKey = (node, fifoName) => node?.style?.ports
    ?.find(port => "name" in port && port.name === fifoName)?.key ?? fifoName;

  // Loop 2: fifo -> edges
  upperTasks.forEach((upperTask, upperTaskName) => {

    fifoGroups.clear();

    for (const fifoName in upperTask.fifos) {
      const fifo = upperTask.fifos[fifoName];
      const id = `${upperTaskName}/${fifoName}`;

      /**
       * @typedef {() => import("@antv/g6/lib/spec/element/edge").EdgeStyle} GetStyle
       * @type {(source: string, target: string, id: string, getStyle: GetStyle) => void} */
      const parseFifo = (source, target, id, getStyle) => void (
        matchFifoGroup(fifoName, source, target) ||
        addEdge({ source, target, id, style: getStyle(), data: fifo })
      );

      if (fifo.produced_by && fifo.consumed_by) {
        /** @type {(by: [string, number]) => string} */
        const getSubTask = by => separate ? by.join("/") : by[0];
        const source = getSubTask(fifo.produced_by);
        const target = getSubTask(fifo.consumed_by);

        const style = { sourcePort: fifoName, targetPort: fifoName };
        parseFifo(source, target, id, () => style);
      } else if (!fifo.produced_by && fifo.consumed_by) {

        // produced_by is missing
        /** @type {(node: import("@antv/g6").NodeData | undefined) => GetStyle} */
        const getStyle = node => () => {
          const sourcePort = getPortKey(node, fifoName);
          return { sourcePort, targetPort: fifoName, stroke: altEdgeColor };
        };

        if (separate) {
          const target = fifo.consumed_by.join("/");
          nodes
            .filter(node => node.id.startsWith(`${upperTaskName}/`))
            .forEach(node => {
              // avoid duplicate id
              const edgeId = `${id}${node.id.slice(node.id.indexOf("/"))}`;
              parseFifo(node.id, target, edgeId, getStyle(node));
            });
        } else {
          const node = nodes.find(node => node.id === upperTaskName);
          parseFifo(upperTaskName, fifo.consumed_by[0], id, getStyle(node));
        }

      } else if (fifo.produced_by && !fifo.consumed_by) {

        // consumed_by is missing
        /** @type {(node: import("@antv/g6").NodeData | undefined) => GetStyle} */
        const getStyle = node => () => {
          const targetPort = getPortKey(node, fifoName);
          return { sourcePort: fifoName, targetPort, stroke: altEdgeColor };
        };

        if (separate) {
          const source = fifo.produced_by.join("/");
          nodes
            .filter(node => node.id.startsWith(`${upperTaskName}/`))
            .forEach(node => {
              const edgeId = `${id}${node.id.slice(node.id.indexOf("/"))}`;
              parseFifo(source, node.id, edgeId, getStyle(node));
            });
        } else {
          const node = nodes.find(node => node.id === upperTaskName);
          parseFifo( fifo.produced_by[0], upperTaskName, id, getStyle(node));
        }

      } else {
        console.warn(
          `fifo ${fifoName} without produced_by and consumed_by in ${upperTaskName}:`,
          upperTask,
        )
        continue;
      }

    }

    // Add fifo groups
    fifoGroups.forEach((indexes, key) => {
      const [name, source, target] = key.split("\n");
      const indexRange = getIndexRange([...indexes.values()]);
      addEdge({ source, target, id: `${upperTaskName}/${name}[${indexRange}]` });
    });

  });

  return graphData;

};
