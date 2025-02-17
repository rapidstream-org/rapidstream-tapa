/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";


/** @type {<K, V>(map: Map<K, Set<V>>, key: K, value: V) => void} */
const addToMappedSet = (map, key, value) => {
  const set = map.get(key);
  set ? set.add(value) : map.set(key, new Set([value]));
};

/** @type {(json: GraphJSON, flat: boolean) => Required<import("@antv/g6").GraphData>} */
export const getGraphData = (json, flat = false) => {

  /** @type {Required<import("@antv/g6").GraphData>} */
  const graphData = {
    nodes: [],
    edges: [],
    combos: [],
  };

  for (const taskName in json.tasks) {

    const task = json.tasks[taskName];

    // Skip lower tasks
    if (task.level !== "upper") continue;

    // Add upper task as combo
    const combo = `combo:${taskName}`;
    graphData.combos.push({
      id: combo,
      data: task,
      type: taskName === json.top ? "rect" : "circle"
    });

    // Add sub task as node
    for (const subTaskName in task.tasks) {
      const subTasks = task.tasks[subTaskName];
      if (flat) {
        subTasks.forEach(
          (subTask, i) => graphData.nodes.push({
            id: `${subTaskName}/${i}`,
            combo,
            data: subTask,
          })
        );
      } else {
        graphData.nodes.push({
          id: subTaskName,
          combo,
          data: { subTasks },
        });
      }
    }

    /** fifo groups for fifos like fifo_x_xx[0], fifo_x_xx[1]...
     * @type {Map<string, Set<number> & {source: string, target: string}>} */
    const fifoGroups = new Map();

    for (const fifoName in task.fifos) {
      const fifo = task.fifos[fifoName];
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

      // console.log(taskName, fifoName, source, target);

      /** Match fifo groups */
      const matchResult = fifoName.match(/^(.*)\[(\d+)\]$/);
      if (matchResult === null) {
        // Not fifo groups, add edge
        graphData.edges.push(
          { source, target, id: `${taskName}/${fifoName}`, data: fifo }
        );
      } else {
        // add fifo group index
        const name = matchResult[1];
        const key = [name, source, target].join("\n");
        const index = Number.parseInt(matchResult[2]);
        addToMappedSet(fifoGroups, key, index);
      }
    }

    fifoGroups.forEach((indexs, key) => {
      const [name, source, target] = key.split("\n");
      const indexArr = [...indexs.values()].sort((a, b) => a - b);
      if (indexArr.some((index, i) => index !== i)) {
        console.warn(
          `fifo group: indexes are not continuous at ${taskName}'s ${name}`
        );
      }
      const idWithIndexRange = `${name}[${indexArr[0]}~${indexArr.at(-1)}]`;
      graphData.edges.push(
        { source, target, id: `${taskName}/${idWithIndexRange}` }
      );
    })

  }

  graphData.combos.forEach(combo => {
    if (combo.type === "circle") {
      const node = graphData.nodes.find(
        ({id}) => id.split("/")[0] === getComboName(combo.id)
      );
      if (node) graphData.edges.push(
        { source: combo.id, target: node.id, id: `combo-to-node/${node.id}` }
      );
    }
  })

  return graphData;

};
