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
 * @type {(node: NodeData) => HTMLElement} */
const getNodeInfo = node => {

  if (node.id.startsWith("<unknown>")) {
    return $text("p", "This node represents a missing fifo source or target.");
  }

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
 *  @param {string} id
 *  @param {Graph} graph */
export const updateSidebarForNode = (id, graph) => {

  /** @ts-expect-error @type {NodeData | undefined} */
  const node = graph.getNodeData(id);
  if (!node) {
    resetSidebar(`Node ${id} not found!`);
    return;
  }

  // Instance
  const nodeInfo = getNodeInfo(node);
  instance.replaceChildren(nodeInfo);

  // Neighbors & Connections
  const sources = graph.getRelatedEdgesData(node.id, "out");
  const targets = graph.getRelatedEdgesData(node.id, "in");

  /** `graph.getNeighborNodesData()` will call `graph.getRelatedEdgesData()`
   * again, thus it'll be better to get neighbors ourselves.
   * @type {Set<string>} */
  const neighborIds = new Set();
  sources.forEach(edge => neighborIds.add(edge.target));
  targets.forEach(edge => neighborIds.add(edge.source));

  /** @type {(elements: (Node | string)[]) => HTMLUListElement} */
  const ul = elements => append(
    $("ul", { style: "font-family: monospace;" }),
    ...elements
  );

  neighbors.replaceChildren(
    neighborIds.size > 0
    ? ul([...neighborIds.values().map(id => $("li", { textContent: id }))])
    : $("p", { textContent: `Node ${node.id} has no neighbors.` }),
  );

  // TODO: replace innerHTML
  connections.replaceChildren(
    $("p", { innerHTML: "Sources<br><code style='font-size: .8rem;'>Format: connection name -> target task name</code>" }),
    ul(sources.map(edge => $("li", { textContent: `${edge.id} -> ${edge.target}` }))),
    $("p", { innerHTML: "Targets<br><code style='font-size: .8rem;'>Format: connection name <- source task name</code>" }),
    ul(targets.map(edge => $("li", { textContent: `${edge.id} <- ${edge.source}` }))),
  );


};


/** Get an `<dl>` element containing:
 * Instance Name, Upper Task, Sub-Task(s)
 * @type {(node: ComboData) => HTMLElement} */
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
 *  @param {string} id */
export const updateSidebarForCombo = (id) => {

  /** @ts-expect-error @type {ComboData | undefined} */
  const combo = graph.getComboData(id);
  if (!combo) {
    resetSidebar(`Combo ${id} not found!`);
    return;
  }


  // Instance
  const comboInfo = getComboInfo(combo);
  instance.replaceChildren(comboInfo);

  neighbors.replaceChildren($text("p", "Please select a node."));
  connections.replaceChildren($text("p", "Please select a node."));

};
