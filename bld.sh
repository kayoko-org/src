#!/bin/ksh
# XAI Build Script - POSIX Strict Edition
set -e

# Configuration
. ./.env
# 1. Directory Setup (Stripped of /lib64 Linux-isms)
mkdir -p "$XAI_ROOT/bin" "$XAI_ROOT/sbin" "$XAI_ROOT/lib" "$XAI_ROOT/etc" "$XAI_ROOT/usr"
ln -sf ../lib "$XAI_ROOT/usr/lib"
cp src/cmd/adm/srcmstr "$XAI_ROOT/sbin/init"

REAL_CC="cc"
export CC="$REAL_CC"
export LDFLAGS="-static" 
export CFLAGS="-I$_INST/include"

# 1. Build the "Tools" (Compiler/Linker/Make)
# This only needs to happen once. It prevents all header skew.
if [ ! -d "$NBSD_SRC/.git" ]; then
    echo "--> Cloning NetBSD source..."
    git clone --depth 1 https://github.com/NetBSD/src.git "$NBSD_SRC"
fi

# --- 2. Xai Branding ---
echo "--> Applying Xai Branding to Source"
# Change the OS name reported by uname -s / uname -a
sed -i 's/ostype=NetBSD/ostype=Xai/' "$NBSD_SRC/sys/conf/newvers.sh"
# Update param.h for internal consistency
sed -i 's/"NetBSD"/"Xai"/' "$NBSD_SRC/sys/sys/param.h"

# Create XAI Kernel Config from GENERIC
if [ ! -f "$NBSD_SRC/sys/arch/$ARCH/conf/XAI" ]; then
    cp "$NBSD_SRC/sys/arch/$ARCH/conf/GENERIC" "$NBSD_SRC/sys/arch/$ARCH/conf/XAI"
    sed -i 's/ident[[:space:]]*GENERIC/ident XAI/' "$NBSD_SRC/sys/arch/$ARCH/conf/XAI"
fi

# --- 3. Isolated Build Sequence ---
# We use 'env -i' to strip CFLAGS/CPATH that cause Ncurses conflicts
BUILD_ENV="env -i PATH=$PATH TERM=$TERM HOME=$HOME"

echo "--> Building NetBSD Toolchain (Clean Room)..."
(cd "$NBSD_SRC" && $BUILD_ENV ./build.sh -m "$ARCH" -T "$TOOL_DIR" -O "$OBJ_DIR" -U tools)

echo "--> Building Xai Kernel..."
(cd "$NBSD_SRC" && $BUILD_ENV ./build.sh -m "$ARCH" -T "$TOOL_DIR" -O "$OBJ_DIR" -U kernel=XAI)

echo "--> Building Libc..."
(cd "$NBSD_SRC" && $BUILD_ENV ./build.sh -m "$ARCH" -T "$TOOL_DIR" -O "$OBJ_DIR" -U -o library=libc)

# --- 4. Artifact Extraction ---
echo "--> Extracting Xai Core Binaries"
mkdir -p "$XAI_ROOT/lib"

# Copy Kernel
cp "$OBJ_DIR/sys/arch/$ARCH/compile/XAI/netbsd" "$XAI_ROOT/Xai"
ln -sf Xai "$XAI_ROOT/netbsd"

# Copy Libc (Find the shared object in the build tree)
find "$OBJ_DIR/lib/libc" -name "libc.so*" -exec cp {} "$XAI_ROOT/lib/" \;

# Copy Bootloader (Arch dependent, usually 'boot')
find "$OBJ_DIR/sys/arch/$ARCH/stand" -name "boot" -exec cp {} "$XAI_ROOT/" \; 2>/dev/null || true


# 3. Build OKSH (Static shell)
if [ ! -x "$OKSH_DIR/oksh" ]; then
    [ -d "$OKSH_DIR" ] || git clone https://github.com/ibara/oksh.git "$OKSH_DIR"
    (cd "$OKSH_DIR" && ./configure --enable-static && make -j"$NPROCS")
fi
install -m 755 "$OKSH_DIR/oksh" "$XAI_ROOT/bin/ksh"
# POSIX sh should be ksh in this house
ln -sf ksh "$XAI_ROOT/bin/sh"

# 4. Build Boot & Login
# Note: Ensure bootseq.c uses POSIX headers, no <linux/fs.h>
"$REAL_CC" -O2 -static -o "$XAI_ROOT/sbin/login" src/cmd/adm/login.c
"$REAL_CC" -O2 -static -s -o "$XAI_ROOT/bin/ls" src/cmd/core/ls.c

mkdir -p "$XAI_ROOT/usr/bin"
"$REAL_CC" -O2 -static -o "$XAI_ROOT/usr/bin/hostname" src/cmd/core/hostname.c

# 5. Build SBASE (The Toolchest)
if [ ! -d "$SBASE_DIR" ]; then
    git clone https://git.suckless.org/sbase "$SBASE_DIR"
fi
# Using static LDFLAGS to keep the base tools independent of the shim
(cd "$SBASE_DIR" && make CC="$REAL_CC" LDFLAGS="-static" sbase-box)

# 6. Populate /bin (POSIX tools only)
install -m 755 "$SBASE_DIR/sbase-box" "$XAI_ROOT/bin/.utils"
for tool in cat whoami touch tr true tar grep uname sed awk pwd mkdir cp mv rm; do
    (cd "$XAI_ROOT/bin" && ln -sf .utils "$tool")
done

# 7. Build the AIX Identity Shim & Uname
# We link uname specifically to /lib/libc.so

# 8. Finalize Library Layout
NEW_LIBC=$(ls /usr/lib/libc.so.* | sort -V | tail -n 1)
cp "$NEW_LIBC" "$XAI_ROOT/lib/libc.so"

# 9. Environment Setup
cat <<EOF > "$XAI_ROOT/etc/profile"
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
export HOME=/root
export LD_PRELOAD=/lib/libkstat.so
# AIX-style prompt
export PS1='\$(whoami)@\$(hostname):\$(pwd)> '
EOF

cat <<EOF > "$XAI_ROOT/etc/lppcfg"
# Fileset:Level
bos.rte:7.3.0.0
bos.mp64:7.3.0.0
devices.common.IBM.usb.rte:7.3.0.0
EOF

mkdir -p "$XAI_ROOT"/usr/bin && cp src/cmd/adm/oslevel "$XAI_ROOT"/usr/bin


cat <<'EOF' > "$XAI_ROOT"/etc/os-release
NAME="Xai"
VERSION="7.3"
ID=xai
PRETTY_NAME="Xai 7.3 (bos73F)"
VERSION_ID="7.3"
XAI_SERVICE_PACK="7300-03-02-2546"
XAI_RECOMMENDED_LEVEL="7300-03-00-0000"
XAI_BUILD_DATE="2026-03-25"
EOF

# Build static ncurses for nvi
NCURSES_VER="6.4"
if [ ! -d "$HOME/ncurses-$NCURSES_VER" ]; then
    curl -o ~/ncurses-$NCURSES_VER.tar.gz https://ftp.gnu.org/pub/gnu/ncurses/ncurses-$NCURSES_VER.tar.gz
    tar -xzf ~/ncurses-$NCURSES_VER.tar.gz -C "$HOME"
    (cd "$HOME/ncurses-$NCURSES_VER" && \
     ./configure --prefix="$_INST" \
                 --without-ada --without-tests --without-debug \
                 --enable-widec --enable-static && \
     make -j"$NPROCS" && make install)
fi


cp -Rp "$_INST"/* "$XAI_ROOT/usr/"
