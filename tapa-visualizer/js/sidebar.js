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

// DOM Helpers

/** @type {(elements: (Node | string)[]) => HTMLUListElement} */
const ul = elements => append(
  $("ul", { style: "font-family: monospace;" }),
  ...elements
);

// Object to element parsers

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
const showCode = (code) => {
  const button = $text("button", "Show C++ Code");
  button.addEventListener("click", () => {
    // TODO: Set title of the dialog to the task name
    const container = document.querySelector("dialog code");
    if (container) {
      container.textContent = code;
      globalThis.Prism?.highlightElement(container, true);
    }
    document.querySelector("dialog")?.showModal();
  });
  return button;
};

// Details

/** Get an `<dl>` element containing:
 * Instance Name, Upper Task, Sub-Task(s)
 * @type {(node: NodeData) => HTMLElement} */
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
  const { task } = data;
  if (task) {
    dl.append(
      $text("dt", "Task Level"),
      $text("dd", task.level),
      $text("dt", "Build Target"),
      $text("dd", task.target),
      $text("dt", "Vendor"),
      $text("dd", task.vendor),

      // Lower task can have ports too
      $text("dt", "Ports"),
      task.ports && task.ports.length !== 0
        ? parsePorts(task.ports)
        : $text("dd", "none"),

      $text("dt", "Code"),
      append($("dd"), showCode(task.code)),
    )
  }

  if ("subTask" in data) {
    appendSubTask(dl, data.subTask);
  } else if ("subTasks" in data && Array.isArray(data.subTasks)) {
    data.subTasks.forEach(
      (subTask, i) => appendSubTask(dl, subTask, i)
    );
  } else {
    console.warn("Selected node is missing data!", node)
  }

  return dl;

};

const sourcesTitle = append(
  $("p", { textContent: "Sources" }),
  $("br"),
  $("code", {
    className: "hint",
    textContent: "Format: connection name -> target task name",
  }),
);
const targetsTitle = append(
  $("p", { textContent: "Targets" }),
  $("br"),
  $("code", {
    className: "hint",
    textContent: "Format: connection name <- source task name",
  }),
);

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

  neighbors.replaceChildren(
    neighborIds.size > 0
    ? ul([...neighborIds.values().map(id => $("li", { textContent: id }))])
    : $("p", { textContent: `Node ${node.id} has no neighbors.` }),
  );

  connections.replaceChildren(
    sourcesTitle,
    ul(sources.map(edge => $("li", { textContent: `${edge.id} -> ${edge.target}` }))),
    targetsTitle,
    ul(targets.map(edge => $("li", { textContent: `${edge.id} <- ${edge.source}` }))),
  );

};


/** Get an `<dl>` element containing:
 * Instance Name, Upper Task, Sub-Task(s)
 * @type {(node: ComboData) => HTMLElement} */
const getComboInfo = (combo) => {
  const comboName = getComboName(combo.id);

  const { data } = combo;
  const tasks = Object.entries(data.tasks).flatMap(
    ([name, subTasks]) => subTasks.map(
      (_subTask, i) => $("li", { textContent: `${name}/${i}` })
    )
  );

  /** get name of sub-task
   * @type {(by: [string, number] | undefined) => string}
   * @param by fifo.produced_by / fifo.consumed_by */
  const get = by => by?.join("/") ?? comboName;
  const fifos = Object.entries(data.fifos).map(
    ([name, { produced_by: p, consumed_by: c, depth }]) => $("li", {
      textContent: `${name}:\n${get(p)} -> ${get(c)}, depth: ${depth ?? "?"}`
    })
  );

  return append(
    $("dl"),
    $text("dt", "Instance Name"),
    $text("dd", combo.id),
    $text("dt", "Task Level"),
    $text("dd", data.level),
    $text("dt", "Build Target"),
    $text("dd", data.target),
    $text("dt", "Vendor"),
    $text("dd", data.vendor),

    // Parse ports to table
    $text("dt", "Ports"),
    data.ports && data.ports.length !== 0
      ? parsePorts(data.ports)
      : $text("dd", "none"),

    $text("dt", "Tasks"),
    tasks.length !== 0
    ? append($("dd"), ul(tasks))
    : $text("dd", "none"),

    $text("dt", "FIFOs"),
    fifos.length !== 0
    ? append($("dd", { style: "white-space: pre;" }), ul(fifos))
    : $text("dd", "none"),

    $text("dt", "Code"),
    append($("dd"), showCode(data.code)),
  );
};

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
