#include "hist.h"
#include "ycsb_db.h"
#include "utils.h"
#include <thread>
#include <chrono>
using namespace std;
std::atomic<long> totalL(0);
void ecall_ycsb_open_wrapper(void);
void ecall_put_wrapper(const char *kdata, int ksize, const char *vdata, int vsize);
void ecall_get_wrapper(char *kdata, int ksize, char *vdata);
namespace ycsbc {

  hist::hist() {
    std::cout<<"Open db" << std::endl;
    ecall_ycsb_open_wrapper();
  }

  /// ignore table
  /// ignore field
  /// read value indicated by a key
  int hist::Read(const string &table, const string &key,
      const vector<string> *fields, vector<KVPair> &result) {
    std::string value;
    static int count = 0;
    char val[400];
    long st = utils::time_now();
    //printf("get key size %d content %s\n",key.size(),key.c_str());
    ecall_get_wrapper((char *)key.c_str(),key.size(),val);
    long et = utils::time_now();
    long cur = totalL.load();
    cur += (et-st);
    totalL.store(cur);
    //printf("calling read %d\n",++count);
    return DB::kOK;
  }

  // ignore table
  // update value indicated by a key
  int hist::Update(const string &table, const string &key,
      vector<KVPair> &values) {
    static int count = 0;
    string val = "";
    for (auto v : values) {
      //val += v.first + "=" + v.second + " ";
      val += v.second;
    }
    //printf("put key size %d content %s value size %d content %s\n",key.size(),key.c_str(),val.size(),val.c_str());
    long st = utils::time_now();
    ecall_put_wrapper(key.c_str(),key.size(),val.c_str(),val.size());
    long et = utils::time_now();
    long cur = totalL.load();
    cur += (et-st);
    totalL.store(cur);
    //printf("calling update %d\n",++count);
    return DB::kOK;
  }

  // ignore table
  // ignore field
  // concate values in KVPairs into one long value
  int hist::Insert(const string &table, const string &key,
      vector<KVPair> &values) {
    return Update(table, key, values);
  }

  // ignore table
  // delete value indicated by a key
  int hist::Delete(const string &table, const string &key) {
    vector<KVPair> empty_val;
    return Update(table, key, empty_val);
  }

  // get all tx from the start_block until latest
  vector<string> hist::PollTxn(int block_number) {
    vector<string> v;
    return v;
  }


  unsigned int hist::GetTip() { return 0;}
}  // ycsbc
