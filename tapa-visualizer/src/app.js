/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { DragCanvas, Graph } from "@antv/g6";
import { createIcons, icons } from "lucide";

createIcons({ icons });

import { antvDagre, dagre, forceAtlas2, graphOptions } from "./graph-config.js";
import {
  resetInstance,
  resetSidebar,
  updateExplorer,
  updateSidebarForCombo,
  updateSidebarForEdge,
  updateSidebarForNode,
} from "./sidebar.js";
import { getComboId } from "./helper.js";
import { getGraphData } from "./parser.js";

import "../css/style.css";

/** Used in "Save Image" button
 * @type {string | undefined} */
let filename;

/** @type {GraphJSON | undefined} */
let graphJSON;

/** Data for G6.Graph()
 * @type {GraphData} */
let graphData = { nodes: [], edges: [], combos: [] };


// Form and options + graph rendering for new option

/** Grouping radios' form in header
 * @satisfies {GroupingForm | null} */
const groupingForm = document.querySelector(".grouping");

/** Options sidebar's form, contains all the options other than grouping.
 *
 * For convenience, it's the only sidebar managed outside of `sidebar.js`.
 * @satisfies {OptionsForm | null} */
const optionsForm = document.querySelector(".sidebar-content-options");

const getLayout = (layout = optionsForm?.elements.layout.value) => {
  switch (layout) {
    case "force-atlas2": return forceAtlas2;
    case "antv-dagre": return antvDagre;
    case "dagre": return dagre;
    default: return forceAtlas2;
  }
};

const getOptions = () => ({
  grouping: groupingForm?.elements.grouping.value ?? "merge",
  expand: optionsForm?.elements.expand.value === "true",
  port: optionsForm?.elements.port.value === "true",
});

let options = getOptions();

/** Render graph when file or grouping changed.
 * @type {(graph: Graph, graphData: GraphData) => Promise<void>} */
const renderGraph = async (graph, graphData) => {
  // Update sortByCombo by combo amount
  antvDagre.sortByCombo = graphData.combos.length > 1;

  // Reset zoom if it's not 100% (1)
  if (graph.rendered && graph.getZoom() !== 1) {
    graph.setData({});
    await graph.draw();
    await graph.zoomTo(1, false);
  }

  graph.setData(graphData);
  await graph.render();
};

/** @param {Graph} graph */
const setupRadioToggles = graph => {

  /** Re-render graph with new option;
   * update options & re-render graph when option changed.
   * @type {(newOption: GetGraphDataOptions) => Promise<void>} */
  const updateGraph = async (newOption) => {
    Object.assign(options, newOption);

    // Only re-render if graph exist
    if (!graphJSON) return;
    graphData = getGraphData(graphJSON, options);
    console.debug(
      "graphData:\n", graphData,
      "\ngetGraphData() options:", options,
    );

    resetSidebar("Loading...");
    await renderGraph(graph, graphData);
    resetInstance();
  };

  if (groupingForm) {
    for (let i = 0; i < groupingForm.elements.length; i++) {
      groupingForm.elements[i].addEventListener("change",
        () => void updateGraph({ grouping: groupingForm.grouping.value }));
    }
  }

  if (optionsForm) {
    for (let i = 0; i < optionsForm.elements.length; i++) {
      const element = optionsForm.elements[i];
      if (!(element instanceof HTMLInputElement)) continue;

      element.addEventListener(
        "change",
        element.name === "layout"
          // layout option
          ? () => {
            graph.setLayout(getLayout());
            void graph.layout().then(() => graph.fitView());
          }
          // other options
          : ({ target }) => {
            target instanceof HTMLInputElement &&
            void updateGraph({ [target.name]: target.value === "true" });
          }
      );
    }
  }
};


// File Input + graph rendering for new file

