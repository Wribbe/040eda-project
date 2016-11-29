#!/bin/sh

#executable="eda040_server"
executable="hello"
cam_num="2"
address="argus-${cam_num}.e.lth.lu.se"
scp_user="rt"
dest="~"

scp ${executable} ${scp_user}@${address}:${dest}
