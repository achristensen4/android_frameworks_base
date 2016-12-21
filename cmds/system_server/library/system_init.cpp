/*
 * System server main initialization.
 *
 * The system server is responsible for becoming the Binder
 * context manager, supplying the root ServiceManager object
 * through which other services can be found.
 */

#define LOG_TAG "sysproc"

#include <utils/IPCThreadState.h>
#include <utils/ProcessState.h>
#include <utils/IServiceManager.h>
#include <utils/TextOutput.h>
#include <utils/Log.h>

#include <SurfaceFlinger.h>
#include <AudioFlinger.h>
#include <CameraService.h>
#include <MediaPlayerService.h>

#include <android_runtime/AndroidRuntime.h>

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <cutils/properties.h>

using namespace android;

namespace android {
/**
 * This class is used to kill this process when the runtime dies.
 */
class GrimReaper : public IBinder::DeathRecipient {
public: 
    GrimReaper() { }

    virtual void binderDied(const wp<IBinder>& who)
    {
        kill(getpid(), SIGKILL);
    }
};

} // namespace android



extern "C" status_t system_init()
{
    sp<ProcessState> proc(ProcessState::self());
    
    sp<IServiceManager> sm = defaultServiceManager();
    
    sp<GrimReaper> grim = new GrimReaper();
    sm->asBinder()->linkToDeath(grim, grim.get(), 0);
    
    char propBuf[PROPERTY_VALUE_MAX];
    property_get("system_init.startsurfaceflinger", propBuf, "1");
    if (strcmp(propBuf, "1") == 0) {
        // Start the SurfaceFlinger
        SurfaceFlinger::instantiate();
    }

    // On the simulator, audioflinger et al don't get started the
    // same way as on the device, and we need to start them here
    if (!proc->supportsProcesses()) {

        // Start the AudioFlinger
        AudioFlinger::instantiate();

        // Start the media playback service
        MediaPlayerService::instantiate();

        // Start the camera service
        CameraService::instantiate();
    }

    // And now start the Android runtime.  We have to do this bit
    // of nastiness because the Android runtime initialization requires
    // some of the core system services to already be started.
    // All other servers should just start the Android runtime at
    // the beginning of their processes's main(), before calling
    // the init function.
    
    AndroidRuntime* runtime = AndroidRuntime::getRuntime();

    runtime->callStatic("com/android/server/SystemServer", "init2");
        
    // If running in our own process, just go into the thread
    // pool.  Otherwise, call the initialization finished
    // func to let this process continue its initilization.
    if (proc->supportsProcesses()) {
        ProcessState::self()->startThreadPool();
        IPCThreadState::self()->joinThreadPool();
    }
    return NO_ERROR;
}

