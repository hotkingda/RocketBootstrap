# RocketBootstrap

A support library for jailbroken iOS that allows tweaks to securely communicate with sandboxed processes via system-wide Mach services.

Fully tested on **iOS 13.0 – iOS 16.5** (Dopamine rootless jailbreak, iPhone 13).

Based on the original work by [@rpetrich](https://github.com/rpetrich/RocketBootstrap). This fork adds iOS 16 compatibility and rootless jailbreak support.

## What It Does

iOS sandboxing prevents apps from accessing arbitrary Mach services. RocketBootstrap works around this by:

1. Running a daemon (`rocketd`) that maintains a whitelist of allowed service names
2. Injecting into `MobileGestaltHelper` and `SpringBoard` to intercept bootstrap lookups
3. Using `cy:rbs:` name redirection (iOS 15.6+) or the `com.apple.mobilegestalt.xpc` channel (older iOS) to broker connections

This enables tweak developers to register IPC services in privileged processes (like SpringBoard) and have sandboxed apps connect to them.

## API Overview

### Mach Port (Low-Level)

```objc
#import "rocketbootstrap.h"

// Server side (privileged process only)
rocketbootstrap_unlock("com.example.myservice");
rocketbootstrap_register(bootstrap_port, "com.example.myservice", service_port);

// Client side
rocketbootstrap_look_up(bootstrap_port, "com.example.myservice", &service_port);
```

### CPDistributedMessagingCenter (Recommended)

The most common usage pattern, as seen in projects like XXTouchNG:

**Server** (runs in SpringBoard):
```objc
#import "rocketbootstrap.h"

rocketbootstrap_unlock("com.example.myservice");

CPDistributedMessagingCenter *center = [CPDistributedMessagingCenter centerNamed:@"com.example.myservice"];
rocketbootstrap_distributedmessagingcenter_apply(center);

[center registerForMessageName:@"myMessage" target:self selector:@selector(handleMessage:userInfo:)];
[center runServerOnCurrentThread];
```

**Client** (runs in any process):
```objc
#import "rocketbootstrap.h"

CPDistributedMessagingCenter *center = [CPDistributedMessagingCenter centerNamed:@"com.example.myservice"];
rocketbootstrap_distributedmessagingcenter_apply(center);

NSDictionary *reply = [center sendMessageAndReceiveReplyName:@"myMessage"
                                                    userInfo:@{@"key": @"value"}
                                                       error:nil];
```

### CFMessagePort

```objc
// Server side
rocketbootstrap_cfmessageportexposelocal(localPort);

// Client side
CFMessagePortRef remote = rocketbootstrap_cfmessageportcreateremote(kCFAllocatorDefault, CFSTR("com.example.myservice"));
```

### XPC

```objc
// Server side (privileged process)
xpc_connection_t listener = rocketbootstrap_xpc_connection_create("com.example.myservice", queue, XPC_CONNECTION_MACH_SERVICE_LISTENER);

// Client side
xpc_connection_t client = rocketbootstrap_xpc_connection_create("com.example.myservice", NULL, 0);
```

## Dynamic Loading

For projects that don't link against `librocketbootstrap.dylib` directly, define `ROCKETBOOTSTRAP_LOAD_DYNAMIC` before including the header. This uses `dlopen`/`dlsym` at runtime:

```objc
#define ROCKETBOOTSTRAP_LOAD_DYNAMIC 1
#import "rocketbootstrap.h"
```

## Building

Requires [Theos](https://theos.dev).

```bash
# Classic layout
make package

# Rootless (Dopamine, etc.)
make package THEOS_PACKAGE_SCHEME=rootless
```

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│ Sandboxed App                                           │
│  CPDistributedMessagingCenter + rbs_apply ──────┐       │
└─────────────────────────────────────────────────┼───────┘
                                                  │ Mach IPC
┌─────────────────────────────────────────────────┼───────┐
│ SpringBoard (librocketbootstrap.dylib injected) │       │
│  rocketbootstrap_unlock ──► rocketd ◄───────────┘       │
│  CPDistributedMessagingCenter server                    │
└─────────────────────────────────────────────────────────┘
                        │
┌───────────────────────┼─────────────────────────────────┐
│ MobileGestaltHelper   │  (iOS < 16)                     │
│  Intercepts mach_msg, handles ROCKETBOOTSTRAP_LOOKUP_ID │
└─────────────────────────────────────────────────────────┘
```

- **iOS 15.6+**: Uses `cy:rbs:` name redirection via `bootstrap_register`/`bootstrap_look_up`
- **iOS < 15.6**: Routes lookups through `com.apple.mobilegestalt.xpc` with a custom message ID
- **iOS 16+**: Skips `_xpc_connection_mach_event` hardcoded offset hook; uses name redirection exclusively

## Changes from Upstream

- iOS 16 support: handle guarded mach ports by using name redirection only
- Rootless jailbreak support: detect and use `/var/jb` paths for `MobileGestaltHelper`, `SpringBoard`, and `rocketd_reenable`
- Updated `TweakInject.tbd` to tapi-tbd v4 format
- Firmware dependency extended to `< 17.0`

## Credits

- [Ryan Petrich](https://github.com/rpetrich) — original author
- [CoolStar](https://github.com/coolstar) — Electra/Chimera contributions
- [DHowett](https://github.com/DHowett) — Theos maintainer
