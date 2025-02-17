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
  const G6: typeof g6; // { [K in keyof typeof g6]: typeof g6[K]; }

  interface NodeData extends Omit<RemoveIndex<g6.NodeData>, "data"> {
    data:
    | { task: UpperTask | LowerTask | undefined, subTask: SubTask; }
    | { task: UpperTask | LowerTask | undefined, subTasks: SubTask[]; };
  }
  interface ComboData extends Omit<RemoveIndex<g6.ComboData>, "data"> {
    data: UpperTask;
  }

  // fix wrong `type` name in @antv/g6/lib/layouts/types.d.ts
  type BuiltInLayoutOptions =
    | Exclude<import("@antv/g6/lib/layouts/types").BuiltInLayoutOptions, ForceAtlas2>
    | ForceAtlas2Fixed;
}

interface ForceAtlas2 extends g6.BaseLayoutOptions, g6.ForceAtlas2LayoutOptions {
  type: "forceAtlas2";
}
interface ForceAtlas2Fixed extends Omit<g6.BaseLayoutOptions, "type">, g6.ForceAtlas2LayoutOptions {
  type: "force-atlas2";
}
