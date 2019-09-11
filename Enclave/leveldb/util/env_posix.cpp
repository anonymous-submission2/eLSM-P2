// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

//#include <dirent.h>
#include <errno.h>
//#include <fcntl.h>
//#include <sgx_thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <sys/mman.h>
//#include <sys/resource.h>
//#include <sys/stat.h>
//#include <sys/time.h>
//#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <deque>
#include <limits>
#include <set>
#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "port/port.h"
#include "util/logging.h"
#include "util/mutexlock.h"
//sgx port
//#include "util/posix_logger.h"
#include "util/env_posix_test_helper.h"
#include "sgx_thread.h"
#include "sgx_tprotected_fs.h"
#include "Enclave.h"
#include "Enclave_t.h"

namespace leveldb {

  namespace  {

    static int open_read_only_file_limit = -1;
    static int mmap_limit = -1;

    static Status IOError(const std::string& context, int err_number) {
      return Status::IOError(context, strerror(err_number));
    }

    // Helper class to limit resource usage to avoid exhaustion.
    // Currently used to limit read-only file descriptors and mmap file usage
    // so that we do not end up running out of file descriptors, virtual memory,
    // or running into kernel performance problems for very large databases.
    class Limiter {
      public:
	// Limit maximum number of resources to |n|.
	Limiter(intptr_t n) {
	  SetAllowed(n);
	}

	// If another resource is available, acquire it and return true.
	// Else return false.
	bool Acquire() {
	  if (GetAllowed() <= 0) {
	    return false;
	  }
	  MutexLock l(&mu_);
	  intptr_t x = GetAllowed();
	  if (x <= 0) {
	    return false;
	  } else {
	    SetAllowed(x - 1);
	    return true;
	  }
	}

	// Release a resource acquired by a previous call to Acquire() that returned
	// true.
	void Release() {
	  MutexLock l(&mu_);
	  SetAllowed(GetAllowed() + 1);
	}

      private:
	port::Mutex mu_;
	port::AtomicPointer allowed_;

	intptr_t GetAllowed() const {
	  return reinterpret_cast<intptr_t>(allowed_.Acquire_Load());
	}

	// REQUIRES: mu_ must be held
	void SetAllowed(intptr_t v) {
	  allowed_.Release_Store(reinterpret_cast<void*>(v));
	}

	Limiter(const Limiter&);
	void operator=(const Limiter&);
    };

    class PosixSequentialFile: public SequentialFile {
      private:
	std::string filename_;
	// sgx_port
	SGX_FILE* file_;

      public:
	//sgx port
	PosixSequentialFile(const std::string& fname, SGX_FILE* f)
	  : filename_(fname), file_(f) { }
	PosixSequentialFile(const std::string& fname)
	  : filename_(fname) { }
	virtual ~PosixSequentialFile() { ;}//fclose(file_); }

    virtual Status Read(size_t n, Slice* result, char* scratch) {
      Status s;
      size_t r = sgx_fread(scratch, 1, n, file_);
      *result = Slice(scratch, r);
      if (r < n) {
	//sgx port
	if (sgx_feof(file_)) {
	  // We leave status as ok if we hit the end of the file
	} else {
	  // A partial read with an error: return a non-ok status
	  s = IOError(filename_, errno);
	}
      }
      return s;
    }

    virtual Status Skip(uint64_t n) {
      //sgx port
      if (sgx_fseek(file_, n, SEEK_CUR)) {
	return IOError(filename_, errno);
      }
      return Status::OK();
    }
  };

class PosixUnSecureSequentialFile: public SequentialFile {
      private:
	std::string filename_;
	// sgx_port
	long file_;

      public:
	//sgx port
	PosixUnSecureSequentialFile(const std::string& fname, long f)
	  : filename_(fname), file_(f) { }
	PosixUnSecureSequentialFile(const std::string& fname)
	  : filename_(fname) { }
	virtual ~PosixUnSecureSequentialFile() { int res = 0;ocall_fclose(file_,&res); }

