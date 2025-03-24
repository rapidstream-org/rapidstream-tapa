 <!--
Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
-->

# TAPA Visualizer

A TAPA task visualizer built with [AntV G6](https://g6.antv.antgroup.com/).

## Development

Launch a local dev server with:
``` sh
npm run dev
# Served at http://localhost:5173/ by default
```

## Build

Build to `dist/` with:
``` sh
npm run build
./postbuild.sh
```

AntV G6 is load from the `unpkg.com` CDN. It's not bundled as its pre-built
version is more ideal. `postbuild.sh` localize AntV G6 from the CDN,
this step may be skipped if you can access `unpkg.com`.

## Dev Notes

- `@antv/g6` works great, but it's filled with type definition bugs. Fixes in
[`g6.d.ts`](./types/g6.d.ts) amd `patch-package` are used for migration.
