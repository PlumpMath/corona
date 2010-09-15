// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Platform specific code for MacOS goes here. For the POSIX comaptible parts
// the implementation is in platform-posix.cc.

#include <unistd.h>
#include <sys/mman.h>
#include <mach/mach_init.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>

#include <AvailabilityMacros.h>

#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <libkern/OSAtomic.h>
#include <mach/mach.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#include <mach/vm_statistics.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>

#include <errno.h>

#undef MAP_TYPE

#include "v8.h"

#include "platform.h"

#if 0
#define PGLOG(x) \
  do { \
    printf x; \
  } while (0)
#else
#define PGLOG(x) do {} while(0)
#endif

// Manually define these here as weak imports, rather than including execinfo.h.
// This lets us launch on 10.4 which does not have these calls.
extern "C" {
  extern int backtrace(void**, int) __attribute__((weak_import));
  extern char** backtrace_symbols(void* const*, int)
      __attribute__((weak_import));
  extern void backtrace_symbols_fd(void* const*, int, int)
      __attribute__((weak_import));
}


namespace v8 {
namespace internal {


class MainThread : public Thread {
  public:
    void Run() {
      UNREACHABLE();
    }
} main_th;

// Export some scheduler data structures to whoever wants to look at them
Thread *main_thread = &main_th;
Thread *current_thread = main_thread;



double ceiling(double x) {
  // Correct Mac OS X Leopard 'ceil' behavior.
  if (-1.0 < x && x < 0.0) {
    return -0.0;
  } else {
    return ceil(x);
  }
}


void OS::Setup() {
  // Seed the random number generator.
  // Convert the current time to a 64-bit integer first, before converting it
  // to an unsigned. Going directly will cause an overflow and the seed to be
  // set to all ones. The seed will be identical for different instances that
  // call this setup code within the same millisecond.
  uint64_t seed = static_cast<uint64_t>(TimeCurrentMillis());
  srandom(static_cast<unsigned int>(seed));
}


// We keep the lowest and highest addresses mapped as a quick way of
// determining that pointers are outside the heap (used mostly in assertions
// and verification).  The estimate is conservative, ie, not all addresses in
// 'allocated' space are actually allocated to our heap.  The range is
// [lowest, highest), inclusive on the low and and exclusive on the high end.
static void* lowest_ever_allocated = reinterpret_cast<void*>(-1);
static void* highest_ever_allocated = reinterpret_cast<void*>(0);


static void UpdateAllocatedSpaceLimits(void* address, int size) {
  lowest_ever_allocated = Min(lowest_ever_allocated, address);
  highest_ever_allocated =
      Max(highest_ever_allocated,
          reinterpret_cast<void*>(reinterpret_cast<char*>(address) + size));
}


bool OS::IsOutsideAllocatedSpace(void* address) {
  return address < lowest_ever_allocated || address >= highest_ever_allocated;
}


size_t OS::AllocateAlignment() {
  return getpagesize();
}


// Constants used for mmap.
// kMmapFd is used to pass vm_alloc flags to tag the region with the user
// defined tag 255 This helps identify V8-allocated regions in memory analysis
// tools like vmmap(1).
static const int kMmapFd = VM_MAKE_TAG(255);
static const off_t kMmapFdOffset = 0;


void* OS::Allocate(const size_t requested,
                   size_t* allocated,
                   bool is_executable) {
  const size_t msize = RoundUp(requested, getpagesize());
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  void* mbase = mmap(NULL, msize, prot,
                     MAP_PRIVATE | MAP_ANON,
                     kMmapFd, kMmapFdOffset);
  if (mbase == MAP_FAILED) {
    LOG(StringEvent("OS::Allocate", "mmap failed"));
    return NULL;
  }
  *allocated = msize;
  UpdateAllocatedSpaceLimits(mbase, msize);
  return mbase;
}


void OS::Free(void* address, const size_t size) {
  // TODO(1240712): munmap has a return value which is ignored here.
  int result = munmap(address, size);
  USE(result);
  ASSERT(result == 0);
}


#ifdef ENABLE_HEAP_PROTECTION

void OS::Protect(void* address, size_t size) {
  UNIMPLEMENTED();
}


void OS::Unprotect(void* address, size_t size, bool is_executable) {
  UNIMPLEMENTED();
}

#endif


void OS::Sleep(int milliseconds) {
  usleep(1000 * milliseconds);
}


void OS::Abort() {
  // Redirect to std abort to signal abnormal program termination
  abort();
}


void OS::DebugBreak() {
  asm("int $3");
}


class PosixMemoryMappedFile : public OS::MemoryMappedFile {
 public:
  PosixMemoryMappedFile(FILE* file, void* memory, int size)
    : file_(file), memory_(memory), size_(size) { }
  virtual ~PosixMemoryMappedFile();
  virtual void* memory() { return memory_; }
 private:
  FILE* file_;
  void* memory_;
  int size_;
};


OS::MemoryMappedFile* OS::MemoryMappedFile::create(const char* name, int size,
    void* initial) {
  FILE* file = fopen(name, "w+");
  if (file == NULL) return NULL;
  fwrite(initial, size, 1, file);
  void* memory =
      mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(file), 0);
  return new PosixMemoryMappedFile(file, memory, size);
}


PosixMemoryMappedFile::~PosixMemoryMappedFile() {
  if (memory_) munmap(memory_, size_);
  fclose(file_);
}


void OS::LogSharedLibraryAddresses() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  unsigned int images_count = _dyld_image_count();
  for (unsigned int i = 0; i < images_count; ++i) {
    const mach_header* header = _dyld_get_image_header(i);
    if (header == NULL) continue;
#if V8_HOST_ARCH_X64
    uint64_t size;
    char* code_ptr = getsectdatafromheader_64(
        reinterpret_cast<const mach_header_64*>(header),
        SEG_TEXT,
        SECT_TEXT,
        &size);
#else
    unsigned int size;
    char* code_ptr = getsectdatafromheader(header, SEG_TEXT, SECT_TEXT, &size);
#endif
    if (code_ptr == NULL) continue;
    const uintptr_t slide = _dyld_get_image_vmaddr_slide(i);
    const uintptr_t start = reinterpret_cast<uintptr_t>(code_ptr) + slide;
    LOG(SharedLibraryEvent(_dyld_get_image_name(i), start, start + size));
  }
#endif  // ENABLE_LOGGING_AND_PROFILING
}