    virtual Status Read(size_t n, Slice* result, char* scratch) {
      Status s;
      int r = 0;
      ocall_fread(scratch, 1, n, file_, &r);
      *result = Slice(scratch, r);
      if (r < n) {
	//sgx port
	int res = 0;
	 ocall_feof(file_,&res);
	if (res) {
	  // We leave status as ok if we hit the end of the file
	} else {
	  // A partial read with an error: return a non-ok status
	  s = IOError(filename_, errno);
	}
      }
      return s;
    }

    virtual Status Skip(uint64_t n) {
      //sgx port
      int res = 0;
      ocall_fseek(file_,n,&res);
      if (res) {
	return IOError(filename_, errno);
      }
      return Status::OK();
    }
  };
  // pread() based random-access
  class UnSecurePosixRandomAccessFile: public RandomAccessFile {
    private:
      std::string filename_;
      bool temporary_fd_;  // If true, fd_ is -1 and we open on every read.
      int fd_;
      Limiter* limiter_;

    public:
      UnSecurePosixRandomAccessFile(const std::string& fname, int fd, Limiter* limiter)
	: filename_(fname), fd_(fd), limiter_(limiter) {
	  temporary_fd_ = !limiter->Acquire();
	  if (temporary_fd_) {
	    // Open file on every access.
	    //sgx port
	    //close(fd_);
	    fd_ = -1;
	  }
	}

      virtual ~UnSecurePosixRandomAccessFile() {
	if (!temporary_fd_) {
	  // sgx port
	  //close(fd_);
	  limiter_->Release();
	}
      }

      virtual Status Read(int flag,uint64_t offset, size_t n, Slice* result,
	  char* scratch) const {
	int fd = fd_;
	//if (temporary_fd_) {
	  // sgx port
	  //fd = open(filename_.c_str(), O_RDONLY);
	//  if (fd < 0) {
	 //   return IOError(filename_, errno);
	 // }
	//}

	Status s;
	// sgx port
	long f = 0; 
	int res = 0;
	int r = 0;
	ocall_fopen(filename_.c_str(),"r",&f);
	//ssize_t r = 0;//pread(fd, scratch, n, static_cast<off_t>(offset));
        ocall_fseek(f,offset,&res);
	if (flag)
        ocall_fread(scratch, 1, n, f,&r);
	else
	ocall_fread_outside((long)scratch,1,n,f,&r);
	*result = Slice(scratch, (r < 0) ? 0 : r);
	if (r < 0) {
	  // An error: return a non-ok status
	  s = IOError(filename_, errno);
	}
	if (temporary_fd_) {
	  // Close the temporary file descriptor opened earlier.
	  // sgx port
	  //close(fd);
	}
        ocall_fclose(f,&res);
	return s;
      }
  };

  // pread() based random-access
  class PosixRandomAccessFile: public RandomAccessFile {
    private:
      std::string filename_;
      bool temporary_fd_;  // If true, fd_ is -1 and we open on every read.
      int fd_;
      Limiter* limiter_;

    public:
      PosixRandomAccessFile(const std::string& fname, int fd, Limiter* limiter)
	: filename_(fname), fd_(fd), limiter_(limiter) {
	  temporary_fd_ = !limiter->Acquire();
	  if (temporary_fd_) {
	    // Open file on every access.
	    //sgx port
	    //close(fd_);
	    fd_ = -1;
	  }
	}

      virtual ~PosixRandomAccessFile() {
	if (!temporary_fd_) {
	  // sgx port
	  //close(fd_);
	  limiter_->Release();
	}
      }

