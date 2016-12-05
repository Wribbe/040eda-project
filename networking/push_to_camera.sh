#!/bin/sh

cam_num="2"

if [ ! $# -eq 0 ]; then
    cam_num="$1"
fi

exec_dir=executables
executable=${exec_dir}/embedded/"eda040_server"
address="argus-${cam_num}.e.lth.lu.se"
scp_user="rt"
dest="~"

scp ${executable} ${scp_user}@${address}:${dest}
