// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

import globals from "globals";
import js from "@eslint/js";
import ts from "typescript-eslint";

/** Global ignores
 * Global ignores can't have any other property then ignores (and name)
 * https://eslint.org/docs/latest/use/configure/configuration-files#globally-ignoring-files-with-ignores
 *  @type {Pick<import("eslint").Linter.Config, "name" | "ignores">} */
const globalIgnores = {
  name: "global ignores",
  ignores: [],
};

/** @type {import("eslint").Linter.Config[]} */
const config = [

  globalIgnores,

  {
    name: "languageOptions/globals/browser",
    languageOptions: { globals: globals.browser },
  },
  {
    name: "languageOptions/globals/nodeBuiltin",
    files: ["*.config.{js,mjs,cjs,ts}"],
    languageOptions: { globals: globals.nodeBuiltin },
  },

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
        projectService: true,
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
  }

];

export default config;
