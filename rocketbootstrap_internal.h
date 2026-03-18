#import <CoreFoundation/CoreFoundation.h>
#import <dlfcn.h>

#import "rocketbootstrap.h"

#define kRocketBootstrapUnlockService "com.rpetrich.rocketbootstrapd"

#define ROCKETBOOTSTRAP_LOOKUP_ID -1

typedef struct {
	mach_msg_header_t head;
	mach_msg_body_t body;
	uint32_t name_length;
	char name[];
} _rocketbootstrap_lookup_query_t;

typedef struct {
	mach_msg_header_t head;
	mach_msg_body_t body;
	mach_msg_port_descriptor_t response_port;
} _rocketbootstrap_lookup_response_t;

#import "LightMessaging/LightMessaging.h"

__attribute__((unused))
static LMConnection connection = {
	MACH_PORT_NULL,
	kRocketBootstrapUnlockService
};

__attribute__((unused))
static inline bool rocketbootstrap_is_passthrough(void)
{
	return kCFCoreFoundationVersionNumber < 800.0;
}

__attribute__((unused))
static inline bool rocketbootstrap_uses_name_redirection(void)
{
	// iOS 16+: mobilegestalt.xpc is guarded, name redirection is the only option
	if (kCFCoreFoundationVersionNumber >= 1900.0) {
		return true;
	}
	if (kCFCoreFoundationVersionNumber >= 1556.00) {
		static int state;
		int currentState = state;
		if (currentState == 0) {
			int check = sandbox_check(getpid(), "mach-lookup", SANDBOX_FILTER_LOCAL_NAME | SANDBOX_CHECK_NO_REPORT, "cy:rbs");
			if (check != 0) {
				// Dopamine 2.x bypasses mach-lookup sandbox checks in launchd,
				// but local sandbox_check() may still return denied.
				// Try actual bootstrap_look_up to verify.
				mach_port_t bootstrap = MACH_PORT_NULL;
				task_get_bootstrap_port(mach_task_self(), &bootstrap);
				mach_port_t testPort = MACH_PORT_NULL;
				kern_return_t err = bootstrap_look_up(bootstrap, "cy:rbs", &testPort);
				if (err == BOOTSTRAP_UNKNOWN_SERVICE) {
					check = 0;
				}
				if (testPort != MACH_PORT_NULL) {
					mach_port_deallocate(mach_task_self(), testPort);
				}
			}
			currentState = check == 0 ? 1 : 2;
			state = currentState;
		}
		return currentState - 1;
	}
	return false;
}

kern_return_t _rocketbootstrap_is_unlocked(const name_t service_name); // Errors if not in a privileged process such as SpringBoard or backboardd
