#!/bin/sh

exec_dir=executables
executable=${exec_dir}/embedded/"eda040_server"
cam_num="2"
address="argus-${cam_num}.e.lth.lu.se"
scp_user="rt"
dest="~"

scp ${executable} ${scp_user}@${address}:${dest}
