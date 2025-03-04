// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

import g6 from "@antv/g6";

/** Remove index signatures */
type RemoveIndex<T> = {
  [
    K in keyof T as
    string extends K ? never :
    number extends K ? never :
    symbol extends K ? never : K
  ]: T[K];
};

declare global {
  type g6 = typeof g6; // { [K in keyof typeof g6]: typeof g6[K]; }
  const G6: g6;

  interface NodeData extends Omit<RemoveIndex<g6.NodeData>, "data"> {
    data:
    | { task: Task | undefined, subTask: SubTask; }
    | { task: Task | undefined, subTasks: SubTask[]; };
  }
  interface ComboData extends Omit<RemoveIndex<g6.ComboData>, "data"> {
    data: UpperTask;
  }
}
