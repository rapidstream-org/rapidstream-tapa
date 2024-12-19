/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

// @ts-check

"use strict";

(() => {

  const { Graph } = G6;

  const container = document.querySelector(".graph-container");

  if (!(container instanceof HTMLElement)) {
    console.error(new TypeError("container is not a HTMLElement!"));
    return;
  }

  const data = {
    nodes: [
      { id: "1", "combo": "combo-1" },
      { id: "2" },
      { id: "3", "combo": "combo-1" },
      { id: "4" },
      { id: "5" },
    ],
    edges: [
      { source: "1", target: "2" },
      { source: "1", target: "3" },
      { source: "2", target: "4" },
      { source: "2", target: "5" },
    ],
    combos: [
      { id: "combo-1" },
    ]
  };

  // https://g6.antv.antgroup.com/api/graph/option
  const graph = new Graph({
    container,
    data,
    autoResize: true,
    autoFit: "view",
    padding: 10,
    theme: "light",

    // https://g6.antv.antgroup.com/api/elements/nodes/base-node
    node: {
      style: {
        size: 10,
        labelText: node => node.id,
        labelFontSize: 9,
        labelMaxLines: 3,
      },
    },
    edge: {},
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
      nodeSize: 25,
      collide: {
        strength: 0.5,
      },
    },
    behaviors: ["drag-canvas", "zoom-canvas", "drag-element"],
    plugins: [
      /** @type {import("@antv/g6").TooltipOptions} */
      ({
        type: "tooltip",
        getContent: (_event, items) => items.map(item => `ID: ${item.id}`).join("<br>")
      }),
    ],
  });

  void graph.render();

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

  setButton(".btn-fitCenter", () => void graph.fitCenter());
  setButton(".btn-fitView", () => void graph.fitView());

  console.log(graph);
})();
