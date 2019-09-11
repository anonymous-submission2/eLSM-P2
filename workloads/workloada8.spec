# Yahoo! Cloud System Benchmark
# Workload A: Update heavy workload
#   Application example: Session store recording recent actions
#                        
#   Read/update ratio: 50/50
#   Default data size: 1 KB records (10 fields, 100 bytes each, plus key)
#   Request distribution: zipfian

#recordcount=2097152
#recordcount=3145728
#recordcount=5242880
#recordcount=7340032
#recordcount=8388608
#recordcount=10485760
#recordcount=17000000
recordcount=26000000
#recordcount=35000000
operationcount=1000000
workload=ycsb
fieldcount=1
readallfields=true
fieldlength=112
readproportion=0.6
updateproportion=0.4
scanproportion=0
insertproportion=0

requestdistribution=uniform

