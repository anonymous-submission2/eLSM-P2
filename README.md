To run eLSM-P2, it requires an SGX machine (with Intel CPU supporting SGX instruction set).

Compile and build
===

The commands below write 1000,000 records to the database. You can run other reads by other scripts.

```
make
./run_write.sh
```

Execute
===

The commands below load YCSB workloads and then run YCSB transactions

```
./load_ycsb.sh
ycsb.sh
```
