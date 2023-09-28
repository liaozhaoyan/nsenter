# nsenter
lua nsenter api

# build

To build inotify.so, simply type make.

# Usage

new -> enter -> do some work. -> exit
```lua
local nsenter = require("nsenter")
local ns = nsenter.new()

ns:enter(58834, "net")  --enter pid net namespace. arg can set int [cgroup mnt pid user ipc net uts]

ns:exit()  -- back to last namespace
```