uint64_t OS::CpuFeaturesImpliedByPlatform() {
  // MacOSX requires all these to install so we can assume they are present.
  // These constants are defined by the CPUid instructions.
  const uint64_t one = 1;
  return (one << SSE2) | (one << CMOV) | (one << RDTSC) | (one << CPUID);
}


int OS::ActivationFrameAlignment() {
  // OS X activation frames must be 16 byte-aligned; see "Mac OS X ABI
  // Function Call Guide".
  return 16;
}


void OS::ReleaseStore(volatile AtomicWord* ptr, AtomicWord value) {
  OSMemoryBarrier();
  *ptr = value;
}


const char* OS::LocalTimezone(double time) {
  if (isnan(time)) return "";
  time_t tv = static_cast<time_t>(floor(time/msPerSecond));
  struct tm* t = localtime(&tv);
  if (NULL == t) return "";
  return t->tm_zone;
}


double OS::LocalTimeOffset() {
  time_t tv = time(NULL);
  struct tm* t = localtime(&tv);
  // tm_gmtoff includes any daylight savings offset, so subtract it.
  return static_cast<double>(t->tm_gmtoff * msPerSecond -
                             (t->tm_isdst > 0 ? 3600 * msPerSecond : 0));
}


int OS::StackWalk(Vector<StackFrame> frames) {
  // If weak link to execinfo lib has failed, ie because we are on 10.4, abort.
  if (backtrace == NULL)
    return 0;

  int frames_size = frames.length();
  ScopedVector<void*> addresses(frames_size);

  int frames_count = backtrace(addresses.start(), frames_size);

  char** symbols = backtrace_symbols(addresses.start(), frames_count);
  if (symbols == NULL) {
    return kStackWalkError;
  }

  for (int i = 0; i < frames_count; i++) {
    frames[i].address = addresses[i];
    // Format a text representation of the frame based on the information
    // available.
    SNPrintF(MutableCStrVector(frames[i].text,
                               kStackWalkMaxTextLen),
             "%s",
             symbols[i]);
    // Make sure line termination is in place.
    frames[i].text[kStackWalkMaxTextLen - 1] = '\0';
  }

  free(symbols);

  return frames_count;
}




VirtualMemory::VirtualMemory(size_t size) {
  address_ = mmap(NULL, size, PROT_NONE,
                  MAP_PRIVATE | MAP_ANON | MAP_NORESERVE,
                  kMmapFd, kMmapFdOffset);
  size_ = size;
}