/** @satisfies {HTMLInputElement & { files: FileList } | null} */
const fileInput = document.querySelector("input.fileInput");
if (fileInput === null) {
  throw new TypeError("Element input.fileInput not found!");
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

      /** @satisfies {GraphJSON} */
      graphJSON = JSON.parse(text);
      if (!graphJSON?.tasks) {
        resetInstance("Invalid graph.json, please retry.");
        return;
      }
      updateExplorer(graphJSON);

      options = getOptions();
      graphData = getGraphData(graphJSON, options);
      Object.assign(globalThis, { graphJSON, graphData });

      // Update options form's className for hint about only one combo
      const classListMethod = graphData.combos.length <= 1 ? "add" : "remove";
      optionsForm?.classList[classListMethod]("only-one-combo");

      console.debug(
        `${filename}:\n`, graphJSON,
        "\ngraphData:\n", graphData,
        "\ngetGraphData() options:", options,
      );

      // Render graph
      resetInstance("Rendering...");
      await renderGraph(graph, graphData);

      const expand = optionsForm?.elements.expand.value === "true";
      const topChildren = graph.getChildrenData(getComboId(graphJSON.top));
      const visibleElements = expand ? graphData.nodes : topChildren;

      // Put edges in front of nodes
      await graph.frontElement(
        graph.getEdgeData()
        .map(({id}) => id)
        .filter(id => typeof id === "string")
      );

      // Run a 2nd layout if amount of visible elements is acceptable
      visibleElements.length >= 10 &&
      visibleElements.length <= 500 &&
      await graph.layout(getLayout());

      // Run translateElementTo() twice to reset position for collapsed combo
      !expand &&
      graph.getChildrenData(getComboId(graphJSON.top)).forEach(item => {
        if (item.type === "circle" && item.style?.collapsed) {
          const position = graph.getElementPosition(item.id);
          void (async () => {
            await graph.translateElementTo(item.id, position, false);
            await graph.translateElementTo(item.id, position, false);
          })();
        }
      });

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
  () => void graph.clear().then(() => {
    filename = "";
    graphJSON = undefined;
    graphData = { nodes: [], edges: [], combos: [] };
    resetSidebar("Please load a file.");
  })
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
  () => void graph.toDataURL({ mode: "overall" }).then(
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
	const sidebar = document.querySelector("aside");
	/** @type { HTMLButtonElement | null } */
	const toggleSidebar = document.querySelector(".btn-toggleSidebar");
	if (sidebar && toggleSidebar) {
		// FIXME: expand main after hide sidebar
		toggleSidebar.addEventListener("click", () => {
			const newValue = sidebar.style.display !== "none" ? "none" : null;
			sidebar.style.setProperty("display", newValue);
		});
		toggleSidebar.disabled = false;
	}

	const dialog = document.querySelector("dialog");
	if (dialog) {
		const closeBtn = dialog.querySelector(":scope .btn-close");
		closeBtn?.addEventListener("click", () => dialog.close());

		const code = dialog.querySelector(":scope > pre > code");
		if (code) {
			const copyBtn = dialog.querySelector(":scope .btn-copy");
			copyBtn?.addEventListener(
				"click",
				() => code.textContent &&
          void navigator.clipboard.writeText(code.textContent),
			);
		}
	}

  /** @type {((states: Record<string, string[]>) => Record<string, string[]>)} */
  const showSelectedNodes = states => {
    const selected = Object.keys(states);
    selected.length > 0 &&
    resetSidebar(`Selected nodes: ${selected.join(", ")}`);
    return states;
  }

  // https://g6.antv.antgroup.com/api/graph/option
  const graph = new Graph({
    ...graphOptions,

    layout: getLayout(),

    behaviors: [
      // "auto-adapt-label",
      "zoom-canvas",
      "drag-element",

      /** drag canvas when Shift or Ctrl are not pressed
       * @type {import("@antv/g6").DragCanvasOptions} */
      ({
        type: "drag-canvas",
        enable: event => {
          if (event.ctrlKey || event.shiftKey) {
            return false;
          } else {
            const defaultEnable = DragCanvas.defaultOptions.enable;
            return typeof defaultEnable === "function"
              ? defaultEnable(event)
              : true;
          }
        }

      }),
      /** Shift + drag: brush select (box selection)
       * @type {import("@antv/g6").BrushSelectOptions} */
      ({
        type: "brush-select",
        trigger: ["shift"],
        mode: "diff",
        enableElements: ["node"],
        onSelect: showSelectedNodes,
      }),
      /** Ctrl + drag: lasso select
       * @type {import("@antv/g6").LassoSelectOptions} */
      ({
        type: "lasso-select",
        trigger: ["control"],
        mode: "diff",
        enableElements: ["node"],
        onSelect: showSelectedNodes,
      }),

      /** Double click to collapse / expand combo
       * @type {import("@antv/g6").CollapseExpandOptions} */
      ({
        type: "collapse-expand",
        animation: false,
        // If combo contains more than 1 children, layout again to avoid G6 bug
        // TODO: fix node with identical position
        onExpand: id =>
          graph.getComboData(id) &&
          graph.getChildrenData(id).length > 1 &&
          void graph.layout(),
      }),

      /** @type {import("@antv/g6").ClickSelectOptions} */
      ({
        type: "click-select",
        degree: 1,
        neighborState: "highlight",
        onClick: ({ target: item }) => {
          if (!("type" in item) || !("id" in item)) { resetSidebar(); return; }
          switch (item.type) {
            case "node":  updateSidebarForNode(item.id, graph); break;
            case "combo": updateSidebarForCombo(item.id, graph); break;
            case "edge":  updateSidebarForEdge(item.id, graph); break;
            default: resetSidebar(); break;
          }
        },
      }),

    ],

  });
  // graph.on();

  globalThis.graph = graph;

  // Graph loading finished, remove loading status in instance sidebar
  resetInstance();

  setupFileInput(graph);
  setupGraphButtons(graph);
  setupRadioToggles(graph);

  console.debug("graph object:\n", graph);
})();
