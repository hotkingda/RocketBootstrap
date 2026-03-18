#import <unistd.h>
#import <dlfcn.h>
#import <stdio.h>
#import <sys/stat.h>

#ifdef __LP64__
#if __has_include(<ptrauth.h>)
#include <ptrauth.h>
#define sign_function(ptr) ({ \
	__typeof__(ptr) _ptr = ptr; \
	ptr ? ptrauth_sign_unauthenticated(_ptr, ptrauth_key_function_pointer, 0) : _ptr; \
})
#else
#define sign_function(ptr) ptr
#endif
#else
#define sign_function(ptr) ptr
#endif

#ifdef __arm64__
#import "libjailbreak_xpc.h"
static int fix_setuid(void)
{
	void *libjailbreak = dlopen("/usr/lib/libjailbreak.dylib", RTLD_LAZY);
	if (!libjailbreak)
		libjailbreak = dlopen("/var/jb/usr/lib/libjailbreak.dylib", RTLD_LAZY);
	if (libjailbreak) {
		// Try Dopamine 2.x API first
		int (*jbclient_process_checkin)(void *, void *, void *, void *, void *, void *) =
			sign_function(dlsym(libjailbreak, "jbclient_process_checkin"));
		if (jbclient_process_checkin) {
			jbclient_process_checkin(NULL, NULL, NULL, NULL, NULL, NULL);
			return 0;
		}
		// Fallback to Electra/unc0ver API
		jb_connection_t (*jb_connect)(void) = sign_function(dlsym(libjailbreak, "jb_connect"));
		int (*jb_fix_setuid_now)(jb_connection_t connection, pid_t pid) = sign_function(dlsym(libjailbreak, "jb_fix_setuid_now"));
		void (*jb_disconnect)(jb_connection_t connection) = sign_function(dlsym(libjailbreak, "jb_disconnect"));
		if (jb_connect && jb_fix_setuid_now && jb_disconnect) {
			jb_connection_t connection = jb_connect();
			if (connection) {
				int result = jb_fix_setuid_now(connection, getpid());
				jb_disconnect(connection);
				return result;
			}
		}
	}
	return 1;
}
#else
static int fix_setuid(void)
{
	// Does not apply to older iOS versions
	return 1;
}
#endif

int main(int argc, char *argv[])
{
	close(0);
	close(1);
	close(2);
	fix_setuid();
	setuid(0);
	setgid(0);
	seteuid(0);
	setegid(0);
	struct stat st;
	if (stat("/var/jb", &st) == 0)
		return execlp("launchctl", "launchctl", "load", "/var/jb/Library/LaunchDaemons/com.rpetrich.rocketbootstrapd.plist", NULL);
	return execlp("launchctl", "launchctl", "load", "/Library/LaunchDaemons/com.rpetrich.rocketbootstrapd.plist", NULL);
}