VirtualMemory::~VirtualMemory() {
  if (IsReserved()) {
    if (0 == munmap(address(), size())) address_ = MAP_FAILED;
  }
}


bool VirtualMemory::IsReserved() {
  return address_ != MAP_FAILED;
}


bool VirtualMemory::Commit(void* address, size_t size, bool is_executable) {
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  if (MAP_FAILED == mmap(address, size, prot,
                         MAP_PRIVATE | MAP_ANON | MAP_FIXED,
                         kMmapFd, kMmapFdOffset)) {
    return false;
  }

  UpdateAllocatedSpaceLimits(address, size);
  return true;
}


bool VirtualMemory::Uncommit(void* address, size_t size) {
  return mmap(address, size, PROT_NONE,
              MAP_PRIVATE | MAP_ANON | MAP_NORESERVE | MAP_FIXED,
              kMmapFd, kMmapFdOffset) != MAP_FAILED;
}


class ThreadHandle::PlatformData : public Malloced {
 public:
  explicit PlatformData(ThreadHandle::Kind kind) {
    Initialize(kind);
  }

  ~PlatformData() {
    if (stack_) {
        free(stack_);
    }
  }

  void Initialize(ThreadHandle::Kind kind) {
    ThreadHandle::PlatformData *current_pd = NULL;

    id_ = -1;
    memset(&jmp_, 0, sizeof(jmp_));
    stack_ = NULL;
    tramp_done_ = false;
    valid_ = false;
    memset(&locals_, 0, sizeof(locals_));

    switch (kind) {
      case ThreadHandle::SELF:
        current_pd = current_thread->thread_handle_data();

        ASSERT(next_id > 0);
        ASSERT(current_pd->id_ >= 0);

        id_ = current_pd->id_;
        memcpy(&jmp_, current_pd->jmp_, sizeof(jmp_));
        stack_ = current_pd->stack_;
        tramp_done_ = current_pd->tramp_done_;
        valid_ = current_pd->valid_;
        memcpy(&locals_, &current_pd->locals_, sizeof(locals_));
        break;

      case ThreadHandle::INVALID:
        id_ = next_id++;
        stack_ = malloc(kStackSize);
        break;

      default:
        UNREACHABLE();
    }
  }

  static int AllocateLocalId() {
    ASSERT(next_local < kMaxThreadLocals);
    return next_local++;
  }

  static const int kMaxThreadLocals = 16;
  static const size_t kStackSize = SIGSTKSZ;

  int id_;
  jmp_buf jmp_;
  void *stack_;
  bool tramp_done_;
  bool valid_;
  void *locals_[kMaxThreadLocals];

  private:
    static int next_id;
    static int next_local;
};


int ThreadHandle::PlatformData::next_id = 0;
int ThreadHandle::PlatformData::next_local = 0;


ThreadHandle::ThreadHandle(Kind kind) {
  data_ = new PlatformData(kind);
}


void ThreadHandle::Initialize(ThreadHandle::Kind kind) {
  data_->Initialize(kind);
}


ThreadHandle::~ThreadHandle() {
  delete data_;
}


bool ThreadHandle::IsSelf() const {
  return (data_->id_ == current_thread->thread_handle_data()->id_);
}


bool ThreadHandle::IsValid() const {
  return data_->valid_;
}


static void
Trampoline(int sig) {
  ThreadHandle::PlatformData *pd = current_thread->thread_handle_data();

  if (setjmp(pd->jmp_) == 0) {
    ASSERT(!pd->tramp_done_);
    ASSERT(!pd->valid_);

    pd->tramp_done_ = true;
  } else {
    ASSERT(pd->valid_);

    current_thread->Run();

    // XXX: Returning from here will cause an attempt to traverse up the stack
    //      to a non-existant stack frame at address 0x0. Add scheduler hook
    //      in Run() implementation.
    UNREACHABLE();
  }
}


