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
    print("--> " .. cmd)
    local success = os.execute(cmd)
    if not success then
        print("\n[31m[!] Command failed.")
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

-- --- 3. Task Definitions ---
local tasks = {}

tasks["init"] = function()
    print("==> Initializing build environment...")
    local dirs = {"/bin", "/sbin", "/lib", "/etc", "/usr/bin", "/usr/lib"}
    for _, d in ipairs(dirs) do sh("mkdir -p " .. cfg.root .. d) end
    sh("ln -sf ../lib " .. cfg.root .. "/usr/lib")
    
    -- Standard NetBSD Includes and Dirs
    sh(string.format("cd %s/etc && %s %s distrib-dirs", cfg.src, cfg.nbmake, cfg.flags))
    sh(string.format("cd %s && %s %s includes", cfg.src, cfg.nbmake, cfg.flags))
end

tasks["branding"] = function()
    print("==> Applying Kayoko Branding to Kernel")

    local vers_script = cfg.src .. "/sys/conf/newvers.sh"
    local f = io.open(vers_script, "r")
    if not f then
        print("[!] Error: Could not find newvers.sh at " .. vers_script .. "")
        return
    end

    local content = f:read("*all")
    f:close()

    -- Swap NetBSD for Kayoko in the ostype variable
    local new_content, count = content:gsub('ost="NetBSD"', 'ost="Kayoko"')

    if count > 0 then
        local wf = io.open(vers_script, "w")
        wf:write(new_content)
        wf:close()
        print("  [Branding] Successfully changed ostype to 'Kayoko'")
    else
        print("  [Branding] Kernel already branded or pattern not found.")
    end

    sh("touch " .. cfg.src .. "/.kayoko_branded")
end

tasks["tools"] = function()
    if not io.open(cfg.nbmake, "r") then
        print("==> Building NetBSD Toolchain (This takes a while)")
        sh(string.format("cd %s && env -i PATH=/usr/bin:/bin TERM=$TERM HOME=$HOME ./build.sh -m %s -T %s -O %s -u tools",
            cfg.src, cfg.arch, cfg.tools, cfg.obj))
    else
        print("==> Toolchain already exists at " .. cfg.nbmake .. "")
    end
end

tasks["base"] = function()
    print("==> Building Base System Components")
    
    local function build_nbsd_component(path, install_to_root)
        print("[Component] " .. path)
        local target_dest = install_to_root and cfg.root or cfg.destdir
        local local_flags = string.format("DESTDIR=%s UNPRIVILEGED=no MKUNPRIVILEGED=no", target_dest)

        sh(string.format("cd %s/%s && %s %s obj && %s %s depend && %s %s -j%s all install",
            cfg.src, path, cfg.nbmake, local_flags, cfg.nbmake, local_flags, cfg.nbmake, local_flags, cfg.nprocs))
    end

    build_nbsd_component("lib/csu")
    build_nbsd_component("external/gpl3/gcc/lib/libgcc")
    build_nbsd_component("lib/libc")

    -- Sync Libc to Root
    sh(string.format("cp -P %s/lib/libc.s* %s/lib/", cfg.destdir, cfg.root))

    print("==> Syncing Libraries to Kayoko Root")
    local function sync_lib(name)
        print("  [Sync] " .. name)
        sh(string.format("cp -P %s/lib/lib%s.s* %s/lib/", cfg.destdir, name, cfg.root))
        sh(string.format("cp -P %s/lib/lib%s.a %s/lib/ 2>/dev/null || true", cfg.destdir, name, cfg.root))
    end

    sync_lib("c")
    sync_lib("gcc_s")
    sync_lib("gcc")
    sync_lib("util")
    sync_lib("crypt")
    sync_lib("curses")
    sync_lib("terminfo")

    sh(string.format("cp -P %s/usr/lib/libgcc_s.* %s/lib/ 2>/dev/null || true", cfg.destdir, cfg.root))
end

