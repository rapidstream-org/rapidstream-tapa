/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { getComboId } from "./helper.js";

/** Color for node with more than 1 connection */
const majorNodeColor = "#0F5132"; // Bootstrap $green-700

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

/** TODO: Better placement per port amount
 * - 1 port:           0.5
 * - 2 ports:    0.25,      0.75
 * - 3 ports: 0,       0.5,       1
 * - 4 ports: 0, 0.25,      0.75, 1
 * - 5 ports: 0, 0.25, 0.5, 0.75, 1
 */
/** @type {Placements} */
const placements = {
  istream: ["top",    [0.25, 0], [0.75, 0], [0, 0], [1, 0]],
  ostream: ["bottom", [0.25, 1], [0.75, 1], [0, 1], [1, 1]],
};

/** @type {5} */
const ioPortsLength = 5; // placements[istream / ostream].length;

/** subTask -> ioPorts
 * @type {(subTask: SubTask, ioPorts: IOPorts) => void} */
const setIOPorts = (subTask, ioPorts) => {
  for (const argName in subTask.args) {
    const { arg, cat } = subTask.args[argName];
    switch (cat) {
      case "istream": ioPorts.istream.push(arg); break;
      case "ostream": ioPorts.ostream.push(arg); break;
    }
  }
};

/** ioPorts -> style.ports
 * @typedef {import("@antv/g6/lib/spec/element/node.js").NodeStyle} NodeStyle
 * @type {(ioPorts: IOPorts, style: NodeStyle) => void} */
const setPortsToStyle = (ioPorts, style) => {
  /** @type {import("@antv/g6").NodePortStyleProps[]} */
  let ports = [];
  /** @type {["istream", "ostream"]} */
  (["istream", "ostream"]).forEach(stream => {
    if (ioPorts[stream].length <= ioPortsLength) {
      ports = ports.concat(
        ports,
        ioPorts[stream].map(
          (key, j) => ({ key, placement: placements[stream][j] })
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
                if (node) node.style = { ...node.style, fill: majorNodeColor };
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
        if (colorByTaskLevel) style.fill = majorNodeColor;

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
            setPortsToStyle(ioPorts, style);
          }

          const id = `${subTaskName}/${i}`;
          nodes.push({ id, combo, style, data: { task, subTask } });
        })
      } else {
        if (showPorts) {
          /** @type {IOPorts} */
          const ioPorts = { istream: [], ostream: [] };
          subTasks.forEach(subTask => setIOPorts(subTask, ioPorts));
          setPortsToStyle(ioPorts, style);
        }

        nodes.push({ id: subTaskName, combo, style, data: { task, subTasks } });
      }

    }
  });

  // Loop 2: fifo -> edges
  upperTasks.forEach((upperTask, upperTaskName) => {

    /** fifo groups, for fifos like fifo_xx[0], fifo_xx[1]...
     * @type {Map<string, Set<number>>} */
    const fifoGroups = new Map();

    for (const fifoName in upperTask.fifos) {
      const fifo = upperTask.fifos[fifoName];
      if (!fifo.produced_by && !fifo.consumed_by) {
        console.warn(
          `fifo ${fifoName} without produced_by and consumed_by in ${upperTaskName}:`,
          upperTask,
        )
        continue;
      }

    /** get source / target sub-task with fallback to the upper task
     *
     * TODO: connect to upperTaskName/1 upperTaskName/2 if they exist
     * @type {(by: [string, number] | undefined) => string}
     * @param by fifo.produced_by / fifo.consumed_by */
     const getSubTask = by => flat
      ? by?.join("/") ?? `${upperTaskName}/0`
      : by?.[0] ?? upperTaskName;

      const source = getSubTask(fifo.produced_by);
      const target = getSubTask(fifo.consumed_by);

      // Match fifo groups
      const matchResult = fifoName.match(/^(.*)\[(\d+)\]$/);
      if (matchResult === null) {
        // Not fifo groups, add edge directly
        const id = `${upperTaskName}/${fifoName}`;
        const style = { sourcePort: fifoName, targetPort: fifoName };
        addEdge({ source, target, id, style, data: fifo });
      } else {
        // Add to fifo group map
        const name = matchResult[1];
        const key = [name, source, target].join("\n");
        const index = Number.parseInt(matchResult[2]);
        addToMappedSet(fifoGroups, key, index);
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