      virtual Status Read(int flag,uint64_t offset, size_t n, Slice* result,
	  char* scratch) const {
	int fd = fd_;
	//if (temporary_fd_) {
	  // sgx port
	  //fd = open(filename_.c_str(), O_RDONLY);
	//  if (fd < 0) {
	 //   return IOError(filename_, errno);
	 // }
	//}

	Status s;
	// sgx port
	SGX_FILE* file_ = sgx_fopen_auto_key(filename_.c_str(),"r");
	if (file_ == NULL) 

	//ssize_t r = 0;//pread(fd, scratch, n, static_cast<off_t>(offset));
        sgx_fseek(file_,n,SEEK_CUR);
        size_t r = sgx_fread(scratch, 1, n, file_);
	*result = Slice(scratch, (r < 0) ? 0 : r);
	if (r < 0) {
	  // An error: return a non-ok status
	  s = IOError(filename_, errno);
	}
	if (temporary_fd_) {
	  // Close the temporary file descriptor opened earlier.
	  // sgx port
	  //close(fd);
	}
        sgx_fclose(file_);
	return s;
      }
  };

  // mmap() based random-access
  class PosixMmapReadableFile: public RandomAccessFile {
    private:
      std::string filename_;
      void* mmapped_region_;
      size_t length_;
      Limiter* limiter_;

    public:
      // base[0,length-1] contains the mmapped contents of the file.
      PosixMmapReadableFile(const std::string& fname, void* base, size_t length,
	  Limiter* limiter)
	: filename_(fname), mmapped_region_(base), length_(length),
	limiter_(limiter) {
	}

      virtual ~PosixMmapReadableFile() {
	//sgx thread
	//munmap(mmapped_region_, length_);
	limiter_->Release();
      }

      virtual Status Read(uint64_t offset, size_t n, Slice* result,
	  char* scratch) const {
	Status s;
	if (offset + n > length_) {
	  *result = Slice();
	  s = IOError(filename_, EINVAL);
	} else {
	  *result = Slice(reinterpret_cast<char*>(mmapped_region_) + offset, n);
	}
	return s;
      }
  };

class UnSecurePosixWritableFile : public WritableFile {
    private:
      std::string filename_;
      // sgx port
      long file_;

    public:
      UnSecurePosixWritableFile(const std::string& fname, long file)
	: filename_(fname),file_(file) { }

      ~UnSecurePosixWritableFile() {
	int res = 0;
	if (file_ != NULL) {
	  // Ignoring any potential errors
	  ocall_fclose(file_,&res);
	}
      }

      virtual Status Append(const Slice& data) {
	// sgx prot
	int res = 0;
	ocall_fwrite(data.data(), 1, data.size(), file_, &res);
	if (res != data.size()) {
	  return IOError(filename_, errno);
	}
	return Status::OK();
      }

      virtual Status Close() {
	Status result;
	int res = 0;
	// sgx port
	ocall_fclose(file_,&res);
	if (res != 0) {
	  result = IOError(filename_, errno);
	}
	file_ = NULL;
	return result;
      }

      virtual Status Flush() {
	int res = 0;
	ocall_fflush(file_,&res);
	if (res != 0) {
	  return IOError(filename_, errno);
	}
	return Status::OK();
      }

      Status SyncDirIfManifest() {
	const char* f = filename_.c_str();
	const char* sep = strrchr(f, '/');
	Slice basename;
	std::string dir;
	if (sep == NULL) {
	  dir = ".";
	  basename = f;
	} else {
	  dir = std::string(f, sep - f);
	  basename = sep + 1;
	}
	Status s;
	if (basename.starts_with("MANIFEST")) {
	  //int fd = open(dir.c_str(), O_RDONLY);
	  //if (fd < 0) {
	  //  s = IOError(dir, errno);
	  //} else {
	  //  if (fsync(fd) < 0) {
	  //    s = IOError(dir, errno);
	  //  }
	  //  close(fd);
	  // }
	}
	return s;
      }

