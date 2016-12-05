#!/bin/sh

cam_num="2"

if [ ! $# -eq 0 ]; then
    cam_num="$1"
else
    read -p "Which camera to push to?: " cam_num
fi

exec_dir=executables
executable=${exec_dir}/embedded/"eda040_server"
address="argus-${cam_num}.e.lth.lu.se"
scp_user="rt"
dest="~"

scp ${executable} ${scp_user}@${address}:${dest}
