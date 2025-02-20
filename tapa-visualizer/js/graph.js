/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { getComboName, trimEdgeId } from "./helper.js";
import {
  resetInstance,
  resetSidebar,
  updateSidebarForCombo,
  updateSidebarForNode
} from "./sidebar.js";
import { getGraphData } from "./praser.js";

/** @type {string} */
let filename;

/** @type {GraphJSON} */
let graphJson;

/** Data for G6.Graph()
 * @type {GraphData} */
let graphData = {
  nodes: [],
  edges: [],
  combos: [],
};

/** `force-atlas2` specifc config for large graph:
 * - `kr`:    much larger than kg
 * - `kg`:    much smaller than kr, larger than 1 to work with small graph
 * - `ks`:    speed, at least 0.01 * nodeSize
 * - `ksmax`: max speed, not very important
 * - `tao`:   10 is the sweet point
 * @type {import("../types/g6.js").ForceAtlas2Fixed}
 * @satisfies {BuiltInLayoutOptions} */
const layoutConfig = {
  type: "force-atlas2",
  kr: 120,
  kg: 10,
  ks: 1,
  ksmax: 120,
  tao: 10,
  preventOverlap: true,
  nodeSize: [100, 40],
  preLayout: true,
};


/** Grouping radios' form in header
 * @satisfies {HTMLFormElement & { elements: { grouping: { value: string; } } } | null} */
const grouping = document.querySelector(".grouping");

// File Input

/** @satisfies {HTMLInputElement & { files: FileList } | null} */
const fileInput = document.querySelector("input.fileInput");
if (fileInput === null) {
  throw new TypeError("Element input.fileInput not found!");
}

/** Render graph when file or grouping changed.
 * @type {(graph: import("@antv/g6").Graph, graphData: GraphData) => Promise<void>} */
const renderGraph = async (graph, graphData) => {
  graph.setData(graphData);
  await graph.render();
}

/** @param {import("@antv/g6").Graph} graph */
const setupFileInput = (graph) => {

  const readFile = () => {

    /** @type {File | undefined} */
    const file = fileInput.files[0];

    if (!file) return;
    if (file.type !== "application/json") {
      console.warn("File type is not application/json!");
    }

    // Update filename and graph
    filename = file.name;

    resetSidebar("Loading...");
    file.text().then(async text => {

      const flat = grouping?.elements.grouping.value === "flat";

      /** @satisfies {GraphJSON} */
      // eslint-disable-next-line @typescript-eslint/no-unsafe-assignment
      graphJson = JSON.parse(text);
      graphData = getGraphData(graphJson, flat);

      console.debug(`${filename}:\n`, graphJson, "\ngraphData:\n", graphData);

      // Reset zoom if it's not 1
      if (graph.rendered && graph.getZoom() !== 1) {
        graph.setData({});
        await graph.draw();
        await graph.zoomTo(1, false);
      }

      // Render & auto layout graph
      resetInstance("Rendering...");
      await renderGraph(graph, graphData);
      graphData.nodes.length > 5 &&
      graphData.nodes.length <= 100 &&
      await graph.layout(layoutConfig);
      resetInstance();

    }).catch(error => {
      console.error(error);
      resetInstance(String(error));
    });

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

  setButton(".btn-clearGraph   ", () => void graph.clear());
  setButton(".btn-rerenderGraph", () => void graph.layout().then(() => graph.fitView()));
  setButton(".btn-fitCenter    ", () => void graph.fitCenter());
  setButton(".btn-fitView      ", () => void graph.fitView());
  setButton(".btn-saveImage    ", () => void graph.toDataURL().then(
    href => Object.assign(
      document.createElement("a"),
      { href, download: filename, rel: "noopener" }
    ).click()
  ));
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

    theme: "light",
    autoResize: true,
    autoFit: {
      type: "view",
      options: { when: "overflow" },
      animation: { duration: 100 }, // ms
    },

    padding: 20, // px
    zoomRange: [0.1, 2.5], // %
    animation: { duration: 250 }, // ms

    // TODO: customize selected style
    // https://g6.antv.antgroup.com/api/elements/nodes/base-node
    node: {
      type: "rect",
      style: {
        size: [120, 40],
        fill: ({ style }) => style?.fill ?? "#198754",
        radius: 2,
        strokeOpacity: 0, // override selected style

        labelPlacement: "center",
        labelWordWrap: true, // enable label ellipsis
        labelMaxWidth: 100,
        labelMaxLines: 2,
        labelFill: "white",
        labelFontWeight: 700,
        labelFontFamily: "monospace",
        labelText: node => node.id,
      },
    },
    edge: {
      style: {
        stroke: "#A3CFBB",
        endArrow: true,
        labelBackground: true,
        labelBackgroundFill: "white",
        labelBackgroundFillOpacity: 0.75,
        labelBackgroundRadius: 1,
        labelFontFamily: "monospace",
        labelText: ({ id }) => trimEdgeId(id),
      }
    },
    combo: {
      // type: "rect",
      style: {
        labelFill: "gray",
        labelFontSize: 10,
        labelPlacement: "top",
        strokeWidth: 2,
        labelText: ({ id }) => getComboName(id),
      }
    },

    layout: { animation: false, ...layoutConfig },

    transforms: ["process-parallel-edges"],

    behaviors: [
      "drag-canvas",
      "zoom-canvas",
      "drag-element",
      /** @type {import("@antv/g6").ClickSelectOptions} */
      ({
        type: "click-select",
        degree: 1,
        onClick: ({ target: item }) => {
          if (!("type" in item)) {
            resetSidebar();
            return;
          }

          switch (item.type) {
            case "node": {
              /** @ts-expect-error @type {NodeData} */
              const node = graph.getNodeData(item.id);
              node
                ? updateSidebarForNode(node, graphData)
                : resetSidebar(`node ${item.id} not found!`);
              break;
            }
            case "combo": {
              /** @ts-expect-error @type {ComboData} */
              const combo = graph.getComboData(item.id);
              combo
                ? updateSidebarForCombo(combo)
                : resetSidebar(`combo ${item.id} not found!`);
              break;
            }
            case "edge":
              resetSidebar("Edge is not supported yet.");
              break;

            default:
              resetSidebar();
              break;
          }
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

  // Rerender when grouping method changed
  if (grouping) {
    for (let i = 0; i < grouping.elements.length; i++) {
      grouping.elements[i].addEventListener("change", ({target}) => {
        if (!(target instanceof HTMLInputElement)) return;
        if (graphJson) {
          const flat = target.value === "flat";
          graphData = getGraphData(graphJson, flat);

          resetSidebar("Loading...");
          void renderGraph(graph, graphData)
            .then(() => resetInstance());
        }
      })
    }
  }

  // Graph loading finished, remove loading status in instance sidebar
  resetInstance();

  setupFileInput(graph);
  setupGraphButtons(graph);

  console.debug("graph object:\n", graph);
})();
