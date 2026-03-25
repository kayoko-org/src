#!/bin/ksh
# XAI Build Script - POSIX Strict Edition
set -e

# Configuration
XAI_ROOT="$(pwd)/root"
SBASE_DIR="$HOME/sbase"
OKSH_DIR="$HOME/oksh"
MUSL_VER="1.2.5"
MUSL_DIR="$HOME/musl-$MUSL_VER"
MUSL_INST="$HOME/musl_out"

# 0. POSIX check for parallelism
# Replaces GNU 'nproc'
NPROCS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)

# 1. Directory Setup (Stripped of /lib64 Linux-isms)
mkdir -p "$XAI_ROOT/bin" "$XAI_ROOT/sbin" "$XAI_ROOT/lib" "$XAI_ROOT/etc" "$XAI_ROOT/usr"
ln -sf ../lib "$XAI_ROOT/usr/lib"
cp srcmstr "$XAI_ROOT/sbin/init"

# 2. Build musl libc
if [ ! -d "$MUSL_INST" ]; then
    [ -f "musl-$MUSL_VER.tar.gz" ] || curl -O https://musl.libc.org/releases/musl-$MUSL_VER.tar.gz
    [ -d "$MUSL_DIR" ] || tar -xf musl-$MUSL_VER.tar.gz -C "$HOME"
    (cd "$MUSL_DIR" && ./configure --prefix="$MUSL_INST" --syslibdir="$MUSL_INST/lib" && make -j"$NPROCS" && make install)
fi

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
cc -O2 -o bootseq bootseq.c -lzstd
cc -O2 -static -o "$XAI_ROOT/sbin/login" login.c

# 5. Build SBASE (The Toolchest)
if [ ! -d "$SBASE_DIR" ]; then
    git clone https://git.suckless.org/sbase "$SBASE_DIR"
fi
# Using static LDFLAGS to keep the base tools independent of the shim
(cd "$SBASE_DIR" && make LDFLAGS="-static" sbase-box)

# 6. Populate /bin (POSIX tools only)
install -m 755 "$SBASE_DIR/sbase-box" "$XAI_ROOT/bin/.utils"
for tool in ls cat whoami touch tr true tar grep sed awk pwd mkdir cp mv rm; do
    (cd "$XAI_ROOT/bin" && ln -sf .utils "$tool")
done

UBASE_DIR="$HOME/ubase"

if [ ! -d "$UBASE_DIR" ]; then
    git clone https://git.suckless.org/ubase "$UBASE_DIR"
fi

# We use musl-gcc to ensure ubase is built against your distro's libc
(cd "$UBASE_DIR" && \
    make CC="$MUSL_INST/bin/musl-gcc" \
         LDFLAGS="-static" \
         ubase-box)

# Populate /bin and /sbin with ubase tools
install -m 755 "$UBASE_DIR/ubase-box" "$XAI_ROOT/bin/.ubase-utils"

# Critical tools for your srcmstr and init
for tool in blkdiscard chvt clear ctrlaltdel dd df dmesg eject fallocate free freeramdisk fsfreeze getty halt hwclock id insmod killall5 last lastlog login lsmod lsusb mesg mknod mkswap mount mountpoint nologin pagesize passwd pidof pivot_root ps pwdx readahead respawn rmmod stat su swaplabel swapoff swapon switch_root sysctl truncate umount unshare uptime vtallow watch who; do
    (cd "$XAI_ROOT/bin" && ln -sf .ubase-utils "$tool")
done

# AIX-specific location for some admin tools
for stool in hwclock swapon swapoff; do
    (cd "$XAI_ROOT/sbin" && ln -sf ../bin/.ubase-utils "$stool")
done

# 7. Build the AIX Identity Shim & Uname
# We link uname specifically to /lib/libc.so
"$MUSL_INST/bin/musl-gcc" -fPIC -shared -O2 -o "$XAI_ROOT/lib/libkstat.so" libkstat.c
"$MUSL_INST/bin/musl-gcc" -O2 -o "$XAI_ROOT/bin/uname" uname.c -Wl,-dynamic-linker,/lib/libc.so

# 8. Finalize Library Layout
# Move musl into the primary /lib/libc.so slot
cp "$MUSL_INST/lib/libc.so" "$XAI_ROOT/lib/libc.so"
chmod +x "$XAI_ROOT/lib/libc.so"

# Create the relative symlink for any legacy software looking for the musl loader name
(cd "$XAI_ROOT/lib" && ln -sf libc.so ld-musl-x86_64.so.1)

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
    tar -xf ~/ncurses-$NCURSES_VER.tar.gz -C "$HOME"
    (cd "$HOME/ncurses-$NCURSES_VER" && \
     ./configure --prefix="$MUSL_INST" \
                 --without-ada --without-tests --without-debug \
                 --enable-widec --enable-static && \
     make -j"$NPROCS" && make install)
fi

LIBMD_VER="1.1.0"
if [ ! -d "$HOME/libmd-$LIBMD_VER" ]; then
    curl -O https://archive.hadrons.org/software/libmd/libmd-$LIBMD_VER.tar.xz
    tar -xf libmd-$LIBMD_VER.tar.xz -C "$HOME"
	(cd "$HOME/libmd-1.1.0" && \
	 ./configure --prefix="$MUSL_INST" \
             --libdir="$MUSL_INST/lib" \
             --disable-shared --enable-static && \
	 make -j"$NPROCS" && make install)
fi

LIBBSD_VER="0.12.2"
if [ ! -d "$HOME/libbsd-$LIBBSD_VER" ]; then
    [ -f "libbsd-$LIBBSD_VER.tar.xz" ] || curl -O https://libbsd.freedesktop.org/releases/libbsd-$LIBBSD_VER.tar.xz
    [ -d "$HOME/libbsd-$LIBBSD_VER" ] || tar -xf libbsd-$LIBBSD_VER.tar.xz -C "$HOME"
(cd "$HOME/libbsd-0.12.2" && \
 ./configure --prefix="$MUSL_INST" \
             --build=x86_64-pc-linux-gnu \
             --host=x86_64-linux-musl \
             --disable-shared --enable-static \
             CC="$MUSL_INST/bin/musl-gcc" \
             LIBMD_CFLAGS="-I$MUSL_INST/include" \
             LIBMD_LIBS="-L$MUSL_INST/lib -lmd" \
             CPPFLAGS="-I$MUSL_INST/include" \
             LDFLAGS="-L$MUSL_INST/lib" && \
 make -j"$NPROCS" && make install)
fi


if [ ! -x "$XAI_ROOT/usr/bin/vi" ]; then
    [ -d "$HOME/neatvi" ] || git clone https://github.com/aligrudi/neatvi.git "$HOME/neatvi"
    
    (cd "$HOME/neatvi" && \
     "$MUSL_INST/bin/musl-gcc" -static -O2 -o vi \
        vi.c ex.c lbuf.c mot.c sbuf.c ren.c dir.c syn.c reg.c led.c uc.c term.c conf.c \
        rset.c rstr.c tag.c cmd.c regex.c && \
     install -D -m 755 vi "$XAI_ROOT/usr/bin/vi" && \
     ln -sf vi "$XAI_ROOT/usr/bin/ex" && \
     ln -sf vi "$XAI_ROOT/usr/bin/view")
fi


