#include "UVCDriver.h"

UVCDriver uvc;

void setup() {
    if (uvc.begin() == 0) {
        printk("UVC Initialized successfully!");
        uvc.startVideoStream();
    } else {
        printk("Failed to initialize UVC.");
    }
}

void loop() {
    if (uvc.isVideoStreaming()) {
        uvc.poll();  // Poll video buffers
    }
}
