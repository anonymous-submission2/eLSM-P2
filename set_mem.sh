#free
#sync
#sudo sh -c 'echo 3 >/proc/sys/vm/drop_caches'
#free
cgcreate -g memory:/myGroup1
sudo sh -c 'echo $(( 1024* 1024 * 1024 )) > /sys/fs/cgroup/memory/myGroup1/memory.limit_in_bytes'
#sudo sh -c 'echo $(( 5120* 1024 * 1024 )) > /sys/fs/cgroup/memory/myGroup/memory.memsw.limit_in_bytes'
