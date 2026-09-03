#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libudev.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>

volatile sig_atomic_t isRunning = 1;
int currentState = -1;
char* INTERNAL_KEYBOARD_INTERFACE; 

void handleSignal(int signal){
    isRunning = 0;
}

//DEPRECATED minor leak
void printAllKeyboards(struct udev* udev){
    struct udev_list_entry *devices, *devListEntry;
    struct udev_device *dev;
    struct udev_enumerate *enumerate;
    int found_external = 0;

    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_add_match_property(enumerate, "ID_INPUT_KEYBOARD", "1");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    printf("\n\nGot the list:\n");
    udev_list_entry_foreach(devListEntry, devices){
        const char* path = udev_list_entry_get_name(devListEntry);
        dev = udev_device_new_from_syspath(udev, path);
        const char* syspath = udev_device_get_syspath(dev);
        if(syspath){
            printf("%s\n", syspath);
        }
    }
    
    udev_device_unref(dev);
    udev_enumerate_unref(enumerate);
}
char* findInternalKeyboard(struct udev* udev){
    struct udev_list_entry *devices, *devListEntry;
    struct udev_device *dev;
    struct udev_enumerate *enumerate;
    int found_external = 0;

    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_add_match_property(enumerate, "ID_INPUT_KEYBOARD", "1");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(devListEntry, devices){
        const char* path = udev_list_entry_get_name(devListEntry);
        dev = udev_device_new_from_syspath(udev, path);
        const char* syspath = udev_device_get_syspath(dev);
        if(syspath){
            if(strstr(syspath, "serio0")){
                char* result = malloc(strlen(syspath) + strlen("/inhibited") + 1);
                strcpy(result, syspath);

                udev_enumerate_unref(enumerate);
                udev_device_unref(dev);
                return result;
            }
            udev_device_unref(dev);
        }
    }

    udev_enumerate_unref(enumerate);
    return 0;
}
/**
 * Find if a external keyboard is connected.
 * @param udev : used to get the list of devices
 * @return 1 if a external keyboard is detected, else 0
 */
int verifyExternalKeyboardIsConnected(struct udev* udev){
    struct udev_list_entry *devices, *devListEntry;
    struct udev_device *dev;
    struct udev_enumerate *enumerate;
    int found_external = 0;

    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_add_match_property(enumerate, "ID_INPUT_KEYBOARD", "1");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(devListEntry, devices){
        const char* path = udev_list_entry_get_name(devListEntry);
        dev = udev_device_new_from_syspath(udev, path);
        const char* syspath = udev_device_get_syspath(dev);
        if(syspath){
            if(strstr(syspath, "3-1")){
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
void disableInternalKeyboard(int disableInternal){
    
    FILE* file = 0;
    if ((file = fopen(INTERNAL_KEYBOARD_INTERFACE, "w")) == NULL) {
        printf(
            "keyboardMonitor.service ERROR: Failed to open %s: %s (errno=%d)\n",
            INTERNAL_KEYBOARD_INTERFACE,
            strerror(errno),
            errno
            );
        return;
    }


    ssize_t written = fprintf(file, "%c", disableInternal + '0');
    if (written < 0) {
        printf("keyboardMonitor.service ERROR: Kernel rejected file rewrite");
        fclose(file);
        return;
    }

    fclose(file);
}


int main(){
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    struct udev *udev = udev_new();
    if(!udev){
        printf("\n\nkeyboardMonitor.service ERROR: udev wasn't able to be created. Stopping the process");
        return 1;
    }

    INTERNAL_KEYBOARD_INTERFACE = strcat(findInternalKeyboard(udev), "/inhibited");

    currentState = verifyExternalKeyboardIsConnected(udev);
    disableInternalKeyboard(currentState);
    int localState = currentState;
    
    while(isRunning){
        verifyExternalKeyboardIsConnected(udev);
        if(currentState != localState){
            disableInternalKeyboard(currentState);
            localState = currentState;
        }
        usleep(3000000);
    }

    free(INTERNAL_KEYBOARD_INTERFACE);
    disableInternalKeyboard(0);
    udev_unref(udev);

    return 0;
}