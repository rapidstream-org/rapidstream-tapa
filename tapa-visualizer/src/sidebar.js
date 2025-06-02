/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { $, $text, append, getComboId, getComboName } from "./helper.js";
import Prism from "./prism-config.js";

// sidebar content container elements
const sidebarContainers = [
  "explorer",
  "cflags",
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

// Explorer & Cflags don't need reset when anything is selected
// .shift() introduces undefined, use [0] for the explorer variable
const [explorer, cflags] = sidebarContainers.splice(0, 2);

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

/** Parse fifos into <table>
 * @type {(fifos: [string, FIFO][], taskName: string) => HTMLTableElement} */
const parseFifos = (fifos, taskName) => append(
  $("table", { style: "text-align: center;" }),
  append(
    $("tr"),
    $text("th", "Name"),
    $text("th", "Source -> Target"),
    $text("th", "Depth"),
  ),
  ...fifos.map(
    ([name, { produced_by, consumed_by, depth }]) => {
      const source = produced_by?.join("/") ?? taskName;
      const target = consumed_by?.join("/") ?? taskName;
      return append(
        $("tr"),
        $text("td", name),
        $text("td", `${source} -> ${target}`),
        $text("td", depth ?? "/"),
      );
    }
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

      code.length >= 2500
      ? codeContainer.setAttribute("style", "font-size: .8rem;")
      : codeContainer.removeAttribute("style");

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
export const updateExplorer = ({ tasks, top, cflags: flags }) => {

  cflags.replaceChildren(
    ul(
      flags.reduce(
        (arr, cur) => {
          const last = arr.length - 1;
          // concat "-isystem" with its following argument
          if (cur === "-isystem") {
            arr.push(`${cur} `);
          } else if (arr[last]?.endsWith(" ")) {
            arr[last] += cur;
          } else {
            arr.push(cur);
          }
          return arr;
        },
        /** @type {string[]} */
        ([]),
      ).map(flag => {
        const li = $text("li", flag);
        if (flag.startsWith("-isystem ")) {
          li.className = "isystem";
        }
        return li;
      }),
    ),
  );

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
    $text("dt", "Node Name"),
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
  };

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


/** Get an array of elements containing:
 * Task Name & Info & Show Code button, Ports, Sub-Tasks & FIFO Streams
 * @type {(node: Task, id: string) => HTMLElement[]} */
const getTaskInfo = (task, id) => {

  // Get taskName from id like `${taskName}/x`
  const i = id.indexOf("/");
  const taskName = i !== -1 ? id.slice(0, i) : id;

  const compactInfo = append(
    $("dl", { className: "compact" }),
    $text("dt", "Task Name:"),
    $text("dd", taskName),
    $text("dt", "Task Level:"),
    $text("dd", task.level),
    $text("dt", "Build Target:"),
    $text("dd", task.target),
    $text("dt", "Vendor:"),
    $text("dd", task.vendor),
    $text("dt", "Code:"),
    append($("dd"), showCode(task.code, taskName)),
  );

  const listInfo = append(
    $("dl"),
    // Lower task can have ports too
    $text("dt", "Ports"),
    task.ports && task.ports.length !== 0
      ? append($("dd"), parsePorts(task.ports))
      : $text("dd", "none"),
  );

  const elements = [
    compactInfo,
    listInfo,
  ];

  if (task.level === "upper") {
    const fifos = Object.entries(task.fifos);

    /** @type {HTMLLIElement[]} */
    const tasks = [];
    for (const name in task.tasks) {
      const subTasks = task.tasks[name];
      for (let i = 0; i < subTasks.length; i++) {
        tasks.push($("li", { textContent: `${name}/${i}` }));
      }
    }

    listInfo.append(
      $text("dt", "FIFO Streams"),
      fifos.length !== 0
        ? append($("dd"), parseFifos(fifos, taskName))
        : $text("dd", "none"),

      $text("dt", "Sub-Tasks"),
      tasks.length !== 0
        ? append($("dd"), ul(tasks))
        : $text("dd", "none"),
    );
  }

  return elements;
};

const sourcesTitle = append(
  $("p", { textContent: "Sources" }),
  $("br"),
  $("code", {
    className: "hint",
    textContent: "Format: connection name -> target name",
  }),
);
const targetsTitle = append(
  $("p", { textContent: "Targets" }),
  $("br"),
  $("code", {
    className: "hint",
    textContent: "Format: connection name <- source name",
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
    : [$text("p", "This item has no task infomation.")];
  task.replaceChildren(...taskInfo);

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
    ? ul([...neighborIds.values().map(id => $text("li", id))])
    : $text("p", `Node ${node.id} has no neighbors.`),
  );

  neighborIds.size > 0
  ? neighbors.replaceChildren(
    append($("p", { className: "hint" }), $text("code", node.id), "'s neighbors:"),
    ul([...neighborIds.values().map(id => $text("li", id))]),
  )
  : neighbors.replaceChildren($text("p", `Node ${node.id} has no neighbors.`));

  connections.replaceChildren(
    append($("p", { className: "hint" }), $text("code", node.id), "'s connections:"),
    sourcesTitle,
    sources.length !== 0
    ? ul(sources.map(edge => $text("li", `${edge.id} -> ${edge.target}`)))
    : $("p", { textContent: "none", style: "padding-inline-start: 1em; font-size: .85rem;" }),
    targetsTitle,
    targets.length !== 0
    ? ul(targets.map(edge => $text("li", `${edge.id} <- ${edge.source}`)))
    : $("p", { textContent: "none", style: "padding-inline-start: 1em; font-size: .85rem;" }),
  );

};


/** @type {(node: ComboData, graph: Graph) => HTMLElement} */
const getComboInfo = (combo, graph) => {
  const children = graph.getChildrenData(combo.id).map(
    child => $text("li", child.id),
  );

  return append(
    $("dl"),
    $text("dt", "Combo Name"),
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
  task.replaceChildren(...taskInfo);

  neighbors.replaceChildren($text("p", "Please select a node."));
  connections.replaceChildren($text("p", "Please select a node."));

};

/** Update sidebar for selected edge
 * TODO: support show edge infomation
 * @param {string} id
 * @param {Graph} graph */
export const updateSidebarForEdge = (id, graph) => {

  const edge = graph.getEdgeData(id);
  if (!edge) {
    resetSidebar(`Edge ${id} not found!`);
    return;
  }

  const dl = append(
    $("dl"),
    $text("dt", "Edge Name"),
    $text("dd", id),
    $text("dt", "Source Node"),
    $text("dd", edge.source),
    $text("dt", "Target Node"),
    $text("dd", edge.target),
  );

  instance.replaceChildren(dl);

  /** @type {HTMLElement[]} */
  const taskElements = [];

  /** @ts-expect-error @type {NodeData} */
  const sourceNode = graph.getNodeData(edge.source);
  /** @ts-expect-error @type {NodeData} */
  const targetNode = graph.getNodeData(edge.target);

  sourceNode?.data?.task &&
  taskElements.push(
    $text("h3", "Source Task"),
    ...getTaskInfo(sourceNode.data.task, sourceNode.id),
  );

  sourceNode?.data?.task && targetNode?.data?.task &&
  taskElements.push($("hr"));

  targetNode?.data?.task &&
  taskElements.push(
    $text("h3", "Target Task"),
    ...getTaskInfo(targetNode.data.task, targetNode.id),
  );

  const hint = $("p", {
    style: "opacity: .75;",
    textContent: "This edge has no task infomation.",
  });

  taskElements.length !== 0
    ? task.replaceChildren(...taskElements)
    : task.replaceChildren(hint);

};
