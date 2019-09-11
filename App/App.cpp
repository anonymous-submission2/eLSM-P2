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
#include <thread>
#include <atomic>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
# include <unistd.h>
# include <pwd.h>
# define MAX_PATH FILENAME_MAX
#include <vector>
#include <string>
#include "sgx_urts.h"
#include "App.h"
#include "Enclave_u.h"
#include <pthread.h>
std::atomic<int> gread(0);
std::atomic<int> gwrite(0);
std::atomic<int> gwritesmall(0);
std::atomic<int> gopen(0);
std::atomic<int> gcheck(0);
/* Global EID shared by multiple threads */
sgx_enclave_id_t global_eid = 0;

typedef struct _sgx_errlist_t {
  sgx_status_t err;
  const char *msg;
  const char *sug; /* Suggestion */
} sgx_errlist_t;

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] = {
  {
    SGX_ERROR_UNEXPECTED,
    "Unexpected error occurred.",
    NULL
  },
  {
    SGX_ERROR_INVALID_PARAMETER,
    "Invalid parameter.",
    NULL
  },
  {
    SGX_ERROR_OUT_OF_MEMORY,
    "Out of memory.",
    NULL
  },
  {
    SGX_ERROR_ENCLAVE_LOST,
    "Power transition occurred.",
    "Please refer to the sample \"PowerTransition\" for details."
  },
  {
    SGX_ERROR_INVALID_ENCLAVE,
    "Invalid enclave image.",
    NULL
  },
  {
    SGX_ERROR_INVALID_ENCLAVE_ID,
    "Invalid enclave identification.",
    NULL
  },
  {
    SGX_ERROR_INVALID_SIGNATURE,
    "Invalid enclave signature.",
    NULL
  },
  {
    SGX_ERROR_OUT_OF_EPC,
    "Out of EPC memory.",
    NULL
  },
  {
    SGX_ERROR_NO_DEVICE,
    "Invalid SGX device.",
    "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards."
  },
  {
    SGX_ERROR_MEMORY_MAP_CONFLICT,
    "Memory map conflicted.",
    NULL
  },
  {
    SGX_ERROR_INVALID_METADATA,
    "Invalid enclave metadata.",
    NULL
  },
  {
    SGX_ERROR_DEVICE_BUSY,
    "SGX device was busy.",
    NULL
  },
  {
    SGX_ERROR_INVALID_VERSION,
    "Enclave version was invalid.",
    NULL
  },
  {
    SGX_ERROR_INVALID_ATTRIBUTE,
    "Enclave was not authorized.",
    NULL
  },
  {
    SGX_ERROR_ENCLAVE_FILE_ACCESS,
    "Can't open enclave file.",
    NULL
  },
};
void ecall_put_wrapper(const char *kdata, int ksize, const char *vdata, int vsize) {
  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
  ret = ecall_put(global_eid,kdata,ksize,vdata,vsize);
}

void ecall_get_wrapper(char *kdata, int ksize, char *vdata) {
  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
  ret = ecall_get(global_eid,kdata,ksize,vdata);
}
void ecall_ycsb_open_wrapper(void) {
  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
  ret = ecall_ycsb_open(global_eid);
  //if (ret != SGX_SUCCESS)
  //   abort();
}

void* ecall_compact_wrapper() {
  ecall_compact(global_eid);
}
void* ecall_fileiotest_wrapper() {
  ecall_fileiotest(global_eid);
}

void ecall_test_wrapper(void) {
  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
  ret = ecall_test(global_eid);
  //if (ret != SGX_SUCCESS)
  //   abort();
}
void ecall_bench_wrapper(void) {
  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
  ret = ecall_bench(global_eid);
  //if (ret != SGX_SUCCESS)
  //   abort();
}
void ecall_open_wrapper(void) {
  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
  ret = ecall_open(global_eid);
  //if (ret != SGX_SUCCESS)
  //   abort();
}

void ecall_writes_wrapper(int writes, int total) {
  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
  ret = ecall_writes(global_eid,writes, total);
  //if (ret != SGX_SUCCESS)
  //   abort();
}
void ecall_getproperty_wrapper() {
  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
  ret = ecall_getproperty(global_eid);
  //if (ret != SGX_SUCCESS)
  //   abort();
}
void ecall_reads_wrapper(int reads, int total) {
  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
  ret = ecall_reads(global_eid,reads,total);
  //if (ret != SGX_SUCCESS)
  //   abort();
}



/* Check error conditions for loading enclave */
void print_error_message(sgx_status_t ret)
{
  size_t idx = 0;
  size_t ttl = sizeof sgx_errlist/sizeof sgx_errlist[0];

  for (idx = 0; idx < ttl; idx++) {
    if(ret == sgx_errlist[idx].err) {
      if(NULL != sgx_errlist[idx].sug)
	printf("Info: %s\n", sgx_errlist[idx].sug);
      printf("Error: %s\n", sgx_errlist[idx].msg);
      break;
    }
  }

  if (idx == ttl)
    printf("Error code is 0x%X. Please refer to the \"Intel SGX SDK Developer Reference\" for more details.\n", ret);
}

