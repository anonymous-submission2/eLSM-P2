//
//  ycsbc.cc
//  YCSB-C
//
//  Created by Jinglei Ren on 12/19/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//  Ju: Tailored the code for LevelDB <chenju2k6@gmail.com>

#include <cstring>
#include <string>
#include <iostream>
#include <vector>
#include <future>
#include <atomic>
#include <sstream>
#include <unordered_map>
#include "utils.h"
#include "properties.h"
#include "timer.h"
#include "client.h"
#include "ycsb_db.h"
#include "core_workload.h"
#include "db_factory.h"
extern std::atomic<long> totalL;
using namespace std;

const unsigned int BLOCK_POLLING_INTERVAL = 2;
const unsigned int CONFIRM_BLOCK_LENGTH = 5;
const unsigned int HL_CONFIRM_BLOCK_LENGTH = 1;
const unsigned int PARITY_CONFIRM_BLOCK_LENGTH = 1;

std::unordered_map<string, double> pendingtx;
// locking the pendingtx queue
SpinLock txlock;

void UsageMessage(const char *command);
bool StrStartWith(const char *str, const char *pre);
string ParseCommandLine(int argc, const char *argv[], utils::Properties &props);

utils::Timer<double> stat_timer;

int DelegateClient(ycsbc::DB *db, ycsbc::CoreWorkload *wl, const int num_ops,
    bool is_loading) {
  db->Init();
  ycsbc::Client client(*db, *wl);
  int oks = 0;
  for (int i = 0; i < num_ops; ++i) {
    if (is_loading) {
      oks += client.DoInsert();
    } else {
      oks += client.DoTransaction();
    }
  }
  db->Close();
  return oks;
}


int ycsb_main(const int argc, const char *argv[]) {
  utils::Properties props;
  string file_name = ParseCommandLine(argc, argv, props);

  ycsbc::DB *db = ycsbc::DBFactory::CreateDB(props);
  if (!db) {
    cout << "Unknown database name " << props["dbname"] << endl;
    exit(0);
  }

  db->Init(&pendingtx, &txlock);

  ycsbc::CoreWorkload wl;
  wl.Init(props);

  const int num_threads = stoi(props.GetProperty("threadcount", "1"));

  utils::Timer<double> stat_timer;

  vector<future<int>> actual_ops;
  int total_ops = stoi(props[ycsbc::CoreWorkload::RECORD_COUNT_PROPERTY]);
  int tx_ops = stoi(props[ycsbc::CoreWorkload::OPERATION_COUNT_PROPERTY]);
  int sum = 0;
  if (props["load"] == "1") {
    // Loads data
    cout << "total_ops=" << total_ops << " tx_ops" << tx_ops << endl;
    for (int i = 0; i < num_threads; ++i) {
      actual_ops.emplace_back(async(launch::async, DelegateClient, db, &wl,
            total_ops / num_threads, true));
    }

    for (auto &n : actual_ops) {
      assert(n.valid());
      sum += n.get();
    }
    cout << "# Loading records :\t" << sum << endl;
  }
  else {
    actual_ops.clear();
    long start_time = utils::time_now();
    for (int i = 0; i < num_threads; ++i) {
      actual_ops.emplace_back(async(launch::async, DelegateClient, db, &wl,
            tx_ops / num_threads, false));
    }

    sum = 0;
    for (auto &n : actual_ops) {
      assert(n.valid());
      sum += n.get();
    }
    long end_time = utils::time_now();
    cout << "# Transaction records :\t" << sum << " out of " << total_ops << endl;
    cout << "# Transaction throughput =\t" << tx_ops/((end_time-start_time)/1000000000.0) << " ops/s" << endl;
    cout << "# Transaction latency =\t" << totalL.load()/tx_ops << " ns/op" << endl;
  } 
}

string ParseCommandLine(int argc, const char *argv[],
    utils::Properties &props) {
  int argindex = 2;
  string filename;
  while (argindex < argc && StrStartWith(argv[argindex], "-")) {
    if (strcmp(argv[argindex], "-threads") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("threadcount", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-load") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("load", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-P") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      filename.assign(argv[argindex]);
      ifstream input(argv[argindex]);
      try {
        props.Load(input);
      } catch (const string &message) {
        cout << message << endl;
        exit(0);
      }
      input.close();
      argindex++;
    } else {
      cout << "Unknown option '" << argv[argindex] << "'" << endl;
      exit(0);
    }
  }

  if (argindex == 1 || argindex != argc) {
    UsageMessage(argv[0]);
    exit(0);
  }

  return filename;
}

void UsageMessage(const char *command) {
  cout << "Usage: " << command << " [options]" << endl;
  cout << "Options:" << endl;
  cout << "  -threads n: execute using n threads (default: 1)" << endl;
  cout << "  -P propertyfile: load properties from the given file. Multiple "
    "files can" << endl;
  cout << "                   be specified, and will be processed in the order "
    "specified" << endl;
}

inline bool StrStartWith(const char *str, const char *pre) {
  return strncmp(str, pre, strlen(pre)) == 0;
}