      virtual Status Sync() {
	// Ensure new files referred to by the manifest are in the filesystem.
	Status s = SyncDirIfManifest();
	if (!s.ok()) {
	  return s;
	}
	int res = 0;
	ocall_fflush(file_,&res);
	if (res != 0) {
	  // fdatasync(fileno(file_)) != 0) {
	  s = Status::IOError(filename_, strerror(errno));
	}
	return s;
	}
      };

  class PosixWritableFile : public WritableFile {
    private:
      std::string filename_;
      // sgx port
      SGX_FILE* file_;

    public:
      PosixWritableFile(const std::string& fname, SGX_FILE* file)
	: filename_(fname),file_(file) { }

      ~PosixWritableFile() {
	if (file_ != NULL) {
	  // Ignoring any potential errors
	  sgx_fclose(file_);
	}
      }

      virtual Status Append(const Slice& data) {
	// sgx prot
	//printf("calling sgx_fwrite with data size is %d\n",data.size());
	size_t r = sgx_fwrite(data.data(), 1, data.size(), file_);
	if (r != data.size()) {
	  int e = sgx_ferror(file_);
	  return IOError(filename_, errno);
	}
	return Status::OK();
      }

      virtual Status Close() {
	Status result;
	// sgx port
	if (sgx_fclose(file_) != 0) {
	  result = IOError(filename_, errno);
	}
	file_ = NULL;
	return result;
      }

      virtual Status Flush() {
	if (sgx_fflush(file_) != 0) {
	  return IOError(filename_, errno);
	}
	return Status::OK();
      }

      Status SyncDirIfManifest() {
	const char* f = filename_.c_str();
	const char* sep = strrchr(f, '/');
	Slice basename;
	std::string dir;
	if (sep == NULL) {
	  dir = ".";
	  basename = f;
	} else {
	  dir = std::string(f, sep - f);
	  basename = sep + 1;
	}
	Status s;
	if (basename.starts_with("MANIFEST")) {
	  //int fd = open(dir.c_str(), O_RDONLY);
	  //if (fd < 0) {
	  //  s = IOError(dir, errno);
	  //} else {
	  //  if (fsync(fd) < 0) {
	  //    s = IOError(dir, errno);
	  //  }
	  //  close(fd);
	  // }
	}
	return s;
      }

      virtual Status Sync() {
	// Ensure new files referred to by the manifest are in the filesystem.
	Status s = SyncDirIfManifest();
	if (!s.ok()) {
	  return s;
	}
	if (sgx_fflush(file_) != 0) {
	  // fdatasync(fileno(file_)) != 0) {
	  s = Status::IOError(filename_, strerror(errno));
	}
	return s;
	}
      };

      static int LockOrUnlock(int fd, bool lock) {
	errno = 0;
	//struct flock f;
	//memset(&f, 0, sizeof(f));
	//f.l_type = (lock ? F_WRLCK : F_UNLCK);
	//f.l_whence = SEEK_SET;
	//f.l_start = 0;
	//f.l_len = 0;        // Lock/unlock entire file
	//return fcntl(fd, F_SETLK, &f);
	return 0;
      }

      class PosixFileLock : public FileLock {
	public:
	  int fd_;
	  std::string name_;
      };

      // Set of locked files.  We keep a separate set instead of just
      // relying on fcntrl(F_SETLK) since fcntl(F_SETLK) does not provide
      // any protection against multiple uses from the same process.
      class PosixLockTable {
	private:
	  port::Mutex mu_;
	  std::set<std::string> locked_files_;
	public:
	  bool Insert(const std::string& fname) {
	    MutexLock l(&mu_);
	    return locked_files_.insert(fname).second;
	  }
	  void Remove(const std::string& fname) {
	    MutexLock l(&mu_);
	    locked_files_.erase(fname);
	  }
      };

      class PosixEnv : public Env {
	public:
	  PosixEnv();
	  virtual ~PosixEnv() {
	    char msg[] = "Destroying Env::Default()\n";
	    //fwrite(msg, 1, sizeof(msg), stderr);
	    abort();
	  }

