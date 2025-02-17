// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

type $ = <K extends keyof HTMLElementTagNameMap>(tagName: K, prop?: Record<string, unknown>) => HTMLElementTagNameMap[K];

type GraphData = Required<import("@antv/g6").GraphData>;

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
