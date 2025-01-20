/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

import { $, getComboName } from "./graph.js";

/** @type {<T extends HTMLElement>(parent: T, ...children: (Node | string)[] ) => T} */
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

/** @type {(node: import("@antv/g6").NodeData ) => HTMLDListElement} */
export const getDetailsFromNode = node => {

  /** @satisfies {HTMLDListElement} */
  const dl = append(
    $("dl"),
    $("dt", { textContent: "Instance Name" }),
    $("dd", { textContent: node.id }),
    $("dt", { textContent: "Upper Task" }),
    $("dd", { textContent: getComboName(node.combo ?? "<none>") }),
  );

  /** @type {(dl: HTMLDListElement, subTask: SubTask, i?: number) => void} */
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
