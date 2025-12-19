#ifndef UVCDriver_H
#define UVCDriver_H

#include <Arduino.h>
#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/video.h>
// #include <zephyr/logging/log.h>
#include <zephyr/usb/class/usbd_uvc.h>
#include <../../../zephyr/samples/subsys/usb/common/sample_usbd.h>

class UVCDriver {
public:
    UVCDriver();
    ~UVCDriver();

    int begin(); // Initialize the UVC driver
    void startVideoStream(); // Start streaming video
    void stopVideoStream();  // Stop video streaming
    void poll();  // Poll the buffers
    bool isVideoStreaming(); // Check if streaming is active

private:
    const struct device *uvc_dev;
    const struct device *video_dev;
    struct usbd_context *sample_usbd;
    struct video_format fmt;
    struct video_caps caps;
    struct k_poll_signal sig;
    struct k_poll_event evt[1];
    k_timeout_t timeout;
    bool streaming;
    void initVideoBuffers(); // Initialize the video buffers
    void handlePolling(); // Method to handle polling
};

#endif