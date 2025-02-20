/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { $, $text, append, getComboName } from "./helper.js";

// sidebar content container elements
const sidebarContainers = [
  "instance",
  "neighbors",
  "connections",
].map(name => {
  const element = document.querySelector(`.sidebar-content-${name}`);
  if (element) {
    return element;
  } else {
    throw new TypeError(`Element .sidebar-content-${name} not found!`);
  }
});

const [
  instance,
  neighbors,
  connections,
] = sidebarContainers;

export const resetInstance = (text = "Please select an item.") =>
  instance.replaceChildren($text("p", text));

export const resetSidebar = (instanceText = "Please select an item.") => [
  instanceText,
  "Please select a node.",
  "Please select a node.",
].forEach(
  (text, i) => sidebarContainers[i].replaceChildren($text("p", text)),
);

// object to element parsers

/** @type {(args: [string, { arg: string, cat: string }][]) => HTMLElement} */
const parseArgs = args => append(
  $("dd"), append(
    $("ul"), ...args.map(
      ([name, { arg, cat }]) =>
        $text("li", `${name}: ${arg} (${cat})`)
    )
  )
);

/** @type {(ports: Port[]) => HTMLTableElement} */
const parsePorts = ports => append(
  $("table", { className: "upperTask-ports" }),
  append(
    $("tr"),
    $text("th", "Name"),
    $text("th", "Category"),
    $text("th", "Type"),
    $text("th", "Width"),
  ),
  ...ports.map(
    ({name, cat, type, width}) => append(
      $("tr"),
      ...[name, cat, type, width].map(
        value => $text("td", value),
      ),
    ),
  ),
);

/** @type {(code: string) => HTMLButtonElement} */
const getCopyButton = (code) => {
  const button = $text("button", "Copy Code");
  button.addEventListener("click", () => void navigator.clipboard.writeText(code));
  return button;
};

// Details

/** Get an `<dl>` element containing:
 * Instance Name, Upper Task, Sub-Task(s)
 * @type {(node: NodeData) => HTMLDListElement} */
const getNodeInfo = node => {

  const dl = append(
    $("dl"),
    $text("dt", "Instance Name"),
    $text("dd", node.id),
    $text("dt", "Upper Task"),
    $text("dd", getComboName(node.combo ?? "<none>")),
  );

  /** Append info for 1 sub-task, indexed or not indexed
   * @param {HTMLDListElement} dl
   * @param {SubTask} subTask
   * @param {number} [i] */
  const appendSubTask = (dl, { args, step }, i) => {
    const argsArr = Object.entries(args);
    if (typeof i === "number") {
      dl.append($("dt", {
        textContent: `Sub-Task ${i}`,
        style: "padding: .25rem 0 .1rem; border-top: 1px solid var(--border);",
      }));
    }
    dl.append(
      $text("dt", "Arguments"),
      argsArr.length > 0
        ? parseArgs(argsArr)
        : $text("dd", "<none>"),
      $text("dt", "Step"),
      $text("dd", step),
    );
  }

  const { data } = node;
  if ("subTask" in data) {
    appendSubTask(dl, data.subTask);
  } else if ("subTasks" in data && Array.isArray(data.subTasks)) {
    data.subTasks.forEach(
      (subTask, i) => appendSubTask(dl, subTask, i)
    );
  } else {
    console.warn("Selected node is missing data!", node)
  }

  const { task } = data;
  if (task) {
    dl.append(
      $text("dt", "Task Level"),
      $text("dd", task.level),
      $text("dt", "Build Target"),
      $text("dd", task.target),
      $text("dt", "Vendor"),
      $text("dd", task.vendor),
      $text("dt", "Code"),
      append($("dd"), getCopyButton(task.code)),
    )
  }

  return dl;

};

/** Update sidebar for selected node
 *  @param {NodeData} node
 *  @param {GraphData} graphData */
export const updateSidebarForNode = (node, graphData) => {

  // Instance
  const nodeInfo = getNodeInfo(node);
  instance.replaceChildren(nodeInfo);

  // Neighbors & Connections

  /** @type {import("@antv/g6").EdgeData[]} */
  const sources = [];
  /** @type {import("@antv/g6").EdgeData[]} */
  const targets = [];
  graphData.edges.forEach(edge => {
    edge.source === node.id && sources.push(edge);
    edge.target === node.id && targets.push(edge);
  });

  /** @type {Set<string>} */
  const neighborIds = new Set();
  sources.forEach(edge => neighborIds.add(edge.target));
  targets.forEach(edge => neighborIds.add(edge.source));

  /** @type {(elements: (Node | string)[]) => HTMLUListElement} */
  const ul = elements => {
    const ul = $("ul", { style: "font-family: monospace;" });
    ul.append(...elements);
    return ul;
  };

  neighbors.replaceChildren(
    neighborIds.size > 0
    ? ul([...neighborIds.values().map(id => $("li", { textContent: id }))])
    : $("p", { textContent: `${node.id} has no neighbors.` }),
  );

  connections.replaceChildren(
    $("p", { textContent: "Sources" }),
    ul(sources.map(edge => $("li", { textContent: `${edge.id} -> ${edge.target}` }))),
    $("p", { textContent: "Targets" }),
    ul(targets.map(edge => $("li", { textContent: `${edge.id} <- ${edge.source}` }))),
  );


};


/** Get an `<dl>` element containing:
 * Instance Name, Upper Task, Sub-Task(s)
 * @type {(node: ComboData) => HTMLDListElement} */
const getComboInfo = (combo) => append(
  $("dl"),
  $text("dt", "Instance Name"),
  $text("dd", combo.id),
  $text("dt", "Task Level"),
  $text("dd", combo.data.level),
  $text("dt", "Build Target"),
  $text("dd", combo.data.target),
  $text("dt", "Vendor"),
  $text("dd", combo.data.vendor),

  // Parse ports to table
  $text("dt", "Ports"),
  combo.data.ports.length === 0
    ? $text("dd", "none")
    : parsePorts(combo.data.ports),

  $text("dt", "Tasks"),
  $text("dd", "TODO"),
  $text("dt", "Fifos"),
  $text("dd", "TODO"),

  $text("dt", "Code"),
  append($("dd"), getCopyButton(combo.data.code)),
);

/** Update sidebar for selected combo
 *  @param {ComboData} combo */
export const updateSidebarForCombo = (combo) => {

  // Instance
  const comboInfo = getComboInfo(combo);
  instance.replaceChildren(comboInfo);

  neighbors.replaceChildren($text("p", "Please select a node."));
  connections.replaceChildren($text("p", "Please select a node."));

};
