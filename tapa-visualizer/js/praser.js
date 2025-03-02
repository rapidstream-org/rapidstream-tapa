/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { getComboId } from "./helper.js";

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

/** @type {import("@antv/g6").Placement[]} */
const placements = [
  [0, 0], [0.25, 0], "top",    [0.75, 0], [1, 0], "right",
  [0, 1], [0.75, 1], "bottom", [0.25, 1], [1, 1], "left",
];

/** @type {12} */
const maxPortsLength = 12; // placements.length;

/** @type {(json: GraphJSON, flat: GetGraphDataOptions) => GraphData} */
export const getGraphData = (json, {
  flat = false,
  // TODO: expand toggle, port toggle
  expand = false,
  port = false,
}) => {

  /** @type {GraphData} */
  const graphData = {
    nodes: [],
    edges: [],
    combos: [],
  };

  const { nodes, edges, combos } = graphData;

  /** Convert `expand` option to combo's `collapsed` style */
  const collapsed = !expand;

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
    type: "rect",
    data: topTask,
  });

  upperTasks.forEach((upperTask, upperTaskName) => {

    // Add sub-tasks
    for (const subTaskName in upperTask.tasks) {
      /** Sub-tasks, `${subTaskName}/0`, `${subTaskName}/1`, ... */
      const subTasks = upperTask.tasks[subTaskName];

      /** Sub-tasks' task
       * @type {Task | undefined} */
      const task = json.tasks[subTaskName];

      /** upperTask's combo id */
      let combo = getComboId(upperTaskName);

      /** Node style, not for combo
       * @type {NonNullable<NodeData["style"]>} */
      const style = {};

      // Add combo for upper task
      if (task?.level === "upper") {
        const newComboId = getComboId(subTaskName);
        const newCombo = {
          id: newComboId,
          combo,
          type: "circle",
          data: task,
          style: { collapsed },
        };

        // Insert combo after its parent for z-index order
        const i = combos.findIndex(({ id }) => id === combo);
        i !== -1
          ? combos.splice(i + 1, 0, newCombo)
          : combos.push(newCombo);

        // Put combo's node under it
        combo = newComboId;

        // Color node by task level if there is more than 1 upper task
        if (colorByTaskLevel) style.fill = majorNodeColor;
      }

      // Add node for subTask
      if (flat) {
        subTasks.forEach(
          (subTask, i) => {
            const args = Object.values(subTask.args);
            const ports = port && args.length <= maxPortsLength
              ? args.map(({ arg }, i) => ({ key: arg, placement: placements[i] }))
              : undefined;

            nodes.push({
              id: `${subTaskName}/${i}`,
              combo,
              style: { ...style, ports },
              data: { task, subTask },
            });
          }
        );
      } else {
        const args = new Set(
          subTasks
            .flatMap(subTask => Object.values(subTask.args))
            .map(({ arg }) => arg)
        );
        const ports = port && args.size <= maxPortsLength
        ? [...args].map((key, i) => ({ key, placement: placements[i] }))
        : undefined;

        nodes.push({
          id: subTaskName,
          combo,
          style: { ...style, ports },
          data: { task, subTasks },
        });
      }

    }

    /** fifo groups, for fifos like fifo_x_xx[0], fifo_x_xx[1]...
     * @type {Map<string, Set<number>>} */
    const fifoGroups = new Map();

    /** @type {string[]} */
    let needUnknownNode = [];

    /**
     * @type {(by: [string, number] | undefined) => string}
     * @param by fifo.produced_by / fifo.consumed_by */
    const getSubTaskWithFallback = by => {
      if (by) {
        return flat ? by.join("/") : by[0];
      } else {
        needUnknownNode.push(upperTaskName);
        return `<unknown>@${upperTaskName}`;
      }
    };

    for (const fifoName in upperTask.fifos) {
      const fifo = upperTask.fifos[fifoName];

      const source = getSubTaskWithFallback(fifo.produced_by);
      const target = getSubTaskWithFallback(fifo.consumed_by);

      // Match fifo groups
      const matchResult = fifoName.match(/^(.*)\[(\d+)\]$/);
      if (matchResult === null) {
        // Not fifo groups, add edge directly
        const style = { sourcePort: fifoName, targetPort: fifoName };
        addEdge({ source, target, id: `${upperTaskName}/${fifoName}`, style, data: fifo });
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
      addEdge({ source, target, id: `${upperTaskName}/${name}[${indexRange}]` });
    });

    needUnknownNode.forEach(
      taskName => nodes.push({
        id: `<unknown>@${taskName}`,
        combo: getComboId(taskName),
        style: { fill: "gray" },
      })
    );

  });

  return graphData;

};
