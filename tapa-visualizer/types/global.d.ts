// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

type SubTask = {
  args: Record<string, { arg: string, cat: string; }>;
  step: number;
};

type FIFO = {
  /** [name, id] */
  produced_by?: [string, number];
  /** [name, id] */
  consumed_by?: [string, number];
  depth?: number;
};

type Port = {
  cat: string, name: string, type: string, width: number;
};

type LowerTask = {
  level: "lower", target: string, vendor: string;
  /** HLS C++ code of this task. */
  code: string,
}

type UpperTask = {
  level: "upper", target: string, vendor: string;
  /** A dict mapping child task names to json instance description objects. */
  tasks: Record<string, SubTask[]>;
  /** A dict mapping child fifo names to json FIFO description objects. */
  fifos: Record<string, FIFO>;
  /** A dict mapping port names to Port objects for the current task. */
  ports: Port[],
  /** HLS C++ code of this task. */
  code: string,
}

type GraphJSON = {
  top: string,
  tasks: Record<string, LowerTask | UpperTask>;
  cflags: string[];
};