/* Initialize the enclave:
 *   Step 1: try to retrieve the launch token saved by last transaction
 *   Step 2: call sgx_create_enclave to initialize an enclave instance
 *   Step 3: save the launch token if it is updated
 */
int initialize_enclave(void)
{
  char token_path[MAX_PATH] = {'\0'};
  sgx_launch_token_t token = {0};
  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
  int updated = 0;

  /* Step 1: try to retrieve the launch token saved by last transaction 
   *         if there is no token, then create a new one.
   */
  /* try to get the token saved in $HOME */
  const char *home_dir = getpwuid(getuid())->pw_dir;

  if (home_dir != NULL && 
      (strlen(home_dir)+strlen("/")+sizeof(TOKEN_FILENAME)+1) <= MAX_PATH) {
    /* compose the token path */
    strncpy(token_path, home_dir, strlen(home_dir));
    strncat(token_path, "/", strlen("/"));
    strncat(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME)+1);
  } else {
    /* if token path is too long or $HOME is NULL */
    strncpy(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME));
  }

  FILE *fp = fopen(token_path, "rb");
  if (fp == NULL && (fp = fopen(token_path, "wb")) == NULL) {
    printf("Warning: Failed to create/open the launch token file \"%s\".\n", token_path);
  }

  if (fp != NULL) {
    /* read the token from saved file */
    size_t read_num = fread(token, 1, sizeof(sgx_launch_token_t), fp);
    if (read_num != 0 && read_num != sizeof(sgx_launch_token_t)) {
      /* if token is invalid, clear the buffer */
      memset(&token, 0x0, sizeof(sgx_launch_token_t));
      printf("Warning: Invalid launch token read from \"%s\".\n", token_path);
    }
  }
  /* Step 2: call sgx_create_enclave to initialize an enclave instance */
  /* Debug Support: set 2nd parameter to 1 */
  ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, &token, &updated, &global_eid, NULL);
  if (ret != SGX_SUCCESS) {
    print_error_message(ret);
    if (fp != NULL) fclose(fp);
    return -1;
  }

  /* Step 3: save the launch token if it is updated */
  if (updated == FALSE || fp == NULL) {
    /* if the token is not updated, or file handler is invalid, do not perform saving */
    if (fp != NULL) fclose(fp);
    return 0;
  }

  /* reopen the file with write capablity */
  fp = freopen(token_path, "wb", fp);
  if (fp == NULL) return 0;
  size_t write_num = fwrite(token, 1, sizeof(sgx_launch_token_t), fp);
  if (write_num != sizeof(sgx_launch_token_t))
    printf("Warning: Failed to save launch token to \"%s\".\n", token_path);
  fclose(fp);
  return 0;
}

/* OCall functions */
void ocall_print_string(const char *str)
{
  /* Proxy/Bridge will check the length and null-terminate 
   * the input string to prevent buffer overflow. 
   */
  printf("%s", str);
}
void ocall_get_filesize(const char *str, uint64_t* res)
{
  struct stat sbuf;
  stat(str, &sbuf);
  *res = sbuf.st_size;
}
void ocall_allocate(long* res, int flag)
{
  //printf("in ocall_allocate and flag is %d\n",flag);
  if (flag) {
    char* bigmem = new char[1024*1024*1024];
    for(int i=0; i<1024*1024*1024; i++) bigmem[i]=0;
    *res = (long)bigmem;
  }
  else {
    char* bigmem = new char[1024*1024*4];
    for(int i=0; i<1024*1024*4; i++) bigmem[i]=0;
    *res = (long)bigmem;
  }
}
void ocall_allocate_specific(long *res, int size)
{
  char* value = new char[size];
  *res = (long)value;
}

void ocall_delete(long res) {
  char* mem = (char *) res;
  delete[] mem;
}

void ocall_allocate3(long* res, long* res1, long* res2)
{
  char* bigmem = new char[1024*1024*16];
  char* bigmem1 = new char[1024*1024*16];
  char* bigmem2 = new char[1024*1024*16];
  for(int i=0; i<1024*1024*16; i++) bigmem[i]=0;
  for(int i=0; i<1024*1024*16; i++) bigmem1[i]=0;
  for(int i=0; i<1024*1024*16; i++) bigmem2[i]=0;
  *res = (long)bigmem;
  *res1 = (long)bigmem1;
  *res2 = (long)bigmem2;
}


void ocall_file_exists(const char *str, int* res)
{
  /* Proxy/Bridge will check the length and null-terminate 
   * the input string to prevent buffer overflow. 
   */
  if (access(str, F_OK) == 0)
    *res = 0;
  else
    *res = 1;
  return;
}

void ocall_create_dir(const char *str) {
  mkdir(str, 0755);
}

void ocall_delete_file(const char *str) {
  unlink(str);
}