	  virtual Status NewSequentialFile(const std::string& fname,
	      SequentialFile** result) {
	    SGX_FILE* f = sgx_fopen_auto_key(fname.c_str(), "r");
	    if (f == NULL) {
	      *result = NULL;
	      return IOError(fname, errno);
	    } else {
	      *result = new PosixSequentialFile(fname, f);
	      return Status::OK();
	    }
	  }
	virtual Status NewUnSecureSequentialFile(const std::string& fname,
	      SequentialFile** result) {
	    long f;
            ocall_fopen(fname.c_str(), "r", &f);
	    if (f == NULL) {
	      *result = NULL;
	      return IOError(fname, errno);
	    } else {
	      *result = new PosixUnSecureSequentialFile(fname, f);
	      return Status::OK();
	    }
	  }


	  virtual Status NewRandomAccessFile(const std::string& fname,
	      RandomAccessFile** result) {
	    *result = NULL;
	    Status s;
	    int fd = 0;//open(fname.c_str(), O_RDONLY);
	    if (fd < 0) {
	      s = IOError(fname, errno);
	   // } else if (mmap_limit_.Acquire()) {
	    } else if (false) {
	      uint64_t size;
	      s = GetFileSize(fname, &size);
	      if (s.ok()) {
		void* base = NULL;//mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
		//if (base != MAP_FAILED) {
		//  *result = new PosixMmapReadableFile(fname, base, size, &mmap_limit_);
		//} else {
		// s = IOError(fname, errno);
		// }
	      }
	      //close(fd);
	      if (!s.ok()) {
		mmap_limit_.Release();
	      }
	    } else {
	      *result = new UnSecurePosixRandomAccessFile(fname, fd, &fd_limit_);
	    }
	    return s;
	  }
    virtual Status NewSpecialRandomAccessFile(const std::string& fname,
	      RandomAccessFile** result) {
	    *result = NULL;
	    Status s;
	    int fd = 0;//open(fname.c_str(), O_RDONLY);
	    if (fd < 0) {
	      s = IOError(fname, errno);
	   // } else if (mmap_limit_.Acquire()) {
	    } else if (false) {
	      uint64_t size;
	      s = GetFileSize(fname, &size);
	      if (s.ok()) {
		void* base = NULL;//mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
		//if (base != MAP_FAILED) {
		//  *result = new PosixMmapReadableFile(fname, base, size, &mmap_limit_);
		//} else {
		// s = IOError(fname, errno);
		// }
	      }
	      //close(fd);
	      if (!s.ok()) {
		mmap_limit_.Release();
	      }
	    } else {
	      *result = new PosixRandomAccessFile(fname, fd, &fd_limit_);
	    }
	    return s;
	  }


	  virtual Status NewWritableFile(const std::string& fname,
	      WritableFile** result) {
	    Status s;
	    SGX_FILE* f = sgx_fopen_auto_key(fname.c_str(), "w");
	    if (f == NULL) {
	      *result = NULL;
	      s = IOError(fname, errno);
	    } else {
	      *result = new PosixWritableFile(fname, f);
	    }
	    return s;
	  }

	  virtual Status NewUnSecureWritableFile(const std::string& fname,
	      WritableFile** result) {
	    Status s;
	    long f = 0;
	    ocall_fopen(fname.c_str(), "w", &f);
	    if (f == NULL) {
	      *result = NULL;
	      s = IOError(fname, errno);
	    } else {
	      *result = new UnSecurePosixWritableFile(fname, f);
	    }
	    return s;
	  }
	

	  virtual Status NewAppendableFile(const std::string& fname,
	      WritableFile** result) {
	    Status s;
	    SGX_FILE* f = sgx_fopen_auto_key(fname.c_str(), "a");
	    if (f == NULL) {
	      *result = NULL;
	      s = IOError(fname, errno);
	    } else {
	      *result = new PosixWritableFile(fname, f);
	    }
	    return s;
	  }

