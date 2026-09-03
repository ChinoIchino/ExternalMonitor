#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libudev.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>

volatile __sig_atomic_t keepRunning = 1;
int currentState = -1;
char* INTERNAL_MOUSE_PATH;

void handleSignal(int signal){
    keepRunning = 0;
}

char* findInternalMouse(struct udev* udev){
    struct udev_list_entry *devices, *devListEntry;
    struct udev_device *dev;
    struct udev_enumerate *enumerate;
    int found_external = 0;

    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_add_match_property(enumerate, "ID_INPUT_MOUSE", "1");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(devListEntry, devices){
        const char* path = udev_list_entry_get_name(devListEntry);
        dev = udev_device_new_from_syspath(udev, path);
        const char* syspath = udev_device_get_syspath(dev);
        if(syspath){
            if(strstr(syspath, "platform")){
                char* result = malloc(strlen(syspath) + strlen("/inhibited") + 1);

                size_t sizeToCopy = strlen(syspath) - 7;
                strncpy(result, syspath, sizeToCopy);
                result[sizeToCopy] = '\0';
                strcat(result, "input16/inhibited");

                udev_enumerate_unref(enumerate);
                udev_device_unref(dev);

                return result;
            }
        }
        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);
    return NULL;
}

int verifyExternalMouseIsConnected(struct udev* udev){
    struct udev_list_entry *devices, *devListEntry;
    struct udev_device *dev;
    struct udev_enumerate *enumerate;
    int found_external = 0;

    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_add_match_property(enumerate, "ID_INPUT_MOUSE", "1");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(devListEntry, devices){
        const char* path = udev_list_entry_get_name(devListEntry);
        dev = udev_device_new_from_syspath(udev, path);
        const char* syspath = udev_device_get_syspath(dev);
        if(syspath){
            // Found a external mouse via the 3-2 bus
            if(strstr(syspath, "3-2")){
                udev_device_unref(dev);
                udev_enumerate_unref(enumerate);
                return currentState = 1;
            }
        }
        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);
    return currentState = 0;
}

void disableInternalTouchPad(int disableInternal){
    FILE* file = 0;
    if ((file = fopen(INTERNAL_MOUSE_PATH, "w")) == NULL) {
        printf(
            "mouseMonitor.service ERROR: Failed to open %s: %s (errno=%d)\n",
            INTERNAL_MOUSE_PATH,
            strerror(errno),
            errno
        );
        return;
    }


    ssize_t written = fprintf(file, "%c", disableInternal + '0');
    if (written < 0) {
        printf("mouseMonitor.service ERROR: Kernel rejected file rewrite");
        fclose(file);
        return;
    }

    fclose(file);
}

int main(){
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    struct udev* udev = udev_new();
    if(!udev){
        printf("\n\nmouseMonitor.service ERROR: udev wasn't able to be created. Stopping the process");
        return 1;
    }

    INTERNAL_MOUSE_PATH = findInternalMouse(udev);

    currentState = verifyExternalMouseIsConnected(udev);
    int localState = currentState;

    disableInternalTouchPad(currentState);
    while(keepRunning){
        currentState = verifyExternalMouseIsConnected(udev);
        if(currentState != localState){
            disableInternalTouchPad(currentState);
            localState = currentState;
        }
        usleep(3000000);
    }

    free(INTERNAL_MOUSE_PATH);
    disableInternalTouchPad(0);
    udev_unref(udev);
    return 0;
}