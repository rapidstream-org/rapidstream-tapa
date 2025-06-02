/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

import { getComboName } from "./helper.js";

/** @satisfies {HTMLElement | null} */
const container = document.querySelector(".graph-container");
if (container === null) throw new TypeError("container is null!");

// Based on Bootstrap Color; H = Highlight
export const color = Object.freeze({
  nodeA: "#198754", // $green-500
  nodeB: "#0F5132", // $green-700
  nodeH: "#20c997", // $teal-500
  edgeA: "#A3CFBB", // $green-200
  edgeB: "#479f76", // $green-400
  edgeH: "#20c997",
});

/** @type {import("@antv/g6").CanvasOptions} */
const canvasOptions = {
  container,
  autoResize: true,
};

/** @type {import("@antv/g6").ViewportOptions} */
const viewportOptions = {
  autoFit: {
    type: "view",
    options: { when: "overflow" },
    animation: { duration: 100 }, // ms
  },
  padding: 20, // px
  zoomRange: [0.1, 2.5], // * 100%
};

/** @type {(id: string | undefined) => string | undefined} */
const trimEdgeId = id => {
  // Remove the prefix part
  id = id?.slice(id.indexOf("/") + 1);
  // If still very long, then cap each part's length to 15
  if (id && id.length > 20) {
    id = id.split("/").map(
      part => part.length <= 15 ? part : `${part.slice(1, 12)}...`
    ).join("/");
  }
  return id;
};

/** @type {NodeStyle} */
const nodeActiveState = {
  halo: true,
  strokeOpacity: 1,
  stroke: color.nodeH,
  haloStroke: color.nodeH,
};

/** @type {Pick<import("@antv/g6").GraphOptions, "node" | "edge" | "combo">} */
const elementOptions = {
  // https://g6.antv.antgroup.com/api/elements/nodes/base-node
  node: {
    type: "rect",
    style: {
      size: [120, 40],
      radius: 2,
      fill: ({ style }) => style?.fill ?? color.nodeA,

      portR: 5,
      portFill: color.nodeA,

      labelPlacement: "center",
      labelWordWrap: true, // enable label ellipsis
      labelMaxWidth: 100,
      labelMaxLines: 2,
      labelFill: "white",
      labelFontWeight: "bold",
      labelFontFamily: "monospace",
      labelText: ({ id }) => id,
    },
    // Builtin states: "selected" "active" "inactive" "disabled" "highlight"
    state: {
      // selected (degree 0): stroke + halo
      selected: {
        ...nodeActiveState,
        lineWidth: 4,
        haloLineWidth: 12,
      },
      // highlight (degree 1): stroke only
      highlight: {
        ...nodeActiveState,
        lineWidth: 2,
        haloLineWidth: 6,
      },
    },
  },
  edge: {
    style: {
      stroke: ({ style }) => style?.stroke ?? color.edgeA,
      endArrow: true,

      halo: true,
      haloStrokeOpacity: .25,
      haloLineWidth: 2,

      labelBackground: true,
      labelBackgroundFill: "white",
      labelBackgroundFillOpacity: 0.75,
      labelBackgroundRadius: 1,
      labelFontFamily: "monospace",
      labelText: ({ id }) => trimEdgeId(id),
    },
    state: {
      selected: {
        labelFontSize: 12,
        labelFontWeight: "normal",
        stroke: color.edgeH,
        lineWidth: 3,
        haloLineWidth: 9,
      },
      highlight: {
        labelFontWeight: "normal",
        stroke: color.edgeH,
        lineWidth: 2,
        haloLineWidth: 6,
      },
    }
  },
  combo: {
    type: "rect",
    style: {
      fill: "#13795B",
      stroke: "#13795B",
      haloStroke: "#13795B",
      collapsedFillOpacity: 0.75,
      collapsedMarkerFontSize: 16,

      labelFill: "gray",
      labelPlacement: "top",
      labelFontFamily: "monospace",
      labelOffsetY: -5,
      labelText: ({ id }) => getComboName(id),
    },
    state: {
      selected: {
        labelFontSize: 12,
        lineWidth: 3,
        halo: true,
        haloLineWidth: 9,
        haloStrokeOpacity: 0.25,
      },
    },
  },
}

/**
 * @type      {import("@antv/g6").SingleLayoutOptions}
 * @satisfies {import("@antv/g6").AntVDagreLayoutOptions} */
export const antvDagre = {
  type: "antv-dagre",
  // rankdir: "LR",
  ranksep: 60,
  nodeSize: [120, 40],
  nodesep: 20,
  // ranker: "network-simplex",
};

/**
 * @type      {import("@antv/g6").SingleLayoutOptions}
 * @satisfies {import("@antv/g6").DagreLayoutOptions} */
export const dagre = {
  type: "dagre",
  nodeSize: [160, 120],
};

/** `force-atlas2` specifc config for large graph:
 * - `kr`:    roughly match up with nodeSize
 * - `kg`:    much smaller than kr; larger than 1 to work with small graph
 * - `ks`:    speed, at least 0.01 * nodeSize
 * - `ksmax`: max speed, not very important
 * - `tao`:   10 is the sweet point
 * @type      {import("@antv/g6").SingleLayoutOptions}
 * @satisfies {import("@antv/g6").ForceAtlas2LayoutOptions} */
export const forceAtlas2 = {
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

/** @type {(items: import("@antv/g6").ElementDatum[]) => HTMLElement | string} */
const getContentForTooltip = items =>
  items
  .filter(item => !item.id?.startsWith("combo:"))
  .map(item => {
    if (!item.id) {
      return "";
    } else if (
      typeof item.source === "string" &&
      typeof item.target === "string"
    ) {
      // Edge
      return [
        `Edge ID: <code>${item.id}</code>`,
        `Source: <code>${item.source}</code>`,
        `Target: <code>${item.target}</code>`,
      ].join("<br>");
    } else {
      // Node
      const [taskName, subTaskIndex, treeIndex] = item.id.split("/");
      const lines = [`Node ID: <code>${item.id}</code>`];

      if (subTaskIndex) {
        lines.push(
          `- Task Name: <code>${taskName}</code>`,
          `- Sub-Task Index: ${subTaskIndex}`
        );
        treeIndex &&
        lines.push(`- Expand Task Tree Index: ${treeIndex}`);
      }

      return lines.join("<br>");
    }
  })
  .join("<br>");

/** @type {Omit<import("@antv/g6").GraphOptions, "data">} */
export const graphOptions = {
  ...canvasOptions,
  ...viewportOptions,
  ...elementOptions,

  theme: "light",

  animation: { duration: 250 }, // ms

  layout: forceAtlas2,

  // behaviors are defined in graph.js since they call the graph object.

  plugins: [
    /** @type {import("@antv/g6").TooltipOptions} */
    ({
      type: "tooltip",
      getContent: (_e, items) => Promise.resolve(getContentForTooltip(items)),
    }),
  ],

  transforms: ["process-parallel-edges"],

};
