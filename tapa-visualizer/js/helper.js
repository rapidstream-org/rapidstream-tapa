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

/** @type {<T extends HTMLElement>(parent: T, ...children: (Node | string)[]) => T} */
export const append = (parent, ...children) => {
  parent.append(...children);
  return parent;
}

// string manipulation

/** @type {(comboId: string) => string} */
export const getComboName = comboId => comboId.replace(/^combo:/, "");

/** @type {(id: string | undefined) => string | undefined} */
export const trimEdgeId = id => {
  // Trim the prefix part
  id = id?.slice(id.indexOf("/") + 1);
  // If still very long, then cap each part's length to 15
  if (id && id.length > 20) {
    id = id.split("/").map(
      part => part.length <= 15 ? part : `${part.slice(1, 12)}...`
    ).join("/");
  }
  return id;
}
