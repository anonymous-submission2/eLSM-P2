# writes reads write_space read_space ifwrite ifread
#./app 0 10000000 100000 5000000 1000 1 0
#LD_PRELOAD=/home/ju/workspace/sgx-perf/build/lib/liblogger.so ./app 0  -load 0 -threads 1 -P workloads/workloada.spec 
./app 0  -load 0 -threads 1 -P workloads/workloada.spec 
