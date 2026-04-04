#!/bin/sh

#
# Kayoko - Source Code
# 
# Copyright (c) 2026 The Kayoko Project. All Rights Reserved
# 
# This file is licensed under the Common Development and Distribution License (CDDL).
# 
# See /usr/src/COPYING for details.
#

# Compile
cc kat.c -o kat

# Make it SetUID root so it can manage process context
chown root kat
chmod 4755 kat
