free
sync
sudo sh -c 'echo 3 >/proc/sys/vm/drop_caches'
free
#cgcreate -g memory:/myGroup
#sudo sh -c 'echo $(( 256* 1024 * 1024 )) > /sys/fs/cgroup/memory/myGroup/memory.limit_in_bytes'
#sudo sh -c 'echo $(( 2560* 1024 * 1024 )) > /sys/fs/cgroup/memory/myGroup/memory.memsw.limit_in_bytes'
