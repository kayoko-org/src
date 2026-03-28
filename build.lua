#!/usr/bin/env lua

local os = require("os")
local io = require("io")

-- --- 1. Core Configuration ---
local cfg = {
    root     = os.getenv("KAYAKO_ROOT") or (os.getenv("PWD") .. "/root"),
    obj      = os.getenv("OBJ_DIR")     or (os.getenv("PWD") .. "/root/obj"),
    tools    = os.getenv("TOOL_DIR")    or (os.getenv("PWD") .. "/root/tools"),
    src      = os.getenv("NBSD_SRC")    or (os.getenv("PWD") .. "/src/gate"),
    arch     = os.getenv("ARCH")        or "amd64",
    inst     = os.getenv("_INST")       or (os.getenv("PWD") .. "/inst"),
    nprocs   = os.getenv("NPROCS")      or "4",
    kernel   = "KAYAKO"
}

cfg.destdir = cfg.obj .. "/destdir." .. cfg.arch
cfg.nbmake  = cfg.tools .. "/bin/nbmake-" .. cfg.arch
cfg.flags   = string.format("DESTDIR=%s UNPRIVILEGED=no MKUNPRIVILEGED=no", cfg.destdir)

-- --- 2. Helper Functions ---

local function sh(cmd)
    print("\27[32m-->\27[0m " .. cmd)
    local success = os.execute(cmd)
    if not success then
        print("\n\27[31m[!] Command failed.\27[0m")
        os.exit(1)
    end
end

-- Checks if source is newer than target
local function needs_update(source, target)
    local f_t = io.open(target, "r")
    if not f_t then return true end
    f_t:close()
    
    -- Get timestamps via stat (NetBSD style)
    local get_time = function(f)
        local h = io.popen("stat -f %m " .. f)
        local out = h:read("*all")
        h:close()
        return tonumber(out) or 0
    end
    
    return get_time(source) > get_time(target)
end

-- --- 3. Stage: Environment Setup ---
print("\27[1m==> Initializing build...\27[0m")
local dirs = {"/bin", "/sbin", "/lib", "/etc", "/usr/bin", "/usr/lib"}
for _, d in ipairs(dirs) do sh("mkdir -p " .. cfg.root .. d) end
sh("ln -sf ../lib " .. cfg.root .. "/usr/lib")

-- --- Branding Stage ---
local function apply_branding()
    print("\27[1m==> Applying Kayoko Branding to Kernel\27[0m")
    
    local vers_script = cfg.src .. "/sys/conf/newvers.sh"
    local f = io.open(vers_script, "r")
    if not f then
        print("\27[31m[!] Error: Could not find newvers.sh at " .. vers_script .. "\27[0m")
        return
    end

    local content = f:read("*all")
    f:close()

    -- Swap NetBSD for Kayoko in the ostype variable
    -- We use a pattern that matches the standard assignment
    local new_content, count = content:gsub('ost="NetBSD"', 'ost="Kayoko"')
    
    if count > 0 then
        local wf = io.open(vers_script, "w")
        wf:write(new_content)
        wf:close()
        print("  [Branding] Successfully changed ostype to 'Kayoko'")
    else
        print("  [Branding] Kernel already branded or pattern not found.")
    end

    -- Optional: Touch a marker file so we don't keep checking if you prefer
    sh("touch " .. cfg.src .. "/.kayoko_branded")
end

-- Call it before building the kernel
apply_branding()

-- --- 4. Stage: Toolchain ---
if not io.open(cfg.nbmake, "r") then
    print("\27[1m=== Building NetBSD Toolchain (This takes a while) ===\27[0m")
    sh(string.format("cd %s && env -i PATH=/usr/bin:/bin TERM=$TERM HOME=$HOME ./build.sh -m %s -T %s -O %s -u tools", 
        cfg.src, cfg.arch, cfg.tools, cfg.obj))
end

-- --- 5. Stage: System Components ---
local function build_nbsd_component(path, install_to_root)
    print("\27[34m[Component]\27[0m " .. path)
    local target_dest = install_to_root and cfg.root or cfg.destdir
    local local_flags = string.format("DESTDIR=%s UNPRIVILEGED=no MKUNPRIVILEGED=no", target_dest)
    
    sh(string.format("cd %s/%s && %s %s obj && %s %s depend && %s %s -j%s all install",
        cfg.src, path, cfg.nbmake, local_flags, cfg.nbmake, local_flags, cfg.nbmake, local_flags, cfg.nprocs))
