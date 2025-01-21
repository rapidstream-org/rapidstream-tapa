<!--
Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
All rights reserved. The contributor(s) of this file has/have agreed to the
RapidStream Contributor License Agreement.
-->
# Release Process

## Rebase

```bash
git remote rename origin rapidstream
git remote add origin https://github.com/rapidstream-org/rapidstream-tapa.git
git fetch origin
git rebase origin/main
git push -f rapidstream rapidstream
```

## Build

```bash
./package.sh
```

The output tarball is at `../artifacts.out/1/rapid/rapid.tar.gz`.
