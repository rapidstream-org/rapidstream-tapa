/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

/** @type {<K, V>(map: Map<K, Set<V>>, key: K, value: V) => void} */
const addToMappedSet = (map, key, value) => {
  const set = map.get(key);
  set ? set.add(value) : map.set(key, new Set([value]));
};

/** @type {(json: GraphJSON) => Required<import("@antv/g6").GraphData>} */
export const getGraphData = (json) => {

  /** @type {Required<import("@antv/g6").GraphData>} */
  const graphData = {
    nodes: [],
    edges: [],
    combos: [],
  };

  /** @type {Map<string, Set<number>>} */
  const nodes = new Map();

  for (const taskName in json.tasks) {

    const task = json.tasks[taskName];

    // skip lower tasks
    if (task.level !== "upper") continue;

    /** fifo groups for fifos like fifo_x_xx[0], fifo_x_xx[1]...
     * @type {Map<string, Set<number> & {source: string, target: string}>} */
    const fifoGroups = new Map();

    for (const fifoName in task.fifos) {
      const [source, sourceId] = task.fifos[fifoName].produced_by;
      const [target, targetId] = task.fifos[fifoName].consumed_by;
      // console.log(taskName, fifoName, source, target);

      addToMappedSet(nodes, source, sourceId);
      addToMappedSet(nodes, target, targetId);

      /** Match fifo groups */
      const matchResult = fifoName.match(/^(.*)\[(\d+)\]$/);
      if (matchResult === null) {
        // Not fifo groups, add edge
        graphData.edges.push({ source, target, id: fifoName });
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
        console.warn(`fifo group: indexes are not continuous at ${taskName}'s ${name}`);
      }
      const idWithIndexRange = `${name}[${indexArr[0]}~${indexArr.at(-1)}]`;
      graphData.edges.push({ source, target, id: idWithIndexRange });
    })

  }

  nodes.forEach(
    (_ids, name) => graphData.nodes.push(
      { id: name },
    ),
  );

  return graphData;

};
