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
mkdir -p "$KAYAKO_ROOT/bin" "$KAYAKO_ROOT/sbin" "$KAYAKO_ROOT/lib" "$KAYAKO_ROOT/etc" "$KAYAKO_ROOT/usr"
ln -sf ../lib "$KAYAKO_ROOT/usr/lib"
cp src/cmd/adm/srcmstr "$KAYAKO_ROOT/sbin/init"

# 1. Force Resolve and Export Paths
# Make sure these are absolute paths and exported for sub-shells
export KAYAKO_ROOT="${KAYAKO_ROOT:-$(pwd)/root}"
export OBJ_DIR="${OBJ_DIR:-$(pwd)/root/obj}"
export TOOL_DIR="${TOOL_DIR:-$(pwd)/root/tools}"
export DESTDIR="$OBJ_DIR/destdir.$ARCH"


REAL_CC="cc"
export CC="$REAL_CC"
export LDFLAGS="-static -lutil"  
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
    echo "--> Cloning Kayako gate..."
    git clone --depth 1 https://github.com/Kayako/gate.git "$NBSD_SRC"
fi
# --- 2. Kayako Branding ---
# --- 1. Build the "Tools" ---
if [ ! -x "$NBMAKE" ]; then
    echo "--> Tools missing. Building NetBSD Toolchain..."
    
    # We UNSET the flags that interfere with the toolchain's internal tests
    (cd "$NBSD_SRC" && env -i \
        PATH="/usr/bin:/bin:/usr/X11R7/bin:/usr/pkg/bin:/usr/pkg/sbin:/usr/local/bin" \
        TERM="$TERM" \
        HOME="$HOME" \
        USER="$USER" \
        ./build.sh -m "$ARCH" -T "$TOOL_DIR" -O "$OBJ_DIR" -u tools)
fi

# --- 2. Branding (Only if changed) ---
# We use a marker file to avoid unnecessary sed runs
if [ ! -f "$NBSD_SRC/.kayako_branded" ]; then
    echo "--> Applying Kayako Branding..."
    sed -i 's/ost="NetBSD"/ost="Kayako"/' "$NBSD_SRC"/sys/conf/newvers.sh
    touch "$NBSD_SRC/.kayako_branded"
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
    $NBMAKE $FLAGS -j"$NPROCS" all) # We skip 'install' to manage KAYAKO_ROOT manually

# --- 4. Artifact Extraction ---
# Use 'cp -u' (update) if your system supports it, or just standard cp
# because copying is fast compared to compiling.
cp -P "$OBJ_DIR/lib/libc/libc.so"* "$KAYAKO_ROOT/lib/"
cp "$OBJ_DIR/lib/libc/libc.a" "$KAYAKO_ROOT/lib/"

# --- 6. Final Header Sync ---
echo "--> Syncing Headers to KAYAKO_ROOT..."
(cd "$NBSD_SRC" && $NBMAKE DESTDIR="$KAYAKO_ROOT" $FLAGS includes)

# --- 3.5 Build the Kernel ---
# Note: We use the custom "KAYAKO" config name here
if [ ! -f "$OBJ_DIR/sys/arch/$ARCH/compile/KAYAKO/netbsd" ]; then
    echo "--> Building Custom Kayako Kernel..."
    # If you haven't created a 'KAYAKO' config file yet, we'll use GENERIC as a base
    if [ ! -f "$NBSD_SRC/sys/arch/$ARCH/conf/KAYAKO" ]; then
        cp "$NBSD_SRC/sys/arch/$ARCH/conf/GENERIC" "$NBSD_SRC/sys/arch/$ARCH/conf/KAYAKO"
    fi
    
    (cd "$NBSD_SRC" && ./build.sh -m "$ARCH" -T "$TOOL_DIR" -O "$OBJ_DIR" kernel=KAYAKO)
fi

# Kernel (Incremental if already built in obj)
if [ -f "$OBJ_DIR/sys/arch/$ARCH/compile/KAYAKO/netbsd" ]; then
    cp "$OBJ_DIR/sys/arch/$ARCH/compile/KAYAKO/netbsd" "$KAYAKO_ROOT/unix"
fi



# 3. Build OKSH (Static shell)
if [ ! -x "$OKSH_DIR/oksh" ]; then
    [ -d "$OKSH_DIR" ] || git clone https://github.com/ibara/oksh.git "$OKSH_DIR"
    (cd "$OKSH_DIR" && ./configure --enable-static && make -j"$NPROCS")
fi
install -m 755 "$OKSH_DIR/oksh" "$KAYAKO_ROOT/bin/ksh"
# POSIX sh should be ksh in this house
ln -sf ksh "$KAYAKO_ROOT/bin/sh"

