// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "port/port_posix.h"

#include <cstdlib>
#include <stdio.h>
#include <string.h>

namespace leveldb {
namespace port {

static void PthreadCall(const char* label, int result) {
  if (result != 0) {
    //fprintf(stderr, "sgx_thread %s: %s\n", label, strerror(result));
    abort();
  }
}

Mutex::Mutex() { PthreadCall("init mutex", sgx_thread_mutex_init(&mu_, NULL)); }

Mutex::~Mutex() { PthreadCall("destroy mutex", sgx_thread_mutex_destroy(&mu_)); }

void Mutex::Lock() { PthreadCall("lock", sgx_thread_mutex_lock(&mu_)); }

void Mutex::Unlock() { PthreadCall("unlock", sgx_thread_mutex_unlock(&mu_)); }

CondVar::CondVar(Mutex* mu)
    : mu_(mu) {
    PthreadCall("init cv", sgx_thread_cond_init(&cv_, NULL));
}

CondVar::~CondVar() { PthreadCall("destroy cv", sgx_thread_cond_destroy(&cv_)); }

void CondVar::Wait() {
  PthreadCall("wait", sgx_thread_cond_wait(&cv_, &mu_->mu_));
}

void CondVar::Signal() {
  PthreadCall("signal", sgx_thread_cond_signal(&cv_));
}

void CondVar::SignalAll() {
  PthreadCall("broadcast", sgx_thread_cond_broadcast(&cv_));
}

//void InitOnce(OnceType* once, void (*initializer)()) {
//  PthreadCall("once", sgx_thread_once(once, initializer));
//}

}  // namespace port
}  // namespace leveldb
