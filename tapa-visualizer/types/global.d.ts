// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

interface JSON {
  /**
   * Converts a JavaScript Object Notation (JSON) string into an object.
   * @param text A valid JSON string.
   * @param reviver A function that transforms the results. This function is
   * called for each member of the object. If a member contains nested objects,
   * the nested objects are transformed before the parent object is.
   */
    parse<T>(
      text: string,
      reviver?: <This, X, Y>(this: This, key: string, value: X) => Y,
    ): T;
}

// Helpers
type $ = <K extends keyof HTMLElementTagNameMap>
  (tagName: K, prop?: Record<string, unknown>) => HTMLElementTagNameMap[K];
type $text = <K extends keyof HTMLElementTagNameMap>
  (tagName: K, textContent: string | number) => HTMLElementTagNameMap[K];

// Globals
declare var graph: Graph;
declare var graphData: GraphData;
declare var graphJSON: GraphJSON;

type Graph = import("@antv/g6").Graph;
type GraphData = Required<import("@antv/g6").GraphData>;

type NodeStyle = import("@antv/g6/lib/spec/element/node").NodeStyle;

type Grouping = "merge" | "separate" | "expand";

/** Options of getGraphData() */
type GetGraphDataOptions = {
  /** Whether expand sub-task to sub-task/0, sub-task/1, ...
   * @default "merge" */
  grouping?: Grouping;
  /** Whether expand non-top upper task's combo by default
   * @default false */
  expand?: boolean;
  /** Toggle ports
   * @default false */
  port?: boolean;
  /** Set node fill color by:
   * - connection count (`> 1`, `<= 1`), or
   * - task level (`upper`, `lower`)
   * @default "auto" */
  // nodeFill?: "auto" | "connection" | "task-level";
};

// Ports
type Placements = {
  x: number[][];
  istream: number;
  ostream: number;
};
/** istream ports and ostream ports from one sub-task */
type IOPorts = {
  istream: [name: string, arg: string][];
  ostream: [name: string, arg: string][];
};

// Forms
interface GroupingFormControls extends HTMLFormControlsCollection {
  /** Sub-task grouping */
  grouping: { value: Grouping; },
}
interface OptionsFormControls extends HTMLFormControlsCollection {
  /** Layout algorithm */
  layout: { value: "force-atlas2" | "antv-dagre"| "dagre"; },
  /** Expand other upper tasks (on graph load) */
  expand: { value: "true" | "false"; },
  /** Show connection ports */
  port: { value: "true" | "false"; },
}
interface GroupingForm extends HTMLFormElement {
  grouping: { value: Grouping; };
  elements: GroupingFormControls;
}
interface OptionsForm extends HTMLFormElement {
  elements: OptionsFormControls;
}

// graph.json

type GraphJSON = {
  top: string,
  /** A dict mapping task names to task object. */
  tasks: Record<string, Task>;
  cflags: string[];
};

type Task = UpperTask | LowerTask;

type UpperTask = {
  level: "upper", target: string, vendor: string;
  /** A dict mapping child task names to instance description objects. */
  tasks: Record<string, SubTask[]>;
  /** A dict mapping child fifo names to FIFO description objects. */
  fifos: Record<string, FIFO>;
  /** A dict mapping port names to Port objects for the current task. */
  ports: Port[],
  /** HLS C++ code of this task. */
  code: string,
}

type LowerTask = {
  level: "lower", target: string, vendor: string;
  /** A dict mapping port names to Port objects for the current task. */
  ports?: Port[],
  /** HLS C++ code of this task. */
  code: string,
}

type SubTask = {
  args: Record<string, { arg: string, cat: string; }>;
  step: number;
};

type FIFO = {
  produced_by?: [name: string, id: number];
  consumed_by?: [name: string, id: number];
  depth?: number;
};

type Port = {
  name: string, cat: string, type: string, width: number;
};
