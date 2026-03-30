Kayoko
======

Kayoko is a POSIX-compliant, fast, secure, and highly portable Unix-like operating system.

Features
--------

Kayoko has many features, including:
- RBAC (via KAT - Kernel Access Table)
- SMAT (System Manager for Administrative Tools)
- NetBSD-based, and more
  
Testing
-------

On a running Kayoko system:

    $ make -C /usr/tests 


Building
-------
    $ lua ./build.lua coreutils|kernel|init|base|branding

Legal
-----

This project is licensed under the PolyForm Noncommercial License 1.0.0. For OEM usage, please view COPYING.OEM.md.