# 4. Build Boot & Login
# Note: Ensure bootseq.c uses POSIX headers, no <linux/fs.h>
# --- 4. Build Kayako Core Utilities ---
echo "--> Building Kayako Core Utilities..."
for src_file in src/cmd/core/*.c; do
    # 1. Strip the directory path and extension
    base_name=$(basename "$src_file")
    bin_name="${base_name%.c}"

    # 2. Determine the target directory
    case "$bin_name" in
        ls|cp|mv|rm|cat|echo|pwd|mkdir|rmdir|chmod|chgrp|ed)
            DEST_DIR="$KAYAKO_ROOT/bin"
            ;;
        *)
            DEST_DIR="$KAYAKO_ROOT/usr/bin"
            ;;
    esac

    echo "	[CC] $base_name -o $DEST_DIR/$bin_name"

    # 3. Compile as a static binary
    # Note: Ensure $KAYAKO_ROOT/usr/bin exists before running this!
    "$REAL_CC" $CFLAGS -O2 -static -s -o "$DEST_DIR/$bin_name" "$src_file" $LDFLAGS
    "$KAYAKO_ROOT/usr/bin/ln" -s "$KAYAKO_ROOT/bin/ed" "$KAYAKO_ROOT/bin/red"
done
echo "--> Building Kayako Administrative Utilities..."
for src_file in src/cmd/adm/*.c; do
    # 1. Extract the base name (e.g., 'sysmgr')
    base_name=$(basename "$src_file" .c)

    # 2. Skip special cases that need custom linking (we handle these later)
    if [ "$base_name" = "login" ]; then
        continue
    fi

    if [ "$base_name" = "lsdsk" ]; then
        continue
    fi
   
    if [ "$base_name" = "sysmgr" ]; then
        continue
    fi

    echo "  [CC] $base_name -o $DEST_DIR/$bin_name"

    # 3. Standard compilation
    "$REAL_CC" $CFLAGS -O2 -static -s \
        -o "$KAYAKO_ROOT/sbin/$base_name" \
        "$src_file" $LDFLAGS
done

# 4. Handle Special Case: sysmgr (needs -lcurses -lterminfo)
echo "  [CC] lsdsk (special)"
"$REAL_CC" $CFLAGS -O2 -static -s \
    -o "$KAYAKO_ROOT/bin/lsdsk" \
    src/cmd/adm/lsdsk.c \
    -lprop -lutil $LDFLAGS && chflags schg $KAYAKO_ROOT/bin/lsdsk


echo "  [CC] login (special)"
"$REAL_CC" $CFLAGS -O2 -static -s \
    -o "$KAYAKO_ROOT/sbin/login" \
    src/cmd/adm/login.c \
    -lcrypt -lutil $LDFLAGS && chflags schg $KAYAKO_ROOT/sbin/login


echo "  [CC] sysmgr (special)"
"$REAL_CC" $CFLAGS -O2 -static -s \
    -o "$KAYAKO_ROOT/sbin/sysmgr" \
    src/cmd/adm/sysmgr.c \
    -lcurses -lterminfo $LDFLAGS


# 9. Environment Setup
cat <<EOF > "$KAYAKO_ROOT/etc/profile"
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
export HOME=/root
export LD_PRELOAD=/lib/libkstat.so
# AIX-style prompt
export PS1='\$(whoami)@\$(hostname):\$(pwd)> '
EOF

cat <<EOF > "$KAYAKO_ROOT/etc/lppcfg"
# Fileset:Level
bos.rte:7.3.0.0
bos.mp64:7.3.0.0
devices.common.IBM.usb.rte:7.3.0.0
EOF

mkdir -p "$KAYAKO_ROOT"/usr/bin && cp src/cmd/adm/oslevel "$KAYAKO_ROOT"/usr/bin


cat <<'EOF' > "$KAYAKO_ROOT"/etc/os-release
NAME="openkayako"
VERSION="7.3"
ID=kayako
PRETTY_NAME="Kayako"
VERSION_ID="7.3"
KAYAKO_SERVICE_PACK="7300-03-02-2546"
KAYAKO_RECOMMENDED_LEVEL="7300-03-00-0000"
KAYAKO_BUILD_DATE="2026-03-25"
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


cp -Rp "$_INST"/* "$KAYAKO_ROOT/usr/"

echo "--> Build Complete."

sleep 2

print "--> Building bootloader (EFI)"

(cd "$NBSD_SRC/sys/arch/i386/stand/efiboot/bootx64" && \
    $NBMAKE obj && $NBMAKE depend && $NBMAKE && \
    cp -v "$OBJ_DIR/sys/arch/i386/stand/efiboot/bootx64/bootx64.efi" "$EFI_STAGING/EFI/BOOT/BOOTX64.EFI")

# --- 11. Manual UEFI Image Generation (No Infra) ---
# --- Variables for GPT Image Construction ---

# The final output filename (UEFI branded)
KAYAKO_USB_FINAL="$OBJ_DIR/kayako-uefi-$(date +%Y%m%d).img"

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
dd if=/dev/zero of="$KAYAKO_USB_FINAL" bs=1m count=2500

"$TOOL_DIR/bin/nbgpt" "$KAYAKO_USB_FINAL" create
"$TOOL_DIR/bin/nbgpt" "$KAYAKO_USB_FINAL" add -t efi -s 32m
"$TOOL_DIR/bin/nbgpt" "$KAYAKO_USB_FINAL" add -t ffs -i 2

# 5. Extract offsets safely
# We use 'nbgpt show' and parse it carefully
EFI_START=$("$TOOL_DIR/bin/nbgpt" "$KAYAKO_USB_FINAL" show | grep "EFI System" | awk '{print $1}')
ROOT_START=$("$TOOL_DIR/bin/nbgpt" "$KAYAKO_USB_FINAL" show | grep "NetBSD FFS" | awk '{print $1}')

# Check if we actually got numbers back to avoid 'no value specified for seek'
if [ -z "$EFI_START" ] || [ -z "$ROOT_START" ]; then
    echo "ERROR: Could not determine GPT offsets."
    exit 1
fi

echo "--> Writing partitions at offsets: EFI=$EFI_START, ROOT=$ROOT_START"
dd if="$EFI_IMG" of="$KAYAKO_USB_FINAL" bs=512 seek="$EFI_START" conv=notrunc
dd if="$ROOT_IMG" of="$KAYAKO_USB_FINAL" bs=512 seek="$ROOT_START" conv=notrunc

echo "--> $KAYAKO_USB_FINAL"

