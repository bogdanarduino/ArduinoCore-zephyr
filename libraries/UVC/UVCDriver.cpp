#include "UVCDriver.h"


UVCDriver::UVCDriver() {
    uvc_dev = DEVICE_DT_GET(DT_NODELABEL(uvc));
    if (!device_is_ready(uvc_dev)) {
        printk("UVC device not ready");
        return;
    }
	
    video_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_camera));
    if (!device_is_ready(video_dev)) {
        printk("DCMI camera on board not ready");
        return;
    }
    printk("Init OK");
    sample_usbd = nullptr;
    streaming = false;
    timeout = K_FOREVER;
}

UVCDriver::~UVCDriver() {
    if (sample_usbd) {
        usbd_disable(sample_usbd);
    }
}

int UVCDriver::begin() {
    if (!device_is_ready(video_dev)) {
        printk("Video device not ready");
        return -ENODEV;
    }

    caps.type = VIDEO_BUF_TYPE_OUTPUT;

    if (video_get_caps(video_dev, &caps)) {
        printk("Could not retrieve video capabilities");
        return -EINVAL;
    }

    // Do before initializing USB
    uvc_set_video_dev(uvc_dev, video_dev);

    sample_usbd = sample_usbd_init_device(NULL);
    if (sample_usbd == NULL) {
        return -ENODEV;
    }

    int ret = usbd_enable(sample_usbd);
    if (ret != 0) {
        return ret;
    }

    // Get the video format once it is selected by the host
    while (true) {
        fmt.type = VIDEO_BUF_TYPE_INPUT;
        ret = video_get_format(uvc_dev, &fmt);
        if (ret == 0) {
            break;
        }
        if (ret != -EAGAIN) {
            printk("Failed to get the video format");
            return ret;
        }
        K_MSEC(10000); // maybe change this if found to not be working properly
    }

    // printk("The host selected format '%s' %ux%u", VIDEO_FOURCC_TO_STR(fmt.pixelformat), fmt.width, fmt.height);

    initVideoBuffers();

    // Set up polling
    k_poll_signal_init(&sig);
    k_poll_event_init(&evt[0], K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &sig);

    ret = video_set_signal(video_dev, &sig);
    if (ret != 0) {
        printk("Failed to setup the signal on %s output endpoint", video_dev->name);
        timeout = K_MSEC(1);
    }

    ret = video_set_signal(uvc_dev, &sig);
    if (ret != 0) {
        printk("Failed to setup the signal on %s input endpoint", uvc_dev->name);
        return ret;
    }

    return 0;
}

void UVCDriver::startVideoStream() {
    int ret = video_stream_start(video_dev, VIDEO_BUF_TYPE_OUTPUT);
    if (ret != 0) {
        printk("Failed to start %s", video_dev->name);
    } else {
        streaming = true;
        printk("Video streaming started.");
    }
}

void UVCDriver::stopVideoStream() {
    // Stop video streaming here
    streaming = false;
    printk("Video streaming stopped.");
}

bool UVCDriver::isVideoStreaming() {
    return streaming;
}

void UVCDriver::initVideoBuffers() {
    size_t bsize;
    if (caps.min_line_count == LINE_COUNT_HEIGHT) {
        bsize = fmt.pitch * fmt.height;
    } else {
        bsize = fmt.pitch * caps.min_line_count;
    }

    for (int i = 0; i < CONFIG_VIDEO_BUFFER_POOL_NUM_MAX; i++) {
        struct video_buffer *vbuf = video_buffer_alloc(bsize, K_NO_WAIT);
        if (vbuf == NULL) {
            printk("Could not allocate the video buffer");
        } else {
            vbuf->type = VIDEO_BUF_TYPE_OUTPUT;
            int ret = video_enqueue(video_dev, vbuf);
            if (ret != 0) {
                printk("Could not enqueue video buffer");
            }
        }
    }
}

void UVCDriver::poll() {
    int ret = k_poll(evt, ARRAY_SIZE(evt), timeout);
    if (ret != 0 && ret != -EAGAIN) {
        printk("Poll exited with status %d", ret);
    }

    handlePolling();
}

void UVCDriver::handlePolling() {
    int ret;
    struct video_buffer *vbuf;

    // Output buffer
    struct video_buffer vbuf_temp_output = {.type = VIDEO_BUF_TYPE_OUTPUT};
    vbuf = &vbuf_temp_output;

    // Dequeue from video device
    if (video_dequeue(video_dev, &vbuf, K_NO_WAIT) == 0) {
        // Process dequeued buffer
        vbuf->type = VIDEO_BUF_TYPE_INPUT;
        ret = video_enqueue(uvc_dev, vbuf);
        if (ret != 0) {
            printk("Could not enqueue video buffer to %s", uvc_dev->name);
        }
    }

    // Input buffer (previous error was here)
    struct video_buffer vbuf_temp_input = {.type = VIDEO_BUF_TYPE_INPUT};
    vbuf = &vbuf_temp_input;

    // Dequeue from UVC device
    if (video_dequeue(uvc_dev, &vbuf, K_NO_WAIT) == 0) {
        // Process dequeued buffer
        vbuf->type = VIDEO_BUF_TYPE_OUTPUT;
        ret = video_enqueue(video_dev, vbuf);
        if (ret != 0) {
            printk("Could not enqueue video buffer to %s", video_dev->name);
        }
    }

    k_poll_signal_reset(&sig);
}
