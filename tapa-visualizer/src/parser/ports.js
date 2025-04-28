/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

/** @type {Placements} */
const placements = {
  x: [
    [0.5],
    [0.25, 0.75],
    [0, 0.5, 1],
    [0, 0.25, 0.75, 1],
    [0, 0.25, 0.5, 0.75, 1],
  ],
  // y:
  istream: 0,
  ostream: 1,
};

/** @type {5} */
const ioPortsLength = 5; // placements[istream / ostream].length;

/** subTask -> ioPorts
 * @type {(subTask: SubTask, ioPorts: IOPorts) => void} */
export const setIOPorts = (subTask, ioPorts) => {
  for (const argName in subTask.args) {
    const { arg, cat } = subTask.args[argName];
    switch (cat) {
      case "istream": ioPorts.istream.push([argName, arg]); break;
      case "ostream": ioPorts.ostream.push([argName, arg]); break;
    }
  }
};

/** ioPorts -> style.ports
 * @type {(ioPorts: IOPorts, style: NodeStyle) => void} */
export const setPortsStyle = (ioPorts, style) => {
  /** @type {import("@antv/g6").NodePortStyleProps[]} */
  let ports = [];
  /** @type {["istream", "ostream"]} */
  (["istream", "ostream"]).forEach(stream => {
    const amount = ioPorts[stream].length;
    if (amount <= ioPortsLength) {
      ports = ports.concat(
        ioPorts[stream].map(
          ([name, key], i) => {
            /** @type {import("@antv/g6").Placement} */
            const placement = [placements.x[amount - 1][i], placements[stream]];
            return { name, key, placement };
          }
        ),
      );
    }
  });
  style.ports = ports;
}
