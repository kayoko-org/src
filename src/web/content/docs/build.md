<!--
Kayoko - Documentation

Copyright (c) 2026 The Kayoko Project. All Rights Reserved

This file is licensed under the Public Documentation License (PDL).

See /usr/src/DOCCOPYING for details.
-->

---
title: Building Kayoko
date: 2026-03-03
description: Documentation for the Kayoko Lua-powered build orchestrator.
---

Kayoko uses a **Lua-powered build orchestrator** for high portability and speed. The system is designed to be modular, allowing you to build individual components or the entire base system.

<p class="text-xl font-light text-kayoko-muted mb-12">
  Kayoko's orchestrator replaces traditional complex Makefiles with a scriptable Lua environment, ensuring builds are reproducible across different host architectures.
</p>

## Quick Start

To begin a build, invoke the Lua interpreter on the main build script followed by your desired target:

    $ lua ./build.lua <coreutils|kernel|init|base|branding>

> **Build Tip**
>
> To add color to your logs, always append `--color=always` to your command string. This is especially helpful for tracking errors during long kernel compilations.

---

## Primary Targets

* **kernel** — Compiles the NetBSD-based core. (Note: You must run the **branding** target before this!)
* **coreutils** — The standard suite of Unix-like tools (`ls`, `cat`, `mkdir`, etc.) specifically written for Kayoko.
* **init** — The system initialization daemon and service manager.
* **base** — A meta-target that builds the kernel, coreutils, and init into a single bootable image.
* **branding** — Applies Kayoko-specific visual identity and strings to the NetBSD kernel source.

---

## Build Environment

The following dependencies must be present on your host system before attempting a build:

| Dependency | Minimum Version / Edition |
| :--- | :--- |
| **Lua** | 5.4+ |
| **C++/CC** | C11 and C++20 Compliant |
| **Make** | GNU Make |
