/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { $, $text, append, getComboId, getComboName } from "./helper.js";
import Prism from "virtual:prismjs";

// sidebar content container elements
const sidebarContainers = [
  "explorer",
  "instance",
  "task",
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

// Explorer don't need reset when anything is selected
// .shift() introduces undefined, use [0] for the explorer variable
const explorer = sidebarContainers[0];
sidebarContainers.shift();

const [
  instance,
  task,
  neighbors,
  connections,
] = sidebarContainers;

export const resetInstance = (text = "Please select an item.") =>
  instance.replaceChildren($text("p", text));

export const resetSidebar = (instanceText = "Please select an item.") => [
  instanceText,
  "Please select a node or combo.",
  "Please select a node.",
  "Please select a node.",
].forEach(
  (text, i) => sidebarContainers[i].replaceChildren($text("p", text)),
);

// DOM Helpers

/** @type {(elements: (Node | string)[]) => HTMLUListElement} */
const ul = elements => append(
  $("ul", { style: "font-family: monospace; white-space: pre-wrap;" }),
  ...elements,
);

// Object to element parsers

/** @type {(args: [string, { arg: string, cat: string }][]) => HTMLElement} */
const parseArgs = args => append(
  $("dd"), append(
    $("ul"), ...args.map(
      ([name, { arg, cat }]) =>
        $text("li", `${name}: ${arg} (${cat})`),
    ),
  ),
);

/** Parse ports into <table>
 * @type {(ports: Port[]) => HTMLTableElement} */
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

const codeDialog = document.querySelector("dialog");
const codeContainer = document.querySelector("dialog code");
/** @type {(code: string, taskName: string) => HTMLButtonElement} */
const showCode = codeDialog && codeContainer
  ? (code, taskName) => {
    const button = $text("button", "Show C++ Code");
    button.addEventListener("click", () => {
      codeContainer.textContent = code;
      Prism.highlightElement(codeContainer);

      const title = codeDialog.querySelector(":scope h2");
      if (title) title.textContent = taskName;

      codeDialog.showModal();
    });
    return button;
  }
  : () => $("button", {
    textContent: "Show C++ Code",
    title: "Error: C++ code-related element(s) does not exist!",
    disabled: true,
  });

// Explorer

/** @type {(graphJSON: GraphJSON) => void} */
export const updateExplorer = ({ tasks, top }) => {
  const taskLis = [];

  /** @type {(task: UpperTask, level: number) => void} */
  const parseUpperTask = (task, level) => {
    for (const subTaskName in task.tasks) {
      const subTask = tasks[subTaskName];
      if (!subTask) {
        console.warn(`task not found: ${subTaskName}`);
        continue;
      }
      taskLis.push($text("li", `${"  ".repeat(level)}${subTaskName}`));
      if (subTask.level === "upper") {
        parseUpperTask(subTask, level + 1);
      }
    }
  };

  const topTask = tasks[top];
  if (!topTask) {
    // TODO: list the tasks instead
    explorer.replaceChildren($text("p", "This graph has no top task."));
  }
  taskLis.push($text("li", top));
  if (topTask.level === "upper") {
    parseUpperTask(topTask, 1);
  }

  const taskUl = ul(taskLis);
  taskUl.addEventListener("click", ({ target }) => {
    if (target instanceof HTMLLIElement && target.textContent) {

      /** @type {Record<string, string[]>} */
      const states = {};
      [graphData.nodes, graphData.edges, graphData.combos].forEach(
        items => items.forEach(({ id }) => states[id ?? ""] = [])
      );

      const id = target.textContent.trim();
      if (id in states) {
        states[id].push("selected");
      } else {
        const comboId = getComboId(id);
        comboId in states
          ? states[comboId].push("selected")
          : console.warn(`id not found: ${id}`);
      }

      void graph.setElementState(states);
    }
  });

  explorer.replaceChildren(taskUl);
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

/** Get an `<dl>` element containing:
 * Instance Name, Upper Task, Sub-Task(s)
 * @type {(node: Task, taskName: string) => HTMLElement} */
const getTaskInfo = (task, taskName) => {
  const dl = append(
    $("dl"),
    $text("dt", "Task Name"),
    $text("dd", taskName),
    $text("dt", "Task Level"),
    $text("dd", task.level),
    $text("dt", "Build Target"),
    $text("dd", task.target),
    $text("dt", "Vendor"),
    $text("dd", task.vendor),

    // Lower task can have ports too
    $text("dt", "Ports"),
    task.ports && task.ports.length !== 0
      ? append($("dd"), parsePorts(task.ports))
      : $text("dd", "none"),
  );

  if (task.level === "upper") {
    /** @type {HTMLLIElement[]} */
    const tasks = [];
    for (const name in task.tasks) {
      const subTasks = task.tasks[name];
      for (let i = 0; i < subTasks.length; i++) {
        tasks.push($("li", { textContent: `${name}/${i}` }));
      }
    }

    /** get sub-tasks' name for fifo
     * @type {(by: [string, number] | undefined) => string}
     * @param by fifo.produced_by / fifo.consumed_by */
    const getName = by => by?.join("/") ?? taskName;

    /** @type {HTMLLIElement[]} */
    const fifos = [];
    for (const name in task.fifos) {
      const { produced_by: p, consumed_by: c, depth: d } = task.fifos[name];
      const depth = d !== undefined ? ` (depth: ${d})` : "";
      const fifo = `${getName(p)} -> ${getName(c)}`;
      fifos.push($text("li", `${name}${depth}:\n${fifo}`));
    }

    dl.append(
      $text("dt", "Sub-Tasks"),
      tasks.length !== 0
        ? append($("dd"), ul(tasks))
        : $text("dd", "none"),

      $text("dt", "FIFO Streams"),
      fifos.length !== 0
        ? append($("dd"), ul(fifos))
        : $text("dd", "none"),
    );
  }

  dl.append(
    $text("dt", "Code"),
    append($("dd"), showCode(task.code, taskName)),
  );

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
 * @param {string} id
 * @param {Graph} graph */
export const updateSidebarForNode = (id, graph) => {

  /** @ts-expect-error @type {NodeData | undefined} */
  const node = graph.getNodeData(id);
  if (!node) {
    resetSidebar(`Node ${id} not found!`);
    return;
  }

  // Details sidebar: Instance & Task
  const nodeInfo = getNodeInfo(node);
  instance.replaceChildren(nodeInfo);

  const taskInfo = node.data.task
    ? getTaskInfo(node.data.task, node.id)
    : $text("p", "This item has no task infomation.");
  task.replaceChildren(taskInfo);

  // Connection sidebar: Neighbors & Connections
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


/** @type {(node: ComboData, graph: Graph) => HTMLElement} */
const getComboInfo = (combo, graph) => {
  const children = graph.getChildrenData(combo.id).map(
    child => $text("li", child.id),
  );

  return append(
    $("dl"),
    $text("dt", "Instance Name"),
    $text("dd", combo.id),
    $text("dt", "Children"),
    append($("dd"), ul(children)),
  );
};

/** Update sidebar for selected combo
 * @param {string} id
 * @param {Graph} graph */
export const updateSidebarForCombo = (id, graph) => {

  /** @ts-expect-error @type {ComboData | undefined} */
  const combo = graph.getComboData(id);
  if (!combo) {
    resetSidebar(`Combo ${id} not found!`);
    return;
  }

  const comboInfo = getComboInfo(combo, graph);
  instance.replaceChildren(comboInfo);

  const taskInfo = getTaskInfo(combo.data, getComboName(combo.id));
  task.replaceChildren(taskInfo);


  neighbors.replaceChildren($text("p", "Please select a node."));
  connections.replaceChildren($text("p", "Please select a node."));

};

/** Update sidebar for selected edge
 * TODO: support show edge infomation
 * @param {string} id */
export const updateSidebarForEdge = id => {
  id.at(-1);
  instance.replaceChildren(
    $text("p", "Edge is not fully supported yet.")
  );

  task.replaceChildren(
    $text("p", "Edge has no task infomation.")
  );

};
