/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { getDetailsFromNode } from "./sidebar.js";
import { getGraphData } from "./praser.js";

/** @type {<T = HTMLElement>(tagName: string, ...props: Record<string, unknown>[] ) => T} */
// eslint-disable-next-line @typescript-eslint/no-unsafe-return
export const $ = (tagName, ...props) => Object.assign(
	document.createElement(tagName), ...props
);

/** @type {(comboId: string) => string} */
export const getComboName = comboId => comboId.replace(/^combo:/, "");


/** @type {GraphJSON} */
let graphJson;

/** Data for G6.Graph()
 * @type {Required<import("@antv/g6").GraphData>} */
let graphData = {
  nodes: [],
  edges: [],
  combos: [],
};

// Grouping radios in header

/** @type { HTMLFormElement & { elements: { grouping: { value: string; } } } | null } } */
const grouping = document.querySelector(".grouping");

// Details sidebar

/** @type {HTMLLIElement | null} */
const details = document.querySelector(".instance-content");
if (details === null) {
  throw new TypeError("Element .instance-content not found!");
}

const resetDetails = () => details.replaceChildren(
  $("p", { textContent: "Please select a node." })
);

// File Input

/** @type {HTMLInputElement & { files: FileList } | null} */
const fileInput = document.querySelector("input.fileInput");
if (fileInput === null) {
  throw new TypeError("Element input.fileInput not found!");
}

/** @type {(graph: import("@antv/g6").Graph) => void} */
const setupFileInput = (graph) => {
  const reader = new FileReader();
  reader.addEventListener("load", e => {
    const result = e.target?.result;
    if (typeof result !== "string") return;

    const flat = grouping?.elements.grouping.value === "flat";

    /** @satisfies {GraphJSON} */
    // eslint-disable-next-line @typescript-eslint/no-unsafe-assignment
    graphJson = JSON.parse(result);
    graphData = getGraphData(graphJson, flat);

    console.debug("graph.json\n", graphJson);
    console.debug("graphData\n", graphData);

    graph.setData(graphData);
    void graph.render();
  });

  const readFile = () => {
    const file = fileInput.files[0];
    if (file) reader.readAsText(file);
  };
  readFile();
  fileInput.addEventListener("change", readFile);
};

// Buttons

/** @type {(graph: import("@antv/g6").Graph) => void} */
const setupGraphButtons = (graph) => {
  /**
   * @typedef {EventListenerOrEventListenerObject} Listener
   * @type {(selector: string, listener: Listener) => void} */
  const setButton = (selector, callback) => {
    /** @satisfies { HTMLButtonElement | null } */
    const button = document.querySelector(selector);
    if (button) {
      button.addEventListener("click", callback);
      button.disabled = false;
    } else {
      console.warn(`setButton(): "${selector}" don't match any element!`);
    }
  };

  setButton(".btn-clearGraph", () => void graph.clear());
  setButton(".btn-fitCenter", () => void graph.fitCenter());
  setButton(".btn-fitView", () => void graph.fitView());
}

(() => {

  const container = document.querySelector(".graph-container");
  if (!(container instanceof HTMLElement)) {
    throw new TypeError("container is not a HTMLElement!");
  }

  // https://g6.antv.antgroup.com/api/graph/option
  const graph = new G6.Graph({
    container,
    autoResize: true,
    autoFit: "view",
    padding: 10,
    theme: "light",

    // https://g6.antv.antgroup.com/api/elements/nodes/base-node
    node: {
      style: {
        labelText: node => node.id,
      },
    },
    edge: {
      style: {
        endArrow: true,
        labelText: ({id}) => id?.split("/").at(-1),
        labelFontFamily: "monospace",
      }
    },
    combo: {
      // type: "rect",
      style: {
        labelText: ({id}) => getComboName(id),
        labelFill: "gray",
        labelFontSize: 10,
        labelPlacement: "top",
        strokeWidth: 2,
      }
    },

    layout: {
      type: "force",
    },
    behaviors: [
      "drag-canvas",
      "zoom-canvas",
      "drag-element",
      /** @type {import("@antv/g6").ClickSelectOptions} */
      ({
        type: "click-select",
        degree: 1,
        onClick: ({ target }) => {
          if (target.type !== "node") {
            resetDetails();
            return;
          }

          const node = graph.getNodeData(target.id);
          details.replaceChildren(
            node
            ? getDetailsFromNode(node)
            : $("p", { textContent: `node ${target.id} not found!` })
          );
        }
      })
    ],
    transforms: ["process-parallel-edges"],
    plugins: [
      /** @type {import("@antv/g6").TooltipOptions} */
      ({
        type: "tooltip",
        getContent: (_event, items) => Promise.resolve(
          items.map(
            item => `ID: <code>${item.id}</code>`
          ).join("<br>")
        ),
      }),
    ],
  });

  if (grouping) {
    for (let i = 0; i < grouping.elements.length; i++) {
      grouping.elements[i].addEventListener("change", ({target}) => {
        if (!(target instanceof HTMLInputElement)) return;
        if (graphJson) {
          const flat = target.value === "flat";
          graphData = getGraphData(graphJson, flat);
          graph.setData(graphData);
          void graph.render();
        }
      })
    }
  }

  // Override the default "Loading..."
  resetDetails();

  setupFileInput(graph);
  setupGraphButtons(graph);

  console.debug("graph object\n", graph);
})();
