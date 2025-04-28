// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

// const path = require("path");
// const fs = require("fs");

import { createRequire as R } from "node:module";

import HtmlWebpackPlugin from "html-webpack-plugin";
import MiniCssExtractPlugin from "mini-css-extract-plugin";

const input = {
  html: "./index.html",
  js: "./src/app.js",
};

/** @type {import("webpack").Configuration["output"]} */
const output = {
  filename: "bundle.js",
};

/** @type {import("webpack").Configuration} */
export default {
  entry: input.js,
  output,

  module: {
    rules: [
      {
        test: /\.css$/,
        use: [ MiniCssExtractPlugin.loader, "css-loader" ],
      },
    ],
  },
  plugins: [
    new HtmlWebpackPlugin({ template: input.html }),
    new MiniCssExtractPlugin()
  ],
  resolve: {
    fallback: {
      "url": (import.meta.resolve ?? R(import.meta.url).resolve)("url/"),
    }
  },

  performance: { // 1024 * 1024 * 2 = 2 MiB
    maxAssetSize: 2097152,
    maxEntrypointSize: 2097152,
  },
  devtool: "source-map",
};
