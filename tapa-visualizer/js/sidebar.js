/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

import { $, getComboName } from "./graph.js";

// sidebar content containers
export const sidebarContainers = [
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

/** @type {<T extends HTMLElement>(parent: T, ...children: (Node | string)[]) => T} */
const append = (parent, ...children) => {
  parent.append(...children);
  return parent;
}

/** @type {(args: [string, { arg: string, cat: string }][]) => HTMLElement} */
const parseArgs = args => append(
  $("dd"), append(
    $("ul"), ...args.map(
      ([name, { arg, cat }]) =>
        $("li", { textContent: `${name}: ${arg} (${cat})` })
    )
  )
);

// Details

/** @type {(node: import("@antv/g6").NodeData) => HTMLDListElement} */
const getDetailsFromNode = node => {

  /** @satisfies {HTMLDListElement} */
  const dl = append(
    $("dl"),
    $("dt", { textContent: "Instance Name" }),
    $("dd", { textContent: node.id }),
    $("dt", { textContent: "Upper Task" }),
    $("dd", { textContent: getComboName(node.combo ?? "<none>") }),
  );

  /**
   * @param {HTMLDListElement} dl
   * @param {SubTask} subTask
   * @param {number} [i] */
  const appendNodeData = (dl, { args, step }, i) => {
    const argsArr = Object.entries(args);
    if (typeof i === "number") {
      dl.append($("dt", {
        textContent: `Sub-Task ${i}`,
        style: "padding: .25rem 0 .1rem; border-top: 1px solid var(--border);",
      }));
    }
    dl.append(
      $("dt", { textContent: `Arguments` }),
      argsArr.length > 0
        ? parseArgs(argsArr)
        : $("dd", { textContent: "<none>" }),
      $("dt", { textContent: `Step` }),
      $("dd", { textContent: step }),
    );
  }

  if (node.data?.subTasks) {
    /**
     * @type {SubTask[]}
     * @ts-expect-error unknown */
    const subTasks = node.data?.subTasks;
    subTasks.forEach(
      (subTask, i) => appendNodeData(dl, subTask, i)
    );
  } else if (node.data?.args) {
    /**
     * @type {SubTask}
     * @ts-expect-error unknown */
    const subTask = node.data;
    appendNodeData(dl, subTask);
  } else {
    console.warn("Selected node is missing data!", node)
  }

  return dl;

};

/** Update sidebar for selected node
 *  @param {string} id
 *  @param {import("@antv/g6").NodeData} node
 *  @param {GraphData} graphData */
 export const updateSidebar = (id, node, graphData) => {

  instance.replaceChildren(
    node
    ? getDetailsFromNode(node)
    : $("p", { textContent: `node ${id} not found!` })
  );

  /** @type {(elements: (Node | string)[]) => HTMLUListElement} */
  const ul = elements => {
    const ul = $("ul", { style: "font-family: monospace;" });
    ul.append(...elements);
    return ul;
  };

  /** @type {import("@antv/g6").EdgeData[]} */
  const sources = [];
  /** @type {import("@antv/g6").EdgeData[]} */
  const targets = [];
  graphData.edges.forEach(edge => {
    edge.source === id && sources.push(edge);
    edge.target === id && targets.push(edge);
  });

  connections.replaceChildren(
    $("p", { textContent: "Sources" }),
    ul(sources.map(edge => $("li", { textContent: `${edge.id} -> ${edge.target}` }))),
    $("p", { textContent: "Targets" }),
    ul(targets.map(edge => $("li", { textContent: `${edge.id} <- ${edge.source}` }))),
  );

  /** @type {Set<string>} */
  const neighborIds = new Set();
  sources.forEach(edge => neighborIds.add(edge.target));
  targets.forEach(edge => neighborIds.add(edge.source));

  neighbors.replaceChildren(
    neighborIds.size > 0
    ? ul([...neighborIds.values().map(id => $("li", { textContent: id }))])
    : $("p", { textContent: `${id} has no neighbors.` }),
  );

};
