/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { getGraphData } from "./praser.js";

/** @type {(graph: import("@antv/g6").Graph) => void} */
const setupFileInput = (graph) => {

  /** @type {HTMLInputElement & { files: FileList } | null} */
  const fileInput = document.querySelector("input.fileInput");
  if (fileInput === null) {
    throw new TypeError("fileInput is null!");
  }

  const reader = new FileReader();
  reader.addEventListener("load", e => {
    const result = e.target?.result;
    if (typeof result !== "string") return;

    /** @type {GraphJSON} */
    // eslint-disable-next-line @typescript-eslint/no-unsafe-assignment
    const json = JSON.parse(result);
    const graphData = getGraphData(json);
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

/** @type {(graph: import("@antv/g6").Graph) => void} */
const setupGraphButtons = (graph) => {
  /**
   * @typedef {EventListenerOrEventListenerObject} Listener
   * @type {(selector: string, listener: Listener) => void} */
  const setButton = (selector, callback) => {
    /** @type { HTMLButtonElement | null } */
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
        labelText: ({id}) => id,
        labelFontFamily: "monospace",
      }
    },
    combo: {
      type: "rect",
      style: {
        labelText: combo => combo.id,
        labelFill: "gray",
        labelFontSize: 9,
        labelPlacement: "top",
      }
    },

    layout: {
      type: "d3-force",
      nodeSize: 150,
      collide: {
        strength: 0.5,
      },
    },
    behaviors: ["drag-canvas", "zoom-canvas", "drag-element"],
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

  setupFileInput(graph);
  setupGraphButtons(graph);
  console.debug(graph);
})();
