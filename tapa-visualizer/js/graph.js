/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { sidebarContainers, updateSidebar } from "./sidebar.js";
import { getGraphData } from "./praser.js";

/** @type {$} */
export const $ = (tagName, props) => Object.assign(
	document.createElement(tagName), props
);

/** @type {(comboId: string) => string} */
export const getComboName = comboId => comboId.replace(/^combo:/, "");


/** @type {GraphJSON} */
let graphJson;

/** Data for G6.Graph()
 * @type {GraphData} */
let graphData = {
  nodes: [],
  edges: [],
  combos: [],
};

// Grouping radios in header

/** @satisfies { HTMLFormElement & { elements: { grouping: { value: string; } } } | null } } */
const grouping = document.querySelector(".grouping");

// File Input

/** @satisfies {HTMLInputElement & { files: FileList } | null} */
const fileInput = document.querySelector("input.fileInput");
if (fileInput === null) {
  throw new TypeError("Element input.fileInput not found!");
}

/** @param {import("@antv/g6").Graph} graph */
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

    // render twice for medium-sized graphs
    // (large enough for a better 2nd render, small enough for performance)
    graphData.nodes.length > 50 &&
    graphData.nodes.length < 1000 &&
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

/** @param {import("@antv/g6").Graph} graph */
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
  setButton(".btn-refreshGraph", () => void graph.render());
  setButton(".btn-fitCenter", () => void graph.fitCenter());
  setButton(".btn-fitView", () => void graph.fitView());
}

// G6.Graph()

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

    animation: {
      duration: 250, // ms
    },

    // https://g6.antv.antgroup.com/api/elements/nodes/base-node
    node: {
      style: {
        labelText: node => node.id,
      },
    },
    edge: {
      style: {
        endArrow: true,
        labelFontFamily: "monospace",
        labelText: ({ id }) => {
          // Trim the prefix part
          id = id?.slice(id.indexOf("/") + 1);
          // If still very long, then cap each part's length to 15
          if (id && id.length > 20) {
            id = id.split("/").map(
              part => part.length <= 15 ? part : `${part.slice(1, 12)}...`
            ).join("/");
          }
          return id;
        },
      }
    },
    combo: {
      // type: "rect",
      style: {
        labelFill: "gray",
        labelFontSize: 10,
        labelPlacement: "top",
        strokeWidth: 2,
        labelText: ({id}) => getComboName(id),
      }
    },

    /** @type {BuiltInLayoutOptions} */
    layout: {
      type: "force-atlas2",
      kr: 200,
      kg: 20,
      tao: 10,
      nodeSize: [60, 40],
      preventOverlap: true,
      padding: 10,
      // mode: "linlog",
      // dissuadeHubs: true,
    },

    transforms: ["process-parallel-edges"],

    behaviors: [
      "drag-canvas",
      "zoom-canvas",
      "drag-element",
      /** @type {import("@antv/g6").ClickSelectOptions} */
      ({
        type: "click-select",
        degree: 1,
        // TODO: update sidebar for combo (UpperTask)
        onClick: ({ target: item }) => {
          if (item.type !== "node") {
            const p = $("p", { textContent: "Please select a node." });
            sidebarContainers.forEach(
              element => element.replaceChildren(p.cloneNode(true)),
            );
            return;
          }

          const node = graph.getNodeData(item.id);
          updateSidebar(item.id, node, graphData);
        }
      })
    ],

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
  sidebarContainers[0].replaceChildren(
    $("p", { textContent: "Please select a node." }),
  );

  setupFileInput(graph);
  setupGraphButtons(graph);

  console.debug("graph object\n", graph);
})();
