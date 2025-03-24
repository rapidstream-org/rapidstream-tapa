/*
 * Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
 * All rights reserved. The contributor(s) of this file has/have agreed to the
 * RapidStream Contributor License Agreement.
 */

"use strict";

// DOM helpers

/** @type {$} */
export const $ = (tagName, props) => Object.assign(
	document.createElement(tagName), props
);

/** @type {$text} */
export const $text = (tagName, textContent) => $(tagName, { textContent });

/**
 * @typedef {(Node | string)[]} Children
 * @type {<T extends HTMLElement>(parent: T, ...children: Children) => T} */
export const append = (parent, ...children) => {
  parent.append(...children);
  return parent;
}

// combo id prefix

/** "name" -> "combo:name"
 * @type {(comboName: string) => string} */
export const getComboId = comboName => `combo:${comboName}`;

/** "combo:name" -> "name"
 * @type {(comboId: string) => string} */
export const getComboName = comboId => comboId.replace(/^combo:/, "");
