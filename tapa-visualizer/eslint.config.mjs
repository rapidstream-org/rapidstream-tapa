// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

import { globalIgnores } from "eslint/config";
import globals from "globals";

import css from "@eslint/css";
import js from "@eslint/js";
import ts from "typescript-eslint";

/**
 * @typedef {import("eslint").Linter} Linter
 * @typedef {import("eslint").Linter.Config} Config
 * @typedef {import("eslint").Linter.Globals} Globals
 * @typedef {import("@typescript-eslint/utils").TSESLint.FlatConfig.Config} FlatConfig
 **/

/** @type {(files: string[], globals: Globals, name?: string) => Config} */
const globalVariables = (files, globals, name) => ({
  name,
  files,
  languageOptions: { globals },
});

// Reverse the arguments for clearer code

/** @type {(name: string, ignorePatterns: string[]) => Config} */
const globalIgnoresR =
  (name, ignorePatterns) => globalIgnores(ignorePatterns, name);
/** @type {(name: string, globals: Globals, files: string[]) => Config} */
const globalVariablesR =
  (name, globals, files) => globalVariables(files, globals, name);


/** @type {(Config | FlatConfig)[]} */
export default [

  globalIgnoresR("global ignores", [
    "js/*.min.js",
  ]),

  // Global variables
  globalVariablesR("globals/browser", globals.browser, [
    "js/*.js",
  ]),
  globalVariablesR("globals/nodeBuiltin", globals.nodeBuiltin, [
    "eslint.config.mjs",
    "vite.config.ts",
  ]),

  // JavaScript
  {
    name: "@eslint/js/recommended",
    ...js.configs.recommended,
  },
  {
    name: "@eslint/js/custom",
    rules: {
      'no-undef': "off",
      "no-plusplus": ["error", { allowForLoopAfterthoughts: true }],
      "prefer-object-has-own": "error",
      "prefer-object-spread": "error",
      "prefer-template": "error",
      "sort-imports": ["warn", { allowSeparatedGroups: true }],
      "sort-vars": "warn",
    },
  },

  // TypeScript
  ...ts.configs.recommendedTypeChecked,
  {
    name: "typescript-eslint/custom",
    languageOptions: {
      parserOptions: {
        project: true,
      },
    },
    rules: {
      "@typescript-eslint/naming-convention": [
        "warn",
        {
          selector: "variable",
          format: ["camelCase", "PascalCase", "UPPER_CASE"],
          leadingUnderscore: "allow",
          trailingUnderscore: "allow",
        },
      ],
      "@typescript-eslint/no-unused-expressions": [
        "warn",
        {
          allowShortCircuit: true,
          allowTernary: true,
          enforceForJSX: true,
        },
      ],
      "@typescript-eslint/no-unused-vars": [
        "warn",
        {
          argsIgnorePattern: "^_",
          varsIgnorePattern: "^_",
          caughtErrorsIgnorePattern: "^_|e",
        },
      ],
    },

  },
  {
    name: "tsdef",
    files: ["types/*.d.ts"],
    rules: {
      "no-var": "off",
    }
  },

  // CSS
  {
    name: "css",
    files: ["css/*.css"],
    plugins: { css },
    language: "css/css",
    rules: {
      "css/no-duplicate-imports": "error",
      "css/no-invalid-at-rules": "error",
      "css/no-invalid-properties": "error",
      "css/require-baseline": ["warn", { available: "widely" }]
    },
  },

];
