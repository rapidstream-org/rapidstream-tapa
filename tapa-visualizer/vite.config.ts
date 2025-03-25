// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

import type { UserConfig } from "vite";
import prismjsPlugin from "vite-plugin-prismjs-plus";

import { join } from "node:path";
import { readFileSync } from "node:fs";

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

    (() => {
      const name = "vite-plugin-lucide-inline-svg";
      const lucideStatic = (icon: string) =>
        `node_modules/lucide-static/icons/${icon}.svg`;

      const minifyLucideSVG = (lucideSVG: string) =>
        lucideSVG
          .replace(/<!-- @license lucide-static v[0-9.]+ - ISC -->\n/, "")
          .replace(/\n\s+/g, " ")
          .replaceAll("\n", "")
          .replaceAll("> <", "><")
          .replaceAll(" />", "/>");

      let root = "";

      return {
        name,
        configResolved: config => { root = config.root ?? ""; },
        // <svg use="x"></svg> -> <svg class="lucide lucide-x" ...>...</svg>
        transformIndexHtml: html => html.replace(
          /<svg\s+use="(.+?)"><\/svg>/g,
          (_match, icon: string) => minifyLucideSVG(
            readFileSync(join(root, lucideStatic(icon)), "utf-8")
          ),
        ),
      };
    })(),

  ],
} satisfies UserConfig;
