#ifndef BLOCKBENCH_hist_H_
#define BLOCKBENCH_hist_H_

#include <iostream>
#include <string>
#include <unordered_map>

#include "properties.h"
#include "utils.h"
#include "timer.h"
#include "ycsb_db.h"
namespace ycsbc {

class hist : public DB {
 public:
  hist();

  void Init(SpinLock *lock) {
  }

  int Read(const std::string &table, const std::string &key,
           const std::vector<std::string> *fields, std::vector<KVPair> &result);

  // no scan operation support
  int Scan(const std::string &table, const std::string &key, int len,
           const std::vector<std::string> *fields,
           std::vector<std::vector<KVPair>> &result) {
    return DB::kOK;
  }

  int Update(const std::string &table, const std::string &key,
             std::vector<KVPair> &values);

  int Insert(const std::string &table, const std::string &key,
             std::vector<KVPair> &values);

  int Delete(const std::string &table, const std::string &key);
  std::vector<std::string> PollTxn(int block_number);

unsigned int GetTip();

 private:
  SpinLock *txlock_;

};

}  // ycsbc

#endif
