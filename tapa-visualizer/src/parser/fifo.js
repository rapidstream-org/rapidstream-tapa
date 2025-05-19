/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { color } from "../graph-config.js";

/** Color for edge connecting parent and children */
const altEdgeColor = color.edgeB;

/** fifo groups, for fifos like fifo_xx[0], fifo_xx[1]...
 * @type {Map<string, Set<number>>} */
const fifoGroups = new Map();

/** @type {(fifoname: string, source: string, target: string, id: string) => boolean} */
const matchFifoGroup = (fifoName, source, target, id) => {
  const matchResult = fifoName.match(/^(.*)\[(\d+)\]$/);
  const matched = matchResult !== null;
  if (matched) {
    // Add to fifo group map
    const name = matchResult[1];
    const idSegments = id.replace(`/${fifoName}`, "");
    const key = [name, source, target, idSegments].join("\n");
    const index = Number.parseInt(matchResult[2]);

    const set = fifoGroups.get(key);
    set ? set.add(index) : fifoGroups.set(key, new Set([index]));
  }
  return matched;
}

/** improve the formating of fifo groups: [1,2,3,5] -> "1~3,5"
 * @type {(indexArr: number[]) => string} */
const getIndexRange = indexes => {
  // Only 1 index, just return it as string
  if (indexes.length === 1) return `${indexes[0]}`;

  // More than 1 index, sort & parse to "a~b,c,d~e"
  indexes.sort((a, b) => a - b);

  // Get continuous groups of indexs
  const groups = [[indexes[0]]];
  for (let i = 1; i < indexes.length; i++) {
    indexes[i - 1] + 1 === indexes[i]
    ? groups.at(-1)?.push(indexes[i])
    : groups.push([indexes[i]]);
  }

  // Parse groups of indexs back into a string
  return groups.map(
    group => group.length === 1 ? group[0] : `${group[0]}~${group.at(-1)}`
  ).join(",");
};

/** get port's key for missing produced_by or consumed_by
 * @type {(node: import("@antv/g6").NodeData | undefined, fifoName: string) => string} */
const getPortKey = (node, fifoName) => node?.style?.ports
?.find(port => "name" in port && port.name === fifoName)?.key ?? fifoName;

/**
 * @param {UpperTask} task
 * @param {string} taskName
 * @param {Grouping} grouping
 * @param {GraphData["nodes"]} nodes
 * @param {(edge: import("@antv/g6").EdgeData) => void} addEdge
 **/
export const parseFifo = (task, taskName, grouping, nodes, addEdge) => {

  fifoGroups.clear();

  for (const fifoName in task.fifos) {
    const fifo = task.fifos[fifoName];
    const id = `${taskName}/${fifoName}`;

    /** Match fifo group, if matched add to fifo group, else add edge directly
     * @typedef {() => import("@antv/g6/lib/spec/element/edge").EdgeStyle} GetStyle
     * @type {(source: string, target: string, id: string, getStyle: GetStyle) => void} */
    const addFifo = (source, target, id, getStyle) => void (
      matchFifoGroup(fifoName, source, target, id) ||
      addEdge({ source, target, id, style: getStyle(), data: { fifo } })
    );

    if (fifo.produced_by && fifo.consumed_by) {
      // normal fifo connection, both side exists

      /** @type {(by: [string, number]) => string} */
      const getSubTask = by => grouping !== "merge" ? by.join("/") : by[0];
      const source = getSubTask(fifo.produced_by);
      const target = getSubTask(fifo.consumed_by);

      const style = { sourcePort: fifoName, targetPort: fifoName };
      addFifo(source, target, id, () => style);

    } else if (!fifo.produced_by && fifo.consumed_by) {
      // produced_by is missing

      /** @type {(node: import("@antv/g6").NodeData | undefined) => GetStyle} */
      const getStyle = node => () => {
        const sourcePort = getPortKey(node, fifoName);
        return { sourcePort, targetPort: fifoName, stroke: altEdgeColor };
      };

      switch (grouping) {
        case "merge": {
          const node = nodes.find(node => node.id === taskName);
          addFifo(taskName, fifo.consumed_by[0], id, getStyle(node));
          break;
        }
        case "separate": {
          const target = fifo.consumed_by.join("/");
          nodes
            .filter(node => node.id.startsWith(`${taskName}/`))
            .forEach(node => {
              // avoid duplicate id
              const edgeId = `${id}${node.id.slice(node.id.indexOf("/"))}`;
              addFifo(node.id, target, edgeId, getStyle(node));
            });
          break;
        }
        case "expand": {
          const targetId = fifo.consumed_by.join("/");
          const sources = nodes.filter(({id}) => id.startsWith(`${taskName}/`));
          const targets = nodes.filter(
            ({id}) => id === targetId || id.startsWith(`${targetId}/`),
          );
          const len = Math.min(sources.length, targets.length);
          for (let i = 0; i < len; i++) {
            const source = sources[i];
            addFifo(source.id, targets[i].id, `${id}/${i}`, getStyle(source));
          }
          break;
        }
      }

    } else if (fifo.produced_by && !fifo.consumed_by) {
      // consumed_by is missing

      /** @type {(node: import("@antv/g6").NodeData | undefined) => GetStyle} */
      const getStyle = node => () => {
        const targetPort = getPortKey(node, fifoName);
        return { sourcePort: fifoName, targetPort, stroke: altEdgeColor };
      };

      switch (grouping) {
        case "merge": {
          const node = nodes.find(node => node.id === taskName);
          addFifo(fifo.produced_by[0], taskName, id, getStyle(node));
          break;
        }
        case "separate": {
          const source = fifo.produced_by.join("/");
          nodes
            .filter(node => node.id.startsWith(`${taskName}/`))
            .forEach(node => {
              const edgeId = `${id}${node.id.slice(node.id.indexOf("/"))}`;
              addFifo(source, node.id, edgeId, getStyle(node));
            });
          break;
        }
        case "expand": {
          const sourceId = fifo.produced_by.join("/");
          const sources = nodes.filter(
            ({id}) => id === sourceId || id.startsWith(`${sourceId}/`),
          );
          const targets = nodes.filter(({id}) => id.startsWith(`${taskName}/`));
          const len = Math.min(sources.length, targets.length);
          for (let i = 0; i < len; i++) {
            const target = targets[i];
            addFifo(sources[i].id, target.id, `${id}/${i}`, getStyle(target));
          }
          break;
        }
      }

    } else {
      console.warn(
        `fifo ${fifoName} without produced_by and consumed_by in ${taskName}:`,
        task,
      )
      continue;
    }

  }

  // Add edges for fifo groups
  fifoGroups.forEach((indexes, key) => {
    const [name, source, target, idSegments] = key.split("\n");

    const segments = idSegments.split("/");
    const prefix = segments.shift();
    const suffix = segments.length > 0
      ? segments.map(segment => `/${segment}`).join("")
      : "";

    const indexRange = getIndexRange([...indexes.values()]);
    const id = `${prefix}/${name}[${indexRange}]${suffix}`;
    addEdge({ source, target, id });
  });

}
