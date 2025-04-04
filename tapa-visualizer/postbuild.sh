#!/bin/sh

# Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

if [[ -w ./dist/index.html ]]; then
  cd ./dist/

  readonly g6="./assets/g6.min.js"

  wget https://unpkg.com/@antv/g6@5/dist/g6.min.js -o "$g6"
  sed -i.bak -e \
    "s|https://unpkg\.com/@antv/g6@5\.[0-9]\.[0-9]\{2\}/dist/g6\.min\.js|$g6|" \
    ./index.html
else
  echo "Please build the project first with:"
  echo "npm run build"
fi
