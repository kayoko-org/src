tasks = {
    -- SECTION: SYSTEM STATE
    { label = "System Power & State...", submenu = {
        { label = "Sync Disks",          cmd = "sync; sync",      input = false },
        { label = "Remount Root R/O",    cmd = "mount -u -o ro /", input = false },
        { label = "Remount Root R/W",    cmd = "mount -u -o rw /", input = false },
        { label = "Reboot System",       cmd = "reboot",          input = false },
        { label = "Halt System",         cmd = "halt -p",         input = false },
    }},

    -- SECTION: STORAGE & DISKS
    { label = "Storage & Filesystems...", submenu = {
        { label = "List Partitions (lsblk)", cmd = "lsblk",            input = false },
        { label = "Disk Usage (df -h)",      cmd = "df -h",             input = false },
        { label = "Check Filesystem (fsck)", cmd = "fsck -y %s",        input = true,  prompt = "Device (e.g. /dev/ada0p2): " },
        { label = "List Mounted FS",         cmd = "mount",             input = false },
    }},

    -- SECTION: NETWORK
    { label = "Network Configuration...", submenu = {
        { label = "Interface Status",    cmd = "ifconfig -a",       input = false },
        { label = "Routing Table",       cmd = "netstat -rn",       input = false },
        { label = "Listening Ports",     cmd = "netstat -an | grep LISTEN", input = false },
        { label = "Ping Host",           cmd = "ping -c 4 %s",      input = true,  prompt = "Hostname/IP: " },
        { label = "Traceroute",          cmd = "traceroute -w 2 %s",input = true,  prompt = "Target: " },
    }},

    -- SECTION: USER MANAGEMENT
    { label = "User & Group Management...", submenu = {
        { label = "Add New User",        cmd = "useradd -m %s",     input = true,  prompt = "Username: " },
        { label = "Delete User",         cmd = "userdel -r %s",     input = true,  prompt = "Username to remove: " },
        { label = "Modify Password",     cmd = "passwd %s",         input = true,  prompt = "User: " },
        { label = "Lock Account",        cmd = "pw lock %s",        input = true,  prompt = "User: " },
        { label = "Show Who is Logged In",cmd = "who",               input = false },
    }},

    -- SECTION: PROCESSES & SERVICES
    { label = "Processes & Services...", submenu = {
        { label = "Process Tree (ps)",   cmd = "ps -auwwxf",        input = false },
        { label = "Top (One-shot)",      cmd = "top -b -n 1",       input = false },
        { label = "Kill Process (PID)",  cmd = "kill -9 %s",        input = true,  prompt = "PID: " },
        { label = "Service Status",      cmd = "service %s status", input = true,  prompt = "Service Name: " },
    }},

    -- SECTION: LOGS
    { label = "System Logs...", submenu = {
        { label = "View Message Log",    cmd = "tail -n 50 /var/log/messages", input = false },
        { label = "SMAT Audit Log",      cmd = "tail -n 50 /var/log/smat.log", input = false },
    }},

    -- SECTION: UTILITIES
    { label = "System Utilities...", submenu = {
        { label = "Hardware Info",       cmd = "dmesg | tail -n 40", input = false },
        { label = "Check Uptime",        cmd = "uptime",             input = false },
        { label = "Current Date/Time",   cmd = "date",               input = false },
    }},

    --- BOTTOM ACTIONS ---
    { label = "Shell Escape",            cmd = "!sh",                input = false },
    { label = "Exit SMAT",               cmd = nil,                  input = false }
}