end

-- Standard NetBSD Includes and Dirs
sh(string.format("cd %s/etc && %s %s distrib-dirs", cfg.src, cfg.nbmake, cfg.flags))
sh(string.format("cd %s && %s %s includes", cfg.src, cfg.nbmake, cfg.flags))

build_nbsd_component("lib/csu")
build_nbsd_component("external/gpl3/gcc/lib/libgcc")
build_nbsd_component("lib/libc")

-- Sync Libc to Root
sh(string.format("cp -P %s/lib/libc.s* %s/lib/", cfg.destdir, cfg.root))

-- --- 6. Stage: Kayoko Custom Utilities ---
print("\27[1m=== Building Kayoko Core & Admin Utils ===\27[0m")

local function compile_util(src, bin, extra_ld)
    if needs_update(src, bin) then
        sh(string.format("cc -I%s/include -O2 -static -s -o %s %s %s -lutil %s", 
            cfg.inst, bin, src, os.getenv("LDFLAGS") or "", extra_ld or ""))
    end
end

-- Build Core (ls, cp, etc)
local handle = io.popen("ls src/cmd/core/*.c")
for src in handle:lines() do
    local name = src:match("([^/]+)%.c$")
    local path = (name:match("^(ls|cp|mv|rm|cat|echo|pwd|mkdir)$")) and "/bin/" or "/usr/bin/"
    compile_util(src, cfg.root .. path .. name)
end
handle:close()

-- Build Admin (login, sysmgr, etc)
compile_util("src/cmd/adm/login.c",  cfg.root .. "/sbin/login", "-lcrypt")
sh("chflags schg " .. cfg.root .. "/sbin/login")
compile_util("src/cmd/adm/sysmgr.c", cfg.root .. "/sbin/sysmgr", "-lcurses -lterminfo")
compile_util("src/cmd/adm/lsdsk.c",  cfg.root .. "/bin/lsdsk", "-lprop")

-- --- Library Sync Stage ---
print("\27[1m=== Syncing Libraries to Kayoko Root ===\27[0m")

local function sync_lib(name)
    print("  [Sync] " .. name)
    -- Copy the actual shared objects and static archives
    -- We use 'cp -P' to preserve symlinks for versioned .so files
    sh(string.format("cp -P %s/lib/lib%s.s* %s/lib/", cfg.destdir, name, cfg.root))
    sh(string.format("cp -P %s/lib/lib%s.a %s/lib/", cfg.destdir, name, cfg.root))
end

-- Sync the essentials
sync_lib("c")        -- Libc
sync_lib("gcc_s")    -- Runtime GCC support
sync_lib("gcc")      -- Static GCC support
sync_lib("util")     -- NetBSD system utilities (needed for your lsdsk/login)
sync_lib("crypt")    -- Password hashing
sync_lib("curses")   -- For sysmgr
sync_lib("terminfo") -- For sysmgr

-- Extra step: libgcc_s often lives in /usr/lib on some setups
-- Ensure Kayoko can find it if the dynamic linker looks there
sh(string.format("cp -P %s/usr/lib/libgcc_s.* %s/lib/ 2>/dev/null || true", cfg.destdir, cfg.root))

-- --- 7. Stage: The Kernel (KAYAKO) ---
print("\27[1m==> Building Kayoko Kernel [%s]\27[0m", cfg.kernel)

-- Ensure Config exists
local conf_path = string.format("%s/sys/arch/%s/conf/%s", cfg.src, cfg.arch, cfg.kernel)
if not io.open(conf_path, "r") then
    sh(string.format("cp %s/sys/arch/%s/conf/GENERIC %s", cfg.src, cfg.arch, conf_path))
end

sh(string.format("cd %s && ./build.sh -m %s -T %s -O %s kernel=%s", 
    cfg.src, cfg.arch, cfg.tools, cfg.obj, cfg.kernel))

-- Move kernel to /unix (The AIX/Kayoko way)
local kernel_obj = string.format("%s/sys/arch/%s/compile/%s/netbsd", cfg.obj, cfg.arch, cfg.kernel)
sh(string.format("cp %s %s/unix", kernel_obj, cfg.root))

print("\27[1;32m==> Build Complete. Kayoko Root populated at: " .. cfg.root .. "\27[0m")