Thread::Thread() : ThreadHandle(ThreadHandle::INVALID) {
  ThreadHandle::PlatformData *pd = thread_handle_data();
  Thread *prev_thread = prev_thread;
  struct sigaction sa;
  stack_t stk;

  ASSERT(!pd->tramp_done_);

  sa.sa_handler = Trampoline;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_ONSTACK;

  if (sigaction(SIGUSR2, &sa, NULL)) {
    perror("sigaction");
    exit(1);
  }

  stk.ss_sp = pd->stack_;
  stk.ss_size = ThreadHandle::PlatformData::kStackSize;
  stk.ss_flags = 0;

  if (sigaltstack(&stk, NULL)) {
    perror("sigaltstack");
    exit(1);
  }

  prev_thread = current_thread;
  current_thread = this;

  kill(getpid(), SIGUSR2);
  while (!pd->tramp_done_)
    ;

  current_thread = prev_thread;
}


Thread::~Thread() {
}


void Thread::Start() {
#ifdef DEBUG
  Thread *prev_thread = current_thread;
#endif

  ThreadHandle::PlatformData *current_pd = current_thread->thread_handle_data();
  ThreadHandle::PlatformData *pd = thread_handle_data();

  ASSERT(pd->tramp_done_);
  ASSERT(current_thread != this);

  pd->valid_ = true;

  if (setjmp(current_pd->jmp_) == 0) {
    current_thread = this;
    longjmp(pd->jmp_, 1);
  } else {
    // When setjmp() returns, current_thread should be whatever
    // thread invoked this function
    ASSERT(current_thread != this);
    ASSERT(current_thread == prev_thread);
  }
}


void Thread::Join() {
  UNIMPLEMENTED();
}


Thread::LocalStorageKey Thread::CreateThreadLocalKey() {
  int k = ThreadHandle::PlatformData::AllocateLocalId();

  PGLOG(("CreateThreadLocalKey(%d) = %d\n", pd->id_, k));

  return static_cast<LocalStorageKey>(k);
}


void Thread::DeleteThreadLocalKey(LocalStorageKey key) {
  UNIMPLEMENTED();
}


void* Thread::GetThreadLocal(LocalStorageKey key) {
  ThreadHandle::PlatformData *pd = current_thread->thread_handle_data();
  int k = static_cast<int>(key);

  ASSERT(k >= 0);
  ASSERT(k < ThreadHandle::PlatformData::kMaxThreadLocals);

  PGLOG(("GetThreadLocal(%d, %d) = %p\n", pd->id_, k, pd->locals_[k]));

  return pd->locals_[k];
}


void Thread::SetThreadLocal(LocalStorageKey key, void* value) {
  ThreadHandle::PlatformData *pd = current_thread->thread_handle_data();
  int k = static_cast<int>(key);

  ASSERT(k >= 0);
  ASSERT(k < ThreadHandle::PlatformData::kMaxThreadLocals);

  PGLOG(("SetThreadLocal(%d, %d, %p)\n", pd->id_, k, value));
  pd->locals_[k] = value;
}


void Thread::YieldCPU() {
  UNIMPLEMENTED();
}


class MacOSMutex : public Mutex {
 public:

  MacOSMutex() { PGLOG(("MacOSMutex::MacOSMutex()\n")); }

  ~MacOSMutex() { PGLOG(("MacOSMutex::~MacOSMutex()\n")); }

  int Lock() { PGLOG(("MacOSMutex::Lock()\n")); return 0; }

  int Unlock() { PGLOG(("MacOSMutex::Unlock()\n")); return 0; }
};


Mutex* OS::CreateMutex() {
  PGLOG(("OS::CreateMutex()\n"));

  return new MacOSMutex();
}


class MacOSSemaphore : public Semaphore {
 public:
  explicit MacOSSemaphore(int count) {
    PGLOG(("MacOSSemaphore::MacOSSemaphore(%d)\n", count));
  }

  ~MacOSSemaphore() { PGLOG(("MacOSSemaphore::~MacOSSemaphore()\n")); }

  void Wait() { PGLOG(("MacOSSemaphore::Wait()\n")); }

  bool Wait(int timeout);

  void Signal() { PGLOG(("MacOSSemaphore::Signal()\n")); }

 private:
  semaphore_t semaphore_;
};


bool MacOSSemaphore::Wait(int timeout) {
  PGLOG(("MacOSSemaphore::Wait(%d)\n", timeout));
  return true;
}


Semaphore* OS::CreateSemaphore(int count) {
  PGLOG(("OS::CreateSemaphore(%d)\n", count));
  return new MacOSSemaphore(count);
}


#ifdef ENABLE_LOGGING_AND_PROFILING

class Sampler::PlatformData : public Malloced {
 public:
  explicit PlatformData(Sampler* sampler)
      : sampler_(sampler),
        task_self_(mach_task_self()),
        profiled_thread_(0),
        sampler_thread_(0) {
  }

