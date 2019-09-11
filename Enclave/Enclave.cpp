/*
 *Copyright 2019 FSSL (full-stack security lab) at Syracuse University
 *
 *Licensed under the Apache License, Version 2.0 (the "License");
 *you may not use this file except in compliance with the License.
 *You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing, software
 *distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *See the License for the specific language governing permissions and
 *limitations under the License.
 */
#include <iostream>
#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */

#include "Enclave.h"
#include "Enclave_t.h"  /* print_string */
#include "leveldb/db.h"
#include <sgx_utils.h>
#include <sgx_trts.h>
#include <sgx_key.h>
#include <sgx_tcrypto.h>
/* 
 * printf: 
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */

 extern sgx_aes_gcm_128bit_key_t cur_key;
 extern uint8_t empty_iv[SGX_AESGCM_IV_SIZE];
 extern sgx_aes_gcm_128bit_tag_t gmac;
 extern sgx_cmac_128bit_tag_t cmac;
void minibench_write(int writes, int total, bool seq);
void minibench_read(int reads, int total, bool seq);
void minibench_getproperty();
void minibench_open();
void printf(const char *fmt, ...)
{
  char buf[BUFSIZ] = {'\0'};
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, BUFSIZ, fmt, ap);
  va_end(ap);
  ocall_print_string(buf);
}
void ecall_test() {}
void ecall_open() {
  minibench_open();
}
leveldb::DB* ycsbdb_;
void ecall_ycsb_open() {
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB::Open(options,"/home/ju/minidb",&ycsbdb_);
  leveldb::ReadOptions ropt;
ropt.fill_cache=true;
  #if 0
    leveldb::Iterator* iter = ycsbdb_->NewIterator(ropt);
    iter->SeekToFirst();
    
    for(;iter->Valid();iter->Next()) {
      //printf("in iteration and key size is %d and value size is %d\n",iter->key().size(),iter->value().size());
    } 
  #endif
}

void ecall_put(const char *kdata, int ksize, const char *vdata, int vsize) {
  leveldb::Slice akey(kdata,ksize);
  leveldb::Slice aval(vdata,vsize);
 // printf("ecall put key is %s,key size is %d and ycsbdb_ is %p\n",kdata,ksize, ycsbdb_);
char key_cipher[8];
      char val_cipher[112];
     //printf("ecall put key is %s,key size is %d and ycsbdb_ is %p\n",kdata,ksize, ycsbdb_);
     ycsbdb_->Put(leveldb::WriteOptions(),akey,aval);
     sgx_rijndael128GCM_encrypt(&cur_key, (const uint8_t *)kdata, 8, (uint8_t *)key_cipher,(const uint8_t *)&empty_iv, SGX_AESGCM_IV_SIZE, NULL, 0, &gmac);
     sgx_rijndael128GCM_encrypt(&cur_key, (const uint8_t *)vdata, 112, (uint8_t *)val_cipher,(const uint8_t *)&empty_iv, SGX_AESGCM_IV_SIZE, NULL, 0, &gmac);
}
void ecall_get(char *kdata, int ksize, char *vdata) {
  char key[20];
  //memcpy(key,kdata,ksize);
  leveldb::Slice akey(kdata,ksize);
  std::string value;
  leveldb::ReadOptions options;
  options.fill_cache=true;
  //printf("ecall get key is %s,key size is %d\n",kdata,ksize);
  ycsbdb_->Get(leveldb::ReadOptions(),akey,&value);
  //printf("ecall get key is %s,key size is %d and value size %d\n",kdata,ksize,value.size());
  //memcpy(vdata,value.c_str(),value.size());
}

void ecall_compact() {
  printf("enter ecall_compact!\n");
  //global_->CompactRange(NULL,NULL);
}

void ecall_writetest(long key, long value) {
  char** gTestKeys = (char**)key;
  char** gTestValues = (char**)value;
  unsigned long eip;
  __asm__(
      // "jmp end_abort_handler_%=\n\t"
      "lea (%%rip), %%rax\n\t"
      "mov %%rax, %0\n\t"
      :"=r"(eip)
      :
      :);
  printf("current eip is %ld\n",eip);
}
void ecall_writes(int writes, int total) {
  minibench_write(writes,total,false);
}


void ecall_reads(int reads, int total) {
  printf("entered ecall reads!\n");
  minibench_read(reads,total,false);
}

void ecall_getproperty() {
  minibench_getproperty();
}

void ecall_fileiotest() {
  long f;
  int res = 0;
  char data[] = "helloh";
  char data1[4200];
  for (int i=0;i<4200;i++) data1[i] = 'h';
  ocall_fopen("/home/ju/sysbase/testfile", "a", &f);
  for (int i=0;i<600;i++) {
    if (i%2 == 0) {
      ocall_fwrite(data, 1, 6, f, &res);
    } else {
      ocall_fwrite(data1, 1, 4200, f, &res);
    }
  }
}