	virtual Status NewUnSecureAppendableFile(const std::string& fname,
	      WritableFile** result) {
	    Status s;
	    long f = 0;
	    ocall_fopen(fname.c_str(),"a",&f);
	    if (f == NULL) {
	      *result = NULL;
	      s = IOError(fname, errno);
	    } else {
	      *result = new UnSecurePosixWritableFile(fname, f);
	    }
	    return s;
	  }


	  virtual bool FileExists(const std::string& fname) {
	    int res = 0;
	    ocall_file_exists(fname.c_str(), &res);
	    // return access(fname.c_str(), F_OK) == 0;
	    if (res) 
	      return false;
	    return true;
	  }

	  virtual Status GetChildren(const std::string& dir,
	      std::vector<std::string>* result) {
	    result->clear();
	    long f = 0;
	    int size = 0;
	    char onefile[100];
	    ocall_getchildren(dir.c_str(),&f, &size);
	    for (int i=0;i<size;i++) {
	      ocall_checkchildren(f,i,onefile);
	      result->push_back(onefile);
            }
	    //DIR* d = opendir(dir.c_str());
	    //if (d == NULL) {
	    //  return IOError(dir, errno);
	    // }
	    //  struct dirent* entry;
	    //  while ((entry = readdir(d)) != NULL) {
	    //    result->push_back(entry->d_name);
	    //  }
	    //  closedir(d);
	    //std::vector<std::string>* tmp  = (std::vector<std::string>*)f; 
	    //for (int i=0;i<tmp->size();i++)
	    return Status::OK();
	  }

	  virtual Status DeleteFile(const std::string& fname) {
	    Status result;
	    ocall_delete_file(fname.c_str());
	    //  if (unlink(fname.c_str()) != 0) {
	    //    result = IOError(fname, errno);
	    //  }
	    return Status::OK();
	  }

	  virtual Status CreateDir(const std::string& name) {
	    Status result;
	    ocall_create_dir(name.c_str());
	    // if (mkdir(name.c_str(), 0755) != 0) {
	    //   result = IOError(name, errno);
	    // }
	    return Status::OK();
	  }

	  virtual Status DeleteDir(const std::string& name) {
	    Status result;
	    // if (rmdir(name.c_str()) != 0) {
	    //   result = IOError(name, errno);
	    // }
	    return result;
	  }

	  virtual Status GetFileSize(const std::string& fname, uint64_t* size) {
	    Status s;
	    uint64_t res = 0;
            ocall_get_filesize(fname.c_str(), &res);
            *size = res;
	    return Status::OK();
	  }

	  virtual Status RenameFile(const std::string& src, const std::string& target) {
	    Status result;
            ocall_rename_file(src.c_str(),target.c_str());
	    // if (rename(src.c_str(), target.c_str()) != 0) {
	    //   result = IOError(src, errno);
	    // }
	    return Status::OK();
	  }

	  virtual Status LockFile(const std::string& fname, FileLock** lock) {
	    *lock = NULL;
	    Status result;
	    /*
	       int fd = open(fname.c_str(), O_RDWR | O_CREAT, 0644);
	       if (fd < 0) {
	       result = IOError(fname, errno);
	       } else if (!locks_.Insert(fname)) {
	       close(fd);
	       result = Status::IOError("lock " + fname, "already held by process");
	       } else if (LockOrUnlock(fd, true) == -1) {
	       result = IOError("lock " + fname, errno);
	       close(fd);
	       locks_.Remove(fname);
	       } else {
	       PosixFileLock* my_lock = new PosixFileLock;
	       my_lock->fd_ = fd;
	       my_lock->name_ = fname;
	     *lock = my_lock;
	     }
	     */
	    return result;
	  }

