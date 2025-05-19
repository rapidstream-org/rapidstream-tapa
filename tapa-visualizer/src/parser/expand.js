/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { getComboName } from "../helper.js";

/** @type {(graphData: GraphData) => void} */
export const expandSubTask = (graphData) => {

  /** insert index to the node, change the number in the toSpliced()
   * to change its position
   * Example: NodeName/1 -> NodeName/1/0, NodeName/1/1, ...
   * @type {(id: string, i: string) => string} */
  const insertIndex = (id, i) => id.split("/").toSpliced(2, 0, i).join("/");

  /** Combos involved in the expanding, saved for set combo.combo later
   * @type {import("@antv/g6").ComboData[]} */
  const expandedCombos = [];
  /** @type {string[]} */
  const expandedComboIds = [];

  for (let i = 1; i < graphData.combos.length; i++) {
    const combo = graphData.combos[i];

    // Check if combo need expand: if it has multiple nodes (sub-tasks)
    const comboNodes = graphData.nodes.filter(
      node => node.id.startsWith(`${getComboName(combo.id)}/`)
    );
    if (comboNodes.length <= 1) continue;

    expandedCombos.push(combo);
    expandedComboIds.push(combo.id);

    // Combo's children
    const children = graphData.nodes.filter(node => node.combo === combo.id);

    // Add expanded combos & nodes, using combo & its children as template
    for (let j = 1; j < comboNodes.length; j++) {
      const comboId = `${combo.id}/${j}`;
      const newCombo = { ...combo, id: comboId };
      expandedCombos.push(newCombo);
      graphData.combos.push(newCombo);

      graphData.nodes.push(
        ...children.map(node => {
          const nodeId = insertIndex(node.id, j.toString());
          return { ...node, id: nodeId, combo: comboId };
        }),
      );
    }

    // Update combo & its original children
    combo.id += "/0";
    children.forEach(node => {
      node.id = insertIndex(node.id, "0");
      node.combo = combo.id;
    });
  }

  // set combo.combo for the involved combos
  expandedCombos.forEach(combo => {
    if (combo.combo && expandedComboIds.includes(combo.combo)) {
      combo.combo += `/${combo.id.split("/").at(-1)}`;
    }
  });

};
