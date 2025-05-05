/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { getComboId, getComboName } from "./helper.js";
import { setIOPorts, setPortsStyle } from "./parser/ports.js";
import { color } from "./graph-config.js";
import { parseFifo } from "./parser/fifo.js";

/** Color for node with more than 1 connection */
const altNodeColor = color.nodeB;

/** @type {Readonly<Required<GetGraphDataOptions>>} */
const defaultOptions = {
  grouping: "merge",
  expand: false,
  port: false,
};

/** @type {(json: GraphJSON, options: Required<GetGraphDataOptions>) => GraphData} */
export const getGraphData = (json, options = defaultOptions) => {

  /** Rename `port` option to a meaningful one */
  const { grouping, port: showPorts } = options;
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
      if (grouping !== "merge") {
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

  // Between Loop 1 and Loop 2: check & expand sub-task
  if (grouping === "expand" && graphData.combos.length > 1) {

    /** @type {(id: string, i: string) => string} */
    const insertIndex = (id, i) => id.split("/").toSpliced(2, 0, i).join("/");

    for (let i = 1; i < graphData.combos.length; i++) {
      const combo = graphData.combos[i];
      const name = getComboName(combo.id);

      // Check if combo need expand: if it has multiple nodes (sub-tasks)
      const comboNodes = nodes.filter(node => node.id.startsWith(`${name}/`));
      if (comboNodes.length <= 1) continue;

      // Combo's children
      const children = graphData.nodes.filter(node => node.combo === combo.id);

      // Add expanded nodes, using combo's children as template
      for (let j = 1; j < comboNodes.length; j++) {
        graphData.nodes.push(
          ...children.map(node => ({
            ...node,
            id: insertIndex(node.id, j.toString())
          }))
        );
      }

      // Update combo's children
      children.forEach(node => node.id = insertIndex(node.id, "0"));
    }

  }

  // Loop 2: fifo -> edges
  upperTasks.forEach(
    (upperTask, upperTaskName) =>
      parseFifo(upperTask, upperTaskName, grouping, nodes, addEdge),
  );

  return graphData;

};
