#include <x86linux/helper.h>

#undef getpid
pid_t cached_getpid() {
  static thread_local pid_t pid __attribute__((tls_model("initial-exec")));
  return likely(pid) ? pid : (pid = getpid());
}
#undef gettid
pid_t cached_gettid() {
  static thread_local pid_t tid __attribute__((tls_model("initial-exec")));
  return likely(tid) ? tid : (tid = gettid());
}
