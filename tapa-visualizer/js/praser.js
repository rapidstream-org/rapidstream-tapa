/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { getComboName } from "./helper.js";

/** Color for node with more than 1 connection */
const majorNodeColor = "#0F5132";

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

/** @type {(json: GraphJSON, flat: boolean) => GraphData} */
export const getGraphData = (json, flat = false) => {

  /** @type {GraphData} */
  const graphData = {
    nodes: [],
    edges: [],
    combos: [],
  };

  /** Connection counts for nodes
   * @type {Map<string, 0 | 1>} */
  const counts = new Map();

  /** @type {(edge: import("@antv/g6").EdgeData) => void} */
  const addEdge = (edge) => {
    [edge.source, edge.target].forEach(id => {
      if (id.startsWith("combo:")) return;
      switch (counts.get(id)) {
        case undefined: // set to false if undefined
          counts.set(id, 0);
          break;
        case 0: { // fill and set to true
          const node = graphData.nodes.find(node => node.id === id);
          if (node) node.style = { ...node.style, fill: majorNodeColor };
          counts.set(id, 1);
          break;
        }
        case 1: // skip filled node
          break;
      }
    });
    graphData.edges.push(edge);
  }

  /** Gather upper tasks to determine if there is only 1 upper task
   * @type {Map<string, UpperTask>} */
  const upperTasks = new Map();
  for (const taskName in json.tasks) {
    const task = json.tasks[taskName];
    if (task.level === "upper") {
      upperTasks.set(taskName, task);
    }
  }

  upperTasks.forEach((upperTask, taskName) => {

    // Add upper task as combo
    const combo = `combo:${taskName}`;
    graphData.combos.push({
      id: combo,
      data: upperTask,
      // Use rect for top task
      type: taskName === json.top ? "rect" : "circle",
      // Use lighter background if there is only 1 upper task
      style: upperTasks.size > 1 ? {} : { fillOpacity: 0.05 }
    });

    // Add sub-task as node
    for (const subTaskName in upperTask.tasks) {
      const subTasks = upperTask.tasks[subTaskName];

      /** @type {UpperTask | LowerTask | undefined} */
      const task = json.tasks[subTaskName];

      if (flat) {
        subTasks.forEach(
          (subTask, i) => graphData.nodes.push({
            id: `${subTaskName}/${i}`,
            combo,
            data: { task, subTask },
          })
        );
      } else {
        graphData.nodes.push({
          id: subTaskName,
          combo,
          data: { task, subTasks },
        });
      }
    }

    /** fifo groups for fifos like fifo_x_xx[0], fifo_x_xx[1]...
     * @type {Map<string, Set<number>>} */
    const fifoGroups = new Map();

    for (const fifoName in upperTask.fifos) {
      const fifo = upperTask.fifos[fifoName];
      if (!fifo.produced_by || !fifo.consumed_by) {
        console.warn(
          `Missing produced_by / consumed_by in ${taskName}'s ${fifoName}:`,
          fifo,
        );
        continue;
      }

      /** @type {(by: [string, number]) => string} */
      const getSubTask = (
        flat
        ? by => by.join("/")
        : by => by[0]
      );

      const source = getSubTask(fifo.produced_by);
      const target = getSubTask(fifo.consumed_by);

      // Match fifo groups
      const matchResult = fifoName.match(/^(.*)\[(\d+)\]$/);
      if (matchResult === null) {
        // Not fifo groups, add edge directly
        addEdge({ source, target, id: `${taskName}/${fifoName}`, data: fifo });
      } else {
        // add fifo group index
        const name = matchResult[1];
        const key = [name, source, target].join("\n");
        const index = Number.parseInt(matchResult[2]);
        addToMappedSet(fifoGroups, key, index);
      }
    }

    fifoGroups.forEach((indexes, key) => {
      const [name, source, target] = key.split("\n");
      const indexRange = getIndexRange([...indexes.values()]);
      addEdge({ source, target, id: `${taskName}/${name}[${indexRange}]` });
    });

  });

  // add combo-to-node connections
  // TODO: replace this with expand / collapse
  graphData.combos.forEach(combo => {
    if (combo.type === "circle") {
      const node = graphData.nodes.find(
        ({id}) => id.split("/")[0] === getComboName(combo.id)
      );
      if (node) {
        const edgeId = `combo-to-node/${node.id}`;
        addEdge({ source: combo.id, target: node.id, id: edgeId,  });
      }
    }
  })

  return graphData;

};