	  virtual Status UnlockFile(FileLock* lock) {
	    PosixFileLock* my_lock = reinterpret_cast<PosixFileLock*>(lock);
	    Status result;
	    if (LockOrUnlock(my_lock->fd_, false) == -1) {
	      result = IOError("unlock", errno);
	    }
	    locks_.Remove(my_lock->name_);
	    //close(my_lock->fd_);
	    delete my_lock;
	    return result;
	  }

	  virtual void Schedule(void (*function)(void*), void* arg);

	  virtual void StartThread(void (*function)(void* arg), void* arg);

	  virtual Status GetTestDirectory(std::string* result) {
	    /*
	       const char* env = getenv("TEST_TMPDIR");
	       if (env && env[0] != '\0') {
	     *result = env;
	     } else {
	     char buf[100];
	     snprintf(buf, sizeof(buf), "/tmp/leveldbtest-%d", int(geteuid()));
	     *result = buf;
	     }
	    // Directory may already exist
	    CreateDir(*result);
	     */
	    return Status::OK();
	  }

	  static uint64_t gettid() {
	    sgx_thread_t tid = sgx_thread_self();
	    uint64_t thread_id = 0;
	    memcpy(&thread_id, &tid, std::min(sizeof(thread_id), sizeof(tid)));
	    return thread_id;
	  }

	  virtual Status NewLogger(const std::string& fname, Logger** result) {
	    //FILE* f = fopen(fname.c_str(), "w");
	    //if (f == NULL) {
	    //  *result = NULL;
	    //  return IOError(fname, errno);
	    //} else {
	    // *result = new PosixLogger(f, &PosixEnv::gettid);
	    return Status::OK();
	    //}
	  }

	  virtual uint64_t NowMicros() {
	    //struct timeval tv;
	    //gettimeofday(&tv, NULL);
	    //return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
	    return 0;
	  }

	  virtual void SleepForMicroseconds(int micros) {
	    //usleep(micros);
	  }

	static void* BGThreadWrapperLong(long arg) {
	    reinterpret_cast<PosixEnv*>(arg)->BGThread();
	    return NULL;
          }


	private:
	  void PthreadCall(const char* label, int result) {
	    if (result != 0) {
	      //fprintf(stderr, "sgx_thread %s: %s\n", label, strerror(result));
	      abort();
	    }
	  }

	  // BGThread() is the body of the background thread
	  void BGThread();
          	  static void* BGThreadWrapper(void* arg) {
	    reinterpret_cast<PosixEnv*>(arg)->BGThread();
	    return NULL;
	  }

	  sgx_thread_mutex_t mu_;
	  sgx_thread_cond_t bgsignal_;
	  sgx_thread_t bgthread_;
	  bool started_bgthread_;

	  // Entry per Schedule() call
	  struct BGItem { void* arg; void (*function)(void*); };
	  typedef std::deque<BGItem> BGQueue;
	  BGQueue queue_;

	  PosixLockTable locks_;
	  Limiter mmap_limit_;
	  Limiter fd_limit_;
      };

      // Return the maximum number of concurrent mmaps.
      static int MaxMmaps() {
	if (mmap_limit >= 0) {
	  return mmap_limit;
	}
	// Up to 1000 mmaps for 64-bit binaries; none for smaller pointer sizes.
	mmap_limit = sizeof(void*) >= 8 ? 1000 : 0;
	return mmap_limit;
      }

      // Return the maximum number of read-only files to keep open.
      static intptr_t MaxOpenFiles() {
	if (open_read_only_file_limit >= 0) {
	  return open_read_only_file_limit;
	}
	//struct rlimit rlim;
	//if (getrlimit(RLIMIT_NOFILE, &rlim)) {
	// getrlimit failed, fallback to hard-coded default.
	open_read_only_file_limit = 50;
	//} else if (rlim.rlim_cur == RLIM_INFINITY) {
	//  open_read_only_file_limit = std::numeric_limits<int>::max();
	// } else {
	// Allow use of 20% of available file descriptors for read-only files.
	//  open_read_only_file_limit = rlim.rlim_cur / 5;
	//}
	return open_read_only_file_limit;
      }

