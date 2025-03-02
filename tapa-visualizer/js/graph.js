/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { antvDagre, graphOptions, layoutOptions } from "./graph-options.js";
import {
  resetInstance,
  resetSidebar,
  updateSidebarForCombo,
  updateSidebarForNode,
} from "./sidebar.js";
import { getGraphData } from "./praser.js";
import { isCombo } from "./helper.js";

/** Used in "Save Image" button
 * @type {string} */
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

// File Input and graph rendering

/** @satisfies {HTMLInputElement & { files: FileList } | null} */
const fileInput = document.querySelector("input.fileInput");
if (fileInput === null) {
  throw new TypeError("Element input.fileInput not found!");
}

/** Grouping radios' form in header
 * @satisfies {HTMLFormElement & { elements: { grouping: { value: string; } } } | null} */
const grouping = document.querySelector(".grouping");


/** Render graph when file or grouping changed.
 * @type {(graph: Graph, graphData: GraphData) => Promise<void>} */
const renderGraph = async (graph, graphData) => {
  antvDagre.sortByCombo = graphData.combos.length > 1;
  graph.setData(graphData);
  await graph.render();
};

/** Render graph when file or grouping changed.
 * @type {(option: GetGraphDataOptions) => Promise<void>} */
const renderGraphWithNewOption = async option => {
  graphData = getGraphData(graphJson, option);
  globalThis.graphData = graphData;

  resetSidebar("Loading...");
  // Reset zoom if it's not 1
  if (graph.rendered && graph.getZoom() !== 1) {
    graph.setData({});
    await graph.draw();
    await graph.zoomTo(1, false);
  }

  await renderGraph(graph, graphData);
  resetInstance();
}

/** @param {Graph} graph */
const setupFileInput = (graph) => {

  const readFile = () => {

    /** @type {File | undefined} */
    const file = fileInput.files[0];

    if (!file) return false;
    if (file.type !== "application/json") {
      console.warn("File type is not application/json!");
    }

    // Update filename and graph
    filename = file.name;

    resetSidebar("Loading...");
    file.text().then(async text => {

      const flat = grouping?.elements.grouping.value === "flat";

      /** @satisfies {GraphJSON} */
      graphJson = JSON.parse(text);
      graphData = getGraphData(graphJson, { flat });
      Object.assign(globalThis, { graphJson, graphData });

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

      const topTask = graphJson.tasks[graphJson.top];
      topTask.level === "upper" &&
      Object.getOwnPropertyNames(topTask.tasks).length > 10 &&
      await graph.layout(graphOptions.layout);
      resetInstance();

    }).catch(error => {
      console.error(error);
      resetInstance(String(error));
    });

    return true;

  };

  readFile() ||
  resetInstance("Please load the graph.json file.");

  fileInput.addEventListener("change", readFile);
};

// Buttons

/** Button selector and click event callback
 * @param {Graph} graph
 * @returns {[string, EventListenerOrEventListenerObject][]} */
const getGraphButtons = (graph) => [[
  ".btn-clearGraph",
  () => void graph.clear().then(() => resetInstance("Please load a file."))
], [
  ".btn-rerenderGraph",
  () => void graph.layout().then(() => graph.fitView())
], [
  ".btn-fitCenter",
  () => void graph.fitCenter()
], [
  ".btn-fitView",
  () => void graph.fitView()
], [
  ".btn-saveImage",
  () => void graph.toDataURL().then(
    href => Object.assign(
      document.createElement("a"),
      { href, download: filename, rel: "noopener" },
    ).click(),
  )
]];

/** @param {Graph} graph */
const setupGraphButtons = graph => getGraphButtons(graph).forEach(
  ([selector, callback]) => {
    /** @satisfies { HTMLButtonElement | null } */
    const button = document.querySelector(selector);
    if (button) {
      button.addEventListener("click", callback);
      button.disabled = false;
    } else {
      console.warn(`setButton(): "${selector}" don't match any element!`);
    }
  }
);

// G6.Graph()

(() => {

  // https://g6.antv.antgroup.com/api/graph/option
  const graph = new G6.Graph({
    ...graphOptions,

    behaviors: [
      // "auto-adapt-label",
      /** @type {import("@antv/g6").BrushSelectOptions} */
      ({
        type: "brush-select",
        mode: "diff",
        enableElements: ["node"],
      }),
      /** @type {import("@antv/g6").DragCanvasOptions} */
      ({
        type: "drag-canvas",
        key: "drag-canvas",
      }),
      "zoom-canvas",
      "drag-element",

      /** @type {import("@antv/g6").CollapseExpandOptions} */
      ({
        type: "collapse-expand",
        animation: false, // only use layout animation in onExpand()
        onExpand: id => isCombo(id) && void graph.layout(layoutOptions),
      }),

      /** @type {import("@antv/g6").ClickSelectOptions} */
      ({
        type: "click-select",
        degree: 1,
        onClick: ({ target: item }) => {
          if (!("type" in item)) { resetSidebar(); return; }
          switch (item.type) {
            case "node":  updateSidebarForNode(item.id, graph); break;
            case "combo": updateSidebarForCombo(item.id); break;
            case "edge":  resetSidebar("Edge is not supported yet."); break;
            default: resetSidebar(); break;
          }
        },
      }),

    ],

  });

  globalThis.graph = graph;

  // Rerender when grouping method changed
  if (grouping) {
    for (let i = 0; i < grouping.elements.length; i++) {
      grouping.elements[i].addEventListener(
        "change",
        ({ target }) =>
          graphJson &&
          target instanceof HTMLInputElement &&
          void renderGraphWithNewOption({ flat: target.value === "flat" }),
      )
    }
  }

  // Graph loading finished, remove loading status in instance sidebar
  resetInstance();

  setupFileInput(graph);
  setupGraphButtons(graph);

  console.debug("graph object:\n", graph);
})();
