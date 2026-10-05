// C runtime glue that the SDK link flow leaves open (it links with -nostartfiles and
// --specs=nosys.specs, like the SDK example makefiles).
#include <errno.h>

// The compiler passes __dso_handle to __cxa_atexit when it registers the destructor of an
// object with static storage. The startup file crtbegin.o defines it, and -nostartfiles
// drops that file. The destructors never run: main() does not return.
void* __dso_handle = 0;

// abort() (std::terminate, pure virtual calls) reaches _getpid and _kill. The libnosys
// versions work the same way, but they make the linker print "is not implemented"
// warnings.
int _getpid(void) { return 1; }

int _kill(int process_id, int signal)
{
  (void)process_id;
  (void)signal;
  errno = EINVAL;
  return -1;
}
