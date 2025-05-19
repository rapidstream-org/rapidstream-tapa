# Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

load("@aspect_rules_webpack//webpack:defs.bzl", "webpack_bundle", "webpack_devserver")
load("@npm//:defs.bzl", "npm_link_all_packages")

npm_link_all_packages(name = "node_modules")

ENTRY = ":src/app.js"

WEBPACK_CONFIG = ":webpack.config.mjs"

SRCS = [
    ":css/button.css",
    ":css/sidebar.css",
    ":css/style.css",
    ":index.html",
    ":src/graph-config.js",
    ":src/helper.js",
    ":src/parser.js",
    ":src/parser/expand.js",
    ":src/parser/fifo.js",
    ":src/parser/ports.js",
    ":src/prism-config.js",
    ":src/sidebar.js",
]

NODE_MODULES = [
    ":node_modules/@antv/g6",
    ":node_modules/css-loader",
    ":node_modules/html-webpack-plugin",
    ":node_modules/lucide",
    ":node_modules/mini-css-extract-plugin",
    ":node_modules/prismjs",
    ":node_modules/tslib",
    ":node_modules/url",
]

webpack_bundle(
    name = "tapa-visualizer",
    srcs = SRCS,
    chdir = "tapa-visualizer",
    entry_point = ENTRY,
    node_modules = ":node_modules",
    output_dir = True,
    visibility = ["//visibility:public"],
    webpack_config = WEBPACK_CONFIG,
    deps = NODE_MODULES,
)

webpack_devserver(
    name = "tapa-visualizer-devserver",
    chdir = "tapa-visualizer",
    data = SRCS + NODE_MODULES,
    entry_point = ENTRY,
    node_modules = ":node_modules",
    webpack_config = WEBPACK_CONFIG,
)