void ocall_rename_file(const char *str1, const char *str2) {
  rename(str1, str2);
}


void ocall_fopen(const char *fname, const char *mode, long* res) {
  int tc = gopen.load();
  tc++;
  gopen.store(tc);
  //printf("ocall open is %d\n",tc);
  FILE* fp;
  fp = fopen(fname,mode);
  *res = (long)fp;
}
std::vector<std::string> result;
void ocall_getchildren(const char *fname, long* res, int* size) {
  DIR* d = opendir(fname);
  struct dirent* entry;
  while ((entry = readdir(d)) != NULL) {
    result.push_back(entry->d_name);
  }
  closedir(d);
  *res = (long)(&result);
  *size = result.size();
}
void ocall_checkchildren(long res,int i, char* filename) {
  std::vector<std::string>* tmp = (std::vector<std::string>*) res;
  memcpy(filename,tmp->at(i).c_str(),strlen(tmp->at(i).c_str()));
}
void ocall_fwrite(const char *data, int size, int count, long file, int *res) {
  static int start = 0, end = 0;
  FILE* fp = (FILE *) file;
  *res = fwrite(data,size,count,fp);
  if (count>10) {
    //	int tc = gwrite.load();
    //	tc++;
    //	gwrite.store(tc);
    //	printf("ocall write is %d and size is %d and filename is %p\n",tc,count,fp);
  } else {
    //	int tc = gwritesmall.load();
    //	tc++;
    //	gwritesmall.store(tc);
    //	printf("ocall write small is  %d and size is %d and filename is %p\n",tc,count,fp);
  }
  return;
}
void ocall_fread(char *data, int size, int count, long file, int *res) {
  FILE* fp = (FILE *) file;
  char* tmp = new char[100];
  *res = fread(data,size,count,fp);
  //	int tc = gread.load();
  //	tc++;
  //	gread.store(tc);
  //	printf("ocall read is %d\n",tc);
  return;
}

void ocall_fread_outside(long data, int size, int count, long file, int *res) {
  FILE* fp = (FILE *) file;
  *res = fread((char *)data,size,count,fp);
  //int tc = gread.load();
  //tc++;
  //gread.store(tc);
  //printf("ocall read is %d\n",tc);
  return;
}
struct thread_state {
  long arg;
  int type;
};
void* ecall_startthread_wrapper(void* arg) {
  thread_state* state = reinterpret_cast<thread_state*>(arg);
  ecall_startthread(global_eid,state->arg,state->type);
  delete state;
}

void ocall_pthread_create(int type, long ls) {
  pthread_t t;
  thread_state* state = new thread_state;
  state->arg = ls;
  state->type = type;
  pthread_create(&t, NULL,  &ecall_startthread_wrapper, state);
}

void ocall_fflush(long file, int *res) {
  FILE* fp = (FILE *) file;
  *res = fflush(fp);
  return;
}

void ocall_fclose(long file, int *res) {
  FILE* fp = (FILE *) file;
  *res = fclose(fp);
  return;
}
void ocall_feof(long file, int *res) {
  FILE* fp = (FILE *) file;
  *res = feof(fp);
  return;
}
void ocall_fseek(long file, int offset,int *res) {
  FILE* fp = (FILE *) file;
  *res = fseek(fp,offset,SEEK_CUR);
  return;
}

int ycsb_main(const int argc, const char *argv[]);
/* Application entry */
void minibench(int argc, const char *argv[])
{
  int writes = std::stoi(argv[2]);
  int reads = std::stoi(argv[3]);
  int write_space = std::stoi(argv[4]);
  int read_space  = std::stoi(argv[5]);
  int ifwrite  = std::stoi(argv[6]);
  int ifread  = std::stoi(argv[7]);
  int ifprop  = std::stoi(argv[8]);
  struct timeval start,end;
  ecall_open_wrapper();
  gettimeofday(&start,NULL);
  if (ifwrite)
    ecall_writes_wrapper(writes,write_space);
  if (ifread) {
    printf("goging to ecall read!\n");
    ecall_reads_wrapper(reads,read_space);
  }
  if (ifprop) {
    ecall_getproperty_wrapper();
    //	ecall_compact_wrapper();
    //	ecall_getproperty_wrapper();
  }
  gettimeofday(&end,NULL);
  printf("rimw apwnr =%ld\n",(end.tv_sec - start.tv_sec)*1000000+(end.tv_usec-start.tv_usec));
  /* Destroy the enclave */
  sgx_destroy_enclave(global_eid);

  printf("Info: SampleEnclave successfully returned.\n");
}


/* Application entry */
int SGX_CDECL main(int argc, const char *argv[])
{
  (void)(argc);
  (void)(argv);

  /* Initialize the enclave */
  if(initialize_enclave() < 0){
    printf("Enter a character before exit ...\n");
    getchar();
    return -1;
  }
  int ismini = std::stoi(argv[1]);
  if (ismini)
    minibench(argc,argv);
  else
    ycsb_main(argc,argv);
  return 0;
}
