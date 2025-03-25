// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

import type { UserConfig } from "vite";
import prismjsPlugin from "vite-plugin-prismjs-plus";

export default {
  build: {
    target: "es2023",
    sourcemap: true,
  },
  plugins: [
    prismjsPlugin({
      manual: true,
      css: true,
      languages: ["cpp"],
      plugins: ["line-numbers"],
    }),
  ],
} satisfies UserConfig;
