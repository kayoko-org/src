#!/bin/ksh
# XAI Build Script - POSIX Strict Edition
set -e

# Configuration
XAI_ROOT="$(pwd)/root"
SBASE_DIR="$HOME/sbase"
OKSH_DIR="$HOME/oksh"
_VER="1.2.5"
_INST="$(pwd)/inst"
# 0. POSIX check for parallelism
# Replaces GNU 'nproc'
NPROCS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)

# 1. Directory Setup (Stripped of /lib64 Linux-isms)
mkdir -p "$XAI_ROOT/bin" "$XAI_ROOT/sbin" "$XAI_ROOT/lib" "$XAI_ROOT/etc" "$XAI_ROOT/usr"
ln -sf ../lib "$XAI_ROOT/usr/lib"
cp srcmstr "$XAI_ROOT/sbin/init"

REAL_CC="cc"
export CC="$REAL_CC"
export LDFLAGS="-static" 
export CFLAGS="-I$_INST/include"


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
"$REAL_CC" -O2 -static -o "$XAI_ROOT/sbin/login" login.c
"$REAL_CC" -O2 -static -o "$XAI_ROOT/bin/ls" ls.c
mkdir -p "$XAI_ROOT/usr/bin"
"$REAL_CC" -O2 -static -o "$XAI_ROOT/usr/bin/hostname" hostname.c

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
LIBC="/usr/lib/libc.so.102.0"
cp "$LIBC" "$XAI_ROOT/lib/libc.so"

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

mkdir -p "$XAI_ROOT"/usr/bin && cp oslevel "$XAI_ROOT"/usr/bin


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


if [ ! -x "$XAI_ROOT/usr/bin/vi" ]; then
    [ -d "$HOME/neatvi" ] || git clone https://github.com/aligrudi/neatvi.git "$HOME/neatvi"
    
    (cd "$HOME/neatvi" && \
     "$CC" -static -O2 -o vi vi.c ex.c lbuf.c mot.c sbuf.c ren.c dir.c syn.c reg.c led.c uc.c term.c conf.c \
        rset.c rstr.c tag.c cmd.c regex.c && \
     install -D -m 755 vi "$XAI_ROOT/usr/bin/vi" && \
     ln -sf vi "$XAI_ROOT/usr/bin/ex" && \
     ln -sf vi "$XAI_ROOT/usr/bin/view")
fi


