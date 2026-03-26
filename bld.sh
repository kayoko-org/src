#!/bin/ksh
# ----------------------------------------------------------------
# NOTICE: This script is part of a proprietary build process.
# Unauthorized reproduction or distribution is strictly prohibited.
# ----------------------------------------------------------------
set -e

clear
print "This information system is for authorized use only."
print "Users have no explicit or implicit expectation of privacy."
print "All files and data on this system may be intercepted, recorded,"
print "read, copied, and disclosed by and to authorized personnel."
print "Unauthorized use may be subject to administrative or legal action."
print "By continuing, you acknowledge and consent to these terms."
sleep 5
# Configuration
. ./.env
# 1. Directory Setup (Stripped of /lib64 Linux-isms)
mkdir -p "$XAI_ROOT/bin" "$XAI_ROOT/sbin" "$XAI_ROOT/lib" "$XAI_ROOT/etc" "$XAI_ROOT/usr"
ln -sf ../lib "$XAI_ROOT/usr/lib"
cp src/cmd/adm/srcmstr "$XAI_ROOT/sbin/init"

# 1. Force Resolve and Export Paths
# Make sure these are absolute paths and exported for sub-shells
export XAI_ROOT="${XAI_ROOT:-$(pwd)/root}"
export OBJ_DIR="${OBJ_DIR:-$(pwd)/root/obj}"
export TOOL_DIR="${TOOL_DIR:-$(pwd)/root/tools}"
export DESTDIR="$OBJ_DIR/destdir.$ARCH"


REAL_CC="cc"
export CC="$REAL_CC"
export LDFLAGS="-static" 
export CFLAGS="-I$_INST/include"
export BUILD_ENV="env -i PATH=$PATH TERM=$TERM HOME=$HOME"
export NBMAKE="$TOOL_DIR/bin/nbmake-$ARCH"
export FLAGS="DESTDIR=$DESTDIR UNPRIVILEGED=no MKUNPRIVILEGED=no"

# 2. Safety Check: If DESTDIR is still empty or somehow root, ABORT.
if [ -z "$DESTDIR" ] || [ "$DESTDIR" = "/" ]; then
    echo "ERROR: DESTDIR is invalid ($DESTDIR). Aborting for system safety."
    exit 1
fi

# 3. Now define FLAGS using the resolved variables
export FLAGS="DESTDIR=$DESTDIR UNPRIVILEGED=no MKUNPRIVILEGED=no"
export NBMAKE="$TOOL_DIR/bin/nbmake-$ARCH"

# 1. Build the "Tools" (Compiler/Linker/Make)

# This only needs to happen once. It prevents all header skew.
if [ ! -d "$NBSD_SRC/.git" ]; then
    echo "--> Cloning NetBSD source..."
    git clone --depth 1 https://github.com/NetBSD/src.git "$NBSD_SRC"
fi

# --- 2. Xai Branding ---
if [ ! -x "$NBMAKE" ]; then
    echo "--> Tools missing. Building NetBSD Toolchain..."
    (cd "$NBSD_SRC" && ./build.sh -m "$ARCH" -T "$TOOL_DIR" -O "$OBJ_DIR" tools)
fi

# --- 2. Branding (Only if changed) ---
# We use a marker file to avoid unnecessary sed runs
if [ ! -f "$NBSD_SRC/.xai_branded" ]; then
    echo "--> Applying Xai Branding..."
    sed -i 's/ost="NetBSD"/ost="XAI"/' "$NBSD_SRC"/src/sys/conf/newvers.sh
    touch "$NBSD_SRC/.xai_branded"
fi

# Helper function for incremental component builds
build_component() {
    dir=$1
    echo "--> Building $dir..."
    (cd "$NBSD_SRC/$dir" && \
        [ -d "obj" ] || $NBMAKE $FLAGS obj && \
        [ -f "obj/.depend" ] || $NBMAKE $FLAGS depend && \
        $NBMAKE $FLAGS -j"$NPROCS" all && \
        $NBMAKE $FLAGS install)
}

# --- 3. The Build Chain ---

# Headers and Dirs (Fast, but necessary)
(cd "$NBSD_SRC/etc" && $NBMAKE $FLAGS distrib-dirs)
(cd "$NBSD_SRC" && $NBMAKE $FLAGS includes)