tasks["coreutils"] = function()
    print("==> Building Kayoko Core & Admin Utils")

 
     -- 1. PREPARATION: Ensure the Kayoko skeleton exists
    local dirs = {
        "/bin", "/sbin", "/usr/bin", "/usr/sbin",
        "/usr/share/man/man1", 
        "/usr/share/man/html1"
    }
    
    for _, dir in ipairs(dirs) do
        -- Use mkdir -p to silently ensure the path exists in the root prefix
        os.execute(string.format("mkdir -p %s%s", cfg.root, dir))
    end
    -- Define specific install locations to override the /usr/bin default
    local path_map = {
        ls = "/bin/", cp = "/bin/", mv = "/bin/", rm = "/bin/",
        cat = "/bin/", echo = "/bin/", pwd = "/bin/", mkdir = "/bin/",
        sh = "/bin/", date = "/bin/", chmod = "/bin/", hostname = "/bin/",
        ed = "/bin/", login = "/sbin/", smat = "/sbin/", lsdsk = "/bin/"
    }

    local function compile_util(src, bin, extra_ld)
        if needs_update(src, bin) then
            print(string.format("[CC] %s -> %s", src, bin:gsub(cfg.root, "")))
            -- Kayoko defaults: Static, stripped, and linked against libutil
            local cmd = string.format("cc -I%s/include -O2 -static -s -o %s %s %s -lutil %s",
                cfg.inst, bin, src, os.getenv("LDFLAGS") or "-lutil -lm -lprop", extra_ld or "")
            if not os.execute(cmd) then
                print("\n[!] Compilation failed for " .. src)
                os.exit(1)
            end
        end
    end

    local categories = {"core", "adm", "textproc"}
    for _, cat in ipairs(categories) do
        local base_dir = "src/cmd/" .. cat
        local handle = io.popen("ls -F " .. base_dir .. " 2>/dev/null")
        
        if handle then
            for entry in handle:lines() do
                if entry:match("/$") then
                    -- STRUCTURED: Entry is a directory (e.g., textproc/ed/)
                    local name = entry:sub(1, -2) -- Strip trailing slash for map lookup
                    local subdir = base_dir .. "/" .. name
                    local target_dir = path_map[name] or "/usr/bin/"

                    if io.open(subdir .. "/Makefile", "r") then
                        print(string.format("[MAKE] %s", subdir))
                        
                        -- Pass BINDIR for the internal path and DESTDIR for the physical root
                        -- Ensure no double slashes or nil strings occur during concat
                        local make_cmd = string.format("make -C %s BINDIR=%s DESTDIR=%s install",
                            subdir, target_dir, cfg.root)

                        if not os.execute(make_cmd) then
                            print("[!] Make failed in " .. subdir)
                            os.exit(1)
                        end
                    end

                elseif entry:match("%.c$") then
                    -- UNSTRUCTURED: Entry is a loose .c file (e.g., core/ls.c)
                    local name = entry:match("([^/]+)%.c$")
                    local target_dir = path_map[name] or "/usr/bin/"
                    
                    -- Ensure target directory exists in the Kayoko root
                    os.execute("mkdir -p " .. cfg.root .. target_dir)
                    
                    compile_util(base_dir .. "/" .. entry, cfg.root .. target_dir .. name)
                end
            end
            handle:close()
        end
    end

    -- Final hardening for the Kayoko environment
    print("--> Setting system immutable flags")
    os.execute("chflags schg " .. cfg.root .. "/sbin/login")
    
end


tasks["kernel"] = function()
    print("==> Building Kayoko Kernel [%s]", cfg.kernel)

    local conf_path = string.format("%s/sys/arch/%s/conf/%s", cfg.src, cfg.arch, cfg.kernel)
    if not io.open(conf_path, "r") then
        sh(string.format("cp %s/sys/arch/%s/conf/GENERIC %s", cfg.src, cfg.arch, conf_path))
    end

    sh(string.format("cd %s && ./build.sh -m %s -T %s -O %s kernel=%s",
        cfg.src, cfg.arch, cfg.tools, cfg.obj, cfg.kernel))

    local kernel_obj = string.format("%s/sys/arch/%s/compile/%s/netbsd", cfg.obj, cfg.arch, cfg.kernel)
    sh(string.format("cp %s %s/unix", kernel_obj, cfg.root))
end

-- --- 4. Execution Logic ---

local function usage()
    print("Usage: ./build.lua [task1 task2 ...]")
    print("Available Tasks: init, branding, tools, base, coreutils, kernel, all")
    os.exit(1)
end

if #arg == 0 then usage() end

local run_list = {}
if arg[1] == "all" then
    run_list = {"init", "branding", "tools", "base", "coreutils", "kernel"}
else
    run_list = arg
end

for _, task_name in ipairs(run_list) do
    if tasks[task_name] then
        tasks[task_name]()
    else
        print("[!] Unknown task: " .. task_name .. "")
        usage()
    end
end

print("==> Build Sequence Complete. Kayoko Root at: " .. cfg.root .. "")
