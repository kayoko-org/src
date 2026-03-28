#!/bin/sh
# Compile
cc kat.c -o kat

# Make it SetUID root so it can manage process context
chown root kat
chmod 4755 kat