      PosixEnv::PosixEnv()
	: started_bgthread_(false),
	mmap_limit_(MaxMmaps()),
	fd_limit_(MaxOpenFiles()) {
	  PthreadCall("mutex_init", sgx_thread_mutex_init(&mu_, NULL));
	  PthreadCall("cvar_init", sgx_thread_cond_init(&bgsignal_, NULL));
	}

      void PosixEnv::Schedule(void (*function)(void*), void* arg) {
	PthreadCall("lock", sgx_thread_mutex_lock(&mu_));

	// Start background thread if necessary
	if (!started_bgthread_) {
	  started_bgthread_ = true;
	  //PthreadCall(
	  //    "create thread",
	  //    sgx_thread_create(&bgthread_, NULL,  &PosixEnv::BGThreadWrapper, this));
        long ls = (long)this;
	ocall_pthread_create(2,ls);
	}

	// If the queue is currently empty, the background thread may currently be
	// waiting.
	if (queue_.empty()) {
	  PthreadCall("signal", sgx_thread_cond_signal(&bgsignal_));
	}

	// Add to priority queue
	queue_.push_back(BGItem());
	queue_.back().function = function;
	queue_.back().arg = arg;

	PthreadCall("unlock", sgx_thread_mutex_unlock(&mu_));
      }

      void PosixEnv::BGThread() {
	while (true) {
	  // Wait until there is an item that is ready to run
	  PthreadCall("lock", sgx_thread_mutex_lock(&mu_));
	  while (queue_.empty()) {
	    PthreadCall("wait",sgx_thread_cond_wait(&bgsignal_, &mu_));
	  }

	  void (*function)(void*) = queue_.front().function;
	  void* arg = queue_.front().arg;
	  queue_.pop_front();

	  PthreadCall("unlock",sgx_thread_mutex_unlock(&mu_));
	  (*function)(arg);
	}
      }
      namespace {
	struct StartThreadState {
	  void (*user_function)(void*);
	  void* arg;
	};
      }
      static void* StartThreadWrapperLong(long arg) {
	StartThreadState* state = reinterpret_cast<StartThreadState*>(arg);
	state->user_function(state->arg);
	delete state;
	return NULL;
      }
      static void* StartThreadWrapper(void* arg) {
	StartThreadState* state = reinterpret_cast<StartThreadState*>(arg);
	state->user_function(state->arg);
	delete state;
	return NULL;
      }
      

      void PosixEnv::StartThread(void (*function)(void* arg), void* arg) {
	sgx_thread_t t;
	StartThreadState* state = new StartThreadState;
	state->user_function = function;
	state->arg = arg;
	//PthreadCall("start thread",
	  //         sgx_thread_create(&t, NULL,  &StartThreadWrapper, state));
        long ls = (long)state;
	ocall_pthread_create(1,ls);
      }

  }  // namespace

  //static sgx_thread_once_t once = PTHREAD_ONCE_INIT;
  static Env* default_env;
  static void InitDefaultEnv() { default_env = new PosixEnv; }

  void EnvPosixTestHelper::SetReadOnlyFDLimit(int limit) {
    assert(default_env == NULL);
    open_read_only_file_limit = limit;
  }

  void EnvPosixTestHelper::SetReadOnlyMMapLimit(int limit) {
    assert(default_env == NULL);
    mmap_limit = limit;
  }

  Env* Env::Default() {
    InitDefaultEnv();
    //sgx_thread_once(&once, InitDefaultEnv);
    return default_env;
  }

}  // namespace leveldb
void ecall_startthread(long ls,int type) {
	if (type == 1)
	leveldb::StartThreadWrapperLong(ls);
        else
	leveldb::PosixEnv::BGThreadWrapperLong(ls);
}
