// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

import { type Config as SVGOConfig, optimize } from "svgo";
import type { PluginOption, UserConfig } from "vite";
import prismjsPlugin from "vite-plugin-prismjs-plus";

import { join } from "node:path";
import { readFileSync } from "node:fs";

const lucideInlineSVGPlugin = (): PluginOption => {
  const name = "vite-plugin-lucide-inline-svg";

  const svgoConfig: SVGOConfig = {
    // TODO: investgate whether the "path" option is needed here
    plugins: [{
      name: "preset-default",
      params: {
        overrides: {
          // See: https://svgo.dev/docs/plugins/removeViewBox/
          removeViewBox: false,
          // Don't sort class="lucide lucide-xxx" to the last
          sortAttrs: false,
        }
      },
    }],
  };

  let root = "";

  // TODO: apply this replacement in some virtual DOM instead
  // From: <svg use="x"></svg>
  // To:   <svg class="lucide lucide-x" ...>...</svg>
  const transformHtml = (html: string) => html.replace(
    /<svg\s+use="(.+?)"><\/svg>/g,
    (match: string, icon: string) => {
      const path = `node_modules/lucide-static/icons/${icon}.svg`;
      try {
        const svg = readFileSync(join(root, path), "utf-8");
        return optimize(svg, svgoConfig).data;
      } catch (e) {
        console.warn(`${name}: Lucide SVG not exist or failed to read`, e);
        return match;
      }
    },
  );

  return {
    name,
    configResolved: config => { root = config.root ?? ""; },
    transformIndexHtml: transformHtml,
    transform:
      (code, id) => id.endsWith(".html") ? transformHtml(code) : undefined,
  };
};

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
    lucideInlineSVGPlugin(),
  ],
} satisfies UserConfig;
