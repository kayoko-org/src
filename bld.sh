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
BUILD_ENV="env -i PATH=$PATH TERM=$TERM HOME=$HOME"
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

# --- 3. Incremental Build Sequence ---
NBMAKE="$TOOL_DIR/bin/nbmake-$ARCH"
DESTDIR="$OBJ_DIR/destdir.$ARCH"

# 1. Tools Check
if [ ! -x "$NBMAKE" ]; then
    echo "--> Tools missing. Building NetBSD Toolchain..."
    (cd "$NBSD_SRC" && ./build.sh -m "$ARCH" -T "$TOOL_DIR" -O "$OBJ_DIR" tools)
fi

# 2. Prepare System Root
mkdir -p "$DESTDIR"

# 1. This creates the /usr/include/arpa etc. tree
(cd "$NBSD_SRC/etc" && \
    $NBMAKE DESTDIR="$DESTDIR" distrib-dirs UNPRIVILEGED=no MKUNPRIVILEGED=no)


# 3. Populate Headers 
# We explicitly tell nbmake that we are NOT in unprivileged mode
echo "--> Installing headers..."
(cd "$NBSD_SRC" && \
    $NBMAKE includes DESTDIR="$DESTDIR" UNPRIVILEGED=no MKUNPRIVILEGED=no)

echo "--> Building C Startup (CSU) objects..."
(cd "$NBSD_SRC/lib/csu" && \
    $NBMAKE obj && \
    $NBMAKE depend && \
    $NBMAKE -j"$NPROCS" $COMMON_FLAGS && \
    $NBMAKE install $COMMON_FLAGS)

# 4. Libc Increment
echo "--> Updating Libc..."
(cd "$NBSD_SRC/lib/libc" && \
    $NBMAKE obj && \
    $NBMAKE depend && \
    $NBMAKE -j"$NPROCS" DESTDIR="$DESTDIR" UNPRIVILEGED=no MKUNPRIVILEGED=no)

# Copy Kernel
cp "$OBJ_DIR/sys/arch/$ARCH/compile/XAI/netbsd" "$XAI_ROOT/unix"

# Copy Libc specifically from the OBJ tree
# NetBSD puts built libs in the OBJDIR equivalent of the source path
LIBC_OBJ_DIR="$OBJ_DIR/lib/libc"
find "$LIBC_OBJ_DIR" -name "libc.so*" -exec cp -P {} "$XAI_ROOT/lib/" \;
find "$LIBC_OBJ_DIR" -name "libc.a" -exec cp {} "$XAI_ROOT/lib/" \;

# Optional: If you want to compile against your new XAI libc later, 
# you'll need the headers in your XAI_ROOT
echo "--> Installing System Headers to XAI_ROOT..."
(cd "$NBSD_SRC" && $BUILD_ENV ./build.sh -m "$ARCH" -T "$TOOL_DIR" -O "$OBJ_DIR" -U installincludes DESTDIR="$XAI_ROOT")

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