  Sampler* sampler_;
  // Note: for profiled_thread_ Mach primitives are used instead of PThread's
  // because the latter doesn't provide thread manipulation primitives required.
  // For details, consult "Mac OS X Internals" book, Section 7.3.
  mach_port_t task_self_;
  thread_act_t profiled_thread_;
  pthread_t sampler_thread_;

  // Sampler thread handler.
  void Runner() {
    // Loop until the sampler is disengaged, keeping the specified samling freq.
    for ( ; sampler_->IsActive(); OS::Sleep(sampler_->interval_)) {
      TickSample sample_obj;
      TickSample* sample = CpuProfiler::TickSampleEvent();
      if (sample == NULL) sample = &sample_obj;

      // We always sample the VM state.
      sample->state = VMState::current_state();
      // If profiling, we record the pc and sp of the profiled thread.
      if (sampler_->IsProfiling()
          && KERN_SUCCESS == thread_suspend(profiled_thread_)) {
#if V8_HOST_ARCH_X64
        thread_state_flavor_t flavor = x86_THREAD_STATE64;
        x86_thread_state64_t state;
        mach_msg_type_number_t count = x86_THREAD_STATE64_COUNT;
#if __DARWIN_UNIX03
#define REGISTER_FIELD(name) __r ## name
#else
#define REGISTER_FIELD(name) r ## name
#endif  // __DARWIN_UNIX03
#elif V8_HOST_ARCH_IA32
        thread_state_flavor_t flavor = i386_THREAD_STATE;
        i386_thread_state_t state;
        mach_msg_type_number_t count = i386_THREAD_STATE_COUNT;
#if __DARWIN_UNIX03
#define REGISTER_FIELD(name) __e ## name
#else
#define REGISTER_FIELD(name) e ## name
#endif  // __DARWIN_UNIX03
#else
#error Unsupported Mac OS X host architecture.
#endif  // V8_HOST_ARCH

        if (thread_get_state(profiled_thread_,
                             flavor,
                             reinterpret_cast<natural_t*>(&state),
                             &count) == KERN_SUCCESS) {
          sample->pc = reinterpret_cast<Address>(state.REGISTER_FIELD(ip));
          sample->sp = reinterpret_cast<Address>(state.REGISTER_FIELD(sp));
          sample->fp = reinterpret_cast<Address>(state.REGISTER_FIELD(bp));
          sampler_->SampleStack(sample);
        }
        thread_resume(profiled_thread_);
      }

      // Invoke tick handler with program counter and stack pointer.
      sampler_->Tick(sample);
    }
  }
};

#undef REGISTER_FIELD


// Entry point for sampler thread.
static void* SamplerEntry(void* arg) {
  Sampler::PlatformData* data =
      reinterpret_cast<Sampler::PlatformData*>(arg);
  data->Runner();
  return 0;
}


Sampler::Sampler(int interval, bool profiling)
    : interval_(interval), profiling_(profiling), active_(false) {
  data_ = new PlatformData(this);
}


Sampler::~Sampler() {
  delete data_;
}


void Sampler::Start() {
  // If we are profiling, we need to be able to access the calling
  // thread.
  if (IsProfiling()) {
    data_->profiled_thread_ = mach_thread_self();
  }

  // Create sampler thread with high priority.
  // According to POSIX spec, when SCHED_FIFO policy is used, a thread
  // runs until it exits or blocks.
  pthread_attr_t sched_attr;
  sched_param fifo_param;
  pthread_attr_init(&sched_attr);
  pthread_attr_setinheritsched(&sched_attr, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&sched_attr, SCHED_FIFO);
  fifo_param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  pthread_attr_setschedparam(&sched_attr, &fifo_param);

  active_ = true;
  pthread_create(&data_->sampler_thread_, &sched_attr, SamplerEntry, data_);
}


void Sampler::Stop() {
  // Seting active to false triggers termination of the sampler
  // thread.
  active_ = false;

  // Wait for sampler thread to terminate.
  pthread_join(data_->sampler_thread_, NULL);

  // Deallocate Mach port for thread.
  if (IsProfiling()) {
    mach_port_deallocate(data_->task_self_, data_->profiled_thread_);
  }
}

#endif  // ENABLE_LOGGING_AND_PROFILING

} }  // namespace v8::internal
// vim:ts=2 sw=2
