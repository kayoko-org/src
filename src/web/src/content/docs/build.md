---
title: "Building Kayoko"
description: "How to compile and test the Kayoko system using the Lua build orchestrator."
---

# Building Kayoko

Kayoko uses a custom Lua-based orchestrator for highly portable and fast compilation.

## Usage

To build specific parts of the system, run the `build.lua` script followed by your target:

```bash
$ lua ./build.lua <target>

