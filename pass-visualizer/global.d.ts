// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

import _G6 from "@antv/g6";

export type ModuleGraph = {
  entities: Entity[];
  connections: Connection[];
};

export type Entity = SubmoduleGraph | PortEntity | InterfaceEntity;
export type SubmoduleGraph = {
  type: "submodule";
  name: string;
  module: string;
  graph: ModuleGraph;
};
export type PortEntity = {
  type: "port";
  name: string;
};
export type InterfaceEntity = {
  type: "interface";
  name: string;
};

export type Connection = {
  sourceEntity: string;
  targetEntity: string;
  sourceIface?: string;
  targetIface?: string;
  identifier: string;
};

declare global {
  const G6: typeof _G6;
}
