// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/builder.h"

#include "db/filename.h"
#include "db/dbformat.h"
#include "db/table_cache.h"
#include "db/version_edit.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"
#include "Enclave.h"
#include <sgx_utils.h>
#include <sgx_trts.h>
#include <sgx_key.h>
#include <sgx_tcrypto.h>
#define LPADP2 1
#define KSIZE 8
#define VSIZE 112
#define FILE_RECORDS  32768
#define TREE_HEIGHT 15

namespace leveldb {

sgx_aes_gcm_128bit_key_t cur_key;
sgx_cmac_128bit_tag_t cmac;

Status BuildTable(const std::string& dbname,
                  Env* env,
                  const Options& options,
                  TableCache* table_cache,
                  Iterator* iter,
                  FileMetaData* meta) {
  Status s;
  meta->file_size = 0;
  // LPADP2 start
  //Ju Chen: This is where we build the Merkle Tree for the File
  //The file size is fixed to store 32768 records, the file is padded with all-zero records.
  #if LPADP2
  iter->SeekToFirst();
  char* merkle = new char[FILE_RECORDS*2*16]; //16byte hash * 32768 * 2 
  char record[KSIZE+VSIZE+16];
  int mp = 0;
  int thr = 0;
  // update the leaf level
  for (; iter->Valid() && thr < FILE_RECORDS; iter->Next(), thr++) {
      Slice key = iter->key();
      Slice value = iter->value();
      memcpy(record,key.data(),key.size());
      memcpy(record+key.size(),value.data(),value.size());
      sgx_rijndael128_cmac_msg(&cur_key, (const uint8_t*)record,key.size()+value.size(),&cmac);
      memcpy(merkle+16*(mp++),&cmac,16);
      //printf("leaf node \n");
      //for(int i=0;i<16;i++)
       //  printf("%c,",merkle[16*(mp-1)+i]);
      //printf("\n");
  }
  //printf("record number is %d\n",thr);
  // update the non-leaf level
  mp = FILE_RECORDS;
  int previous = 0;
  int stride = FILE_RECORDS;
  for (int i=0;i<TREE_HEIGHT;i++) {
      for (int j=0; j<stride;j=j+2) {
      	sgx_rijndael128_cmac_msg(&cur_key, (const uint8_t*)(merkle+(previous+j)*16),32,&cmac);
      	memcpy(merkle+16*(mp++),&cmac,16);
        //printf("non leaf node \n");
      	//for(int k=0;k<16;k++)
        // printf("%c,",merkle[16*(mp-1)+k]);
        //printf("\n");
      }
      previous += stride;
      stride = stride>>1; 
  }
  #endif
#if 0
  //reconstruct for record 0
  char proof[TREE_HEIGHT*16];
  int index = 0;
  int pointer = 0;
  for(int i=0;i<TREE_HEIGHT;i++) { 
    pointer = index%2?index-1:index+1;
    index = index/2 + FILE_RECORDS;
    memcpy(proof+i*16,merkle+pointer*16,16);
    //printf("reconstruct node\n");
    //for(int j=0;j<16;j++)
    //   printf("%c,",proof[i*16+j]);
    //printf("\n");
  }
  // verify for record 0
  iter->SeekToFirst();
  char tmp[32];
  Slice key = iter->key();
  Slice value = iter->value();
  memcpy(record,key.data(),key.size());
  memcpy(record+key.size(),value.data(),value.size());
  sgx_rijndael128_cmac_msg(&cur_key, (const uint8_t*)record,key.size()+value.size(),&cmac);
  for(int i=0;i<TREE_HEIGHT;i++)
  {
    memcpy(tmp,&cmac,16);
    memcpy(tmp+16,proof+i*16,16);
    sgx_rijndael128_cmac_msg(&cur_key, (const uint8_t*)tmp,32,&cmac);
  }
  int res = memcmp(&cmac,&merkle[(FILE_RECORDS*2-2)*16],16);
  printf("in build table and proof verification is %d\n",res);
#endif
  

  iter->SeekToFirst();

  std::string fname = TableFileName(dbname, meta->number);
  if (iter->Valid()) {
    WritableFile* file;
    s = env->NewUnSecureWritableFile(fname, &file);
    if (!s.ok()) {
      return s;
    }
    TableBuilder* builder = new TableBuilder(options, file);
    meta->smallest.DecodeFrom(iter->key());
    int ppos = 0;
    char proofandvalue[TREE_HEIGHT*16+VSIZE];
    for (; iter->Valid(); iter->Next()) {
      int index = ppos++;
      int pointer = 0;
#if LPADP2
      for(int i=0;i<TREE_HEIGHT;i++) { 
        pointer = index%2?index-1:index+1;
        index = index/2 + FILE_RECORDS;
        memcpy(proofandvalue+VSIZE+i*16,merkle+pointer*16,16);
      }
#endif
      Slice key = iter->key();
      meta->largest.DecodeFrom(key);
#if LPADP2
      memcpy(proofandvalue,iter->value().data(),VSIZE);
      builder->Add(key, Slice(proofandvalue,VSIZE+TREE_HEIGHT*16));
#else
      builder->Add(key,iter->value());
#endif
    }
#if LPADP2
    memcpy(meta->rh,&merkle[(2*FILE_RECORDS-2)*16],16);
    delete merkle;
#endif
    // Finish and check for builder errors
    if (s.ok()) {
      s = builder->Finish();
      if (s.ok()) {
        meta->file_size = builder->FileSize();
        assert(meta->file_size > 0);
      }
    } else {
      builder->Abandon();
    }
    delete builder;

    // Finish and check for file errors
    if (s.ok()) {
      s = file->Sync();
    }
    if (s.ok()) {
      s = file->Close();
    }
    delete file;
    file = NULL;

    if (s.ok()) {
      // Verify that the table is usable
      Iterator* it = table_cache->NewIterator(ReadOptions(),
                                              meta->number,
                                              meta->file_size);
      s = it->status();
      if (s.ok()) {}
      else
      delete it;
    }
  }

  // Check for input iterator errors
  if (!iter->status().ok()) {
    s = iter->status();
  }

  if (s.ok() && meta->file_size > 0) {
    // Keep it
  } else {
    env->DeleteFile(fname);
  }
  return s;
}

}  // namespace leveldb