# Core Components
build_component "lib/csu"
build_component "external/gpl3/gcc/lib/libgcc"

# Libc (The heavy lifter)
echo "--> Updating Libc..."
(cd "$NBSD_SRC/lib/libc" && \
    [ -d "obj" ] || $NBMAKE $FLAGS obj && \
    [ -f "obj/.depend" ] || $NBMAKE $FLAGS depend && \
    $NBMAKE $FLAGS -j"$NPROCS" all) # We skip 'install' to manage XAI_ROOT manually

# --- 4. Artifact Extraction ---
# Use 'cp -u' (update) if your system supports it, or just standard cp
# because copying is fast compared to compiling.
cp -P "$OBJ_DIR/lib/libc/libc.so"* "$XAI_ROOT/lib/"
cp "$OBJ_DIR/lib/libc/libc.a" "$XAI_ROOT/lib/"

# Kernel (Incremental if already built in obj)
if [ -f "$OBJ_DIR/sys/arch/$ARCH/compile/XAI/netbsd" ]; then
    cp "$OBJ_DIR/sys/arch/$ARCH/compile/XAI/netbsd" "$XAI_ROOT/unix"
fi

# --- 6. Final Header Sync ---
echo "--> Syncing Headers to XAI_ROOT..."
(cd "$NBSD_SRC" && $NBMAKE DESTDIR="$XAI_ROOT" $FLAGS includes)

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

echo "--> Build Complete."

sleep 2

print "--> Building bootloader (EFI)"

(cd "$NBSD_SRC/sys/arch/i386/stand/efiboot/bootx64" && \
    $NBMAKE obj && $NBMAKE depend && $NBMAKE && \
    cp -v "$OBJ_DIR/sys/arch/i386/stand/efiboot/bootx64/bootx64.efi" "$EFI_STAGING/EFI/BOOT/BOOTX64.EFI")

# --- 11. Manual UEFI Image Generation (No Infra) ---
# --- Variables for GPT Image Construction ---

# The final output filename (UEFI branded)
XAI_USB_FINAL="$OBJ_DIR/xai-uefi-$(date +%Y%m%d).img"

# Intermediate partition files (the "bricks" for the image)
EFI_IMG="$OBJ_DIR/efi.part"
ROOT_IMG="$OBJ_DIR/root.part"

# Path to the cross-compiled GPT tool
NBGPT="$TOOL_DIR/bin/nbgpt"

# Ensure OBJ_DIR exists
mkdir -p "$OBJ_DIR"

# 4. Combine into a GPT Disk Image
echo "--> Initializing GPT Image..."
# Create a 2.5GB blank file using dd since truncate is missing
dd if=/dev/zero of="$XAI_USB_FINAL" bs=1m count=2500

"$TOOL_DIR/bin/nbgpt" "$XAI_USB_FINAL" create
"$TOOL_DIR/bin/nbgpt" "$XAI_USB_FINAL" add -t efi -s 32m
"$TOOL_DIR/bin/nbgpt" "$XAI_USB_FINAL" add -t ffs -i 2

# 5. Extract offsets safely
# We use 'nbgpt show' and parse it carefully
EFI_START=$("$TOOL_DIR/bin/nbgpt" "$XAI_USB_FINAL" show | grep "EFI System" | awk '{print $1}')
ROOT_START=$("$TOOL_DIR/bin/nbgpt" "$XAI_USB_FINAL" show | grep "NetBSD FFS" | awk '{print $1}')

# Check if we actually got numbers back to avoid 'no value specified for seek'
if [ -z "$EFI_START" ] || [ -z "$ROOT_START" ]; then
    echo "ERROR: Could not determine GPT offsets."
    exit 1
fi

echo "--> Writing partitions at offsets: EFI=$EFI_START, ROOT=$ROOT_START"
dd if="$EFI_IMG" of="$XAI_USB_FINAL" bs=512 seek="$EFI_START" conv=notrunc
dd if="$ROOT_IMG" of="$XAI_USB_FINAL" bs=512 seek="$ROOT_START" conv=notrunc

echo "--> $XAI_USB_FINAL"

