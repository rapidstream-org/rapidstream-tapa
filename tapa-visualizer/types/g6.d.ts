// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

import _G6, { BaseLayoutOptions, ForceAtlas2LayoutOptions } from "@antv/g6";
import { BuiltInLayoutOptions as _BuiltInLayoutOptions } from "@antv/g6/lib/layouts/types";

declare global {
  const G6: typeof _G6;

  // fix wrong `type` name in @antv/g6/lib/layouts/types.d.ts
  interface ForceAtlas2 extends BaseLayoutOptions, ForceAtlas2LayoutOptions {
    type: "forceAtlas2";
  }
  interface ForceAtlas2Fixed extends BaseLayoutOptions, ForceAtlas2LayoutOptions {
    type: "force-atlas2";
  }
  type BuiltInLayoutOptions = Exclude<_BuiltInLayoutOptions, ForceAtlas2> | ForceAtlas2Fixed;
}
