#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "db/db_impl.h"
#include "db/version_set.h"
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/write_batch.h"
#include "port/port.h"
#include "util/crc32c.h"
#include "util/histogram.h"
#include "util/mutexlock.h"
#include "util/random.h"
#include "util/testutil.h"
#include "Enclave.h"
#include "Enclave_t.h"  /* print_string */
#include <sgx_utils.h>
#include <sgx_trts.h>
#include <sgx_key.h>
#include <sgx_tcrypto.h>

sgx_aes_gcm_128bit_key_t cur_key;
uint8_t empty_iv[SGX_AESGCM_IV_SIZE];
sgx_aes_gcm_128bit_tag_t gmac;
sgx_cmac_128bit_tag_t cmac;


class RandomGenerator {
 private:
  std::string data_;
  int pos_;

 public:
  RandomGenerator() {
    // We use a limited amount of data over and over again and ensure
    // that it is larger than the compression window (32KB), and also
    // large enough to serve all typical value sizes we want to write.
    leveldb::Random rnd(301);
    std::string piece;
    while (data_.size() < 262144) {
      // Add a short fragment that is as compressible as specified
      // by FLAGS_compression_ratio.
      leveldb::test::CompressibleString(&rnd, 0.5,100, &piece);
      data_.append(piece);
    }
    pos_ = 0;
  }

  leveldb::Slice Generate(size_t len) {
    if (pos_ + len > data_.size()) {
      pos_ = 0;
      assert(len < data_.size());
    }
    pos_ += len;
    return leveldb::Slice(data_.data() + pos_ - len, len);
  }
};

leveldb::DB *minidb_;
void minibench_open() {
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB::Open(options,"/home/ju/minidb",&minidb_);
}


void minibench_write_forcompact(int writes, int total, bool seq) {
  leveldb::Random rand(304);
  RandomGenerator gen;
  leveldb::Status s;
  leveldb::WriteOptions options;
  char key_cipher[100];
  char val_cipher[112];
  printf("call into writes and number is %d\n",writes);
  for (int j = 0; j < 256 ; j++) {
    int k = j;
    for (int i=0;i<1024*40;i++) {
      //const int k = seq ? j : (rand.Next() % total);
      char key[100];
      snprintf(key, sizeof(key), "%08d", k);
      // sgx_rijndael128GCM_encrypt(&cur_key, (const uint8_t *)key, 8, (uint8_t *)key_cipher,(const uint8_t *)&empty_iv, SGX_AESGCM_IV_SIZE, NULL, 0, &gmac);
      //  sgx_rijndael128GCM_encrypt(&cur_key, (const uint8_t *)gen.Generate(112).data(), 112, (uint8_t *)val_cipher,(const uint8_t *)&empty_iv, SGX_AESGCM_IV_SIZE, NULL, 0, &gmac);
      //  s = minidb_->Put(options,leveldb::Slice(key_cipher,8),leveldb::Slice(val_cipher,112));
      s = minidb_->Put(options,key,gen.Generate(112));
      k += 1024;
    }
  }
}


void minibench_write(int writes, int total, bool seq) {
  leveldb::Random rand(304);
  RandomGenerator gen;
  leveldb::Status s;
  leveldb::WriteOptions options;
  char key_cipher[100];
  char val_cipher[112];
  printf("call into writes and number is %d\n",writes);
  for (int j = 0; j < writes ; j++) {
    const int k = seq ? j : (rand.Next() % total);
    char key[100];
    snprintf(key, sizeof(key), "%08d", k);
// sgx_rijndael128GCM_encrypt(&cur_key, (const uint8_t *)key, 8, (uint8_t *)key_cipher,(const uint8_t *)&empty_iv, SGX_AESGCM_IV_SIZE, NULL, 0, &gmac);
//  sgx_rijndael128GCM_encrypt(&cur_key, (const uint8_t *)gen.Generate(112).data(), 112, (uint8_t *)val_cipher,(const uint8_t *)&empty_iv, SGX_AESGCM_IV_SIZE, NULL, 0, &gmac);
//   s = minidb_->Put(options,leveldb::Slice(key_cipher,8),leveldb::Slice(val_cipher,112));
    s = minidb_->Put(options,key,gen.Generate(112));
//    if (j%100000 == 0)
//	printf("finisehd %d writes\n",j);
  }
}

void minibench_read(int reads, int total, bool seq) {
  leveldb::Random rand(305);
  leveldb::ReadOptions options;
  options.fill_cache=true;
  std::string value;
  printf("minibench_read %d out of %d\n",reads,total);
  for (int i = 0; i < reads; i++) {
    char key[100];
    int k = seq ? i : (rand.Next() % total);
//    k = k<<5;
    snprintf(key, sizeof(key), "%08d", k);
    minidb_->Get(options, key, &value).ok();
    printf("finished %d read key is %s value size is %d!!\n",i,key,value.size());
  }
}

void minibench_getproperty() {
  std::string result;
  minidb_->GetProperty("leveldb.sstables", &result);
  printf("result is %s\n",result.c_str());
}
