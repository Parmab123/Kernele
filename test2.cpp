// test_circular_queue.cpp
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>

#define DEVICE_NAME "/dev/my_circular_queue"
#define IOCTL_SET_SIZE _IOW('q', 1, int)
#define IOCTL_PUSH_DATA _IOW('q', 2, struct queue_data)
#define IOCTL_POP_DATA _IOR('q', 3, struct queue_data)

struct queue_data {
    char *data;
    int length;
};

int main() {
    int fd = open(DEVICE_NAME, O_RDWR);
    if (fd < 0) {
        std::cerr << "Failed to open device" << std::endl;
        return -1;
    }

    // Set the size of the queue
    int size = 1024;
    if (ioctl(fd, IOCTL_SET_SIZE, &size) < 0) {
        std::cerr << "Failed to set size" << std::endl;
        close(fd);
        return -1;
    }

    // Push data into the queue
    const char *data_to_push = "Hello, Circular Queue!";
    struct queue_data qdata;
    qdata.data = (char *)malloc(strlen(data_to_push) + 1);
    strcpy(qdata.data, data_to_push);
    qdata.length = strlen(data_to_push);

    if (ioctl(fd, IOCTL_PUSH_DATA, &qdata) < 0) {
        std::cerr << "Failed to push data" << std::endl;
        free(qdata.data);
        close(fd);
        return -1;
    }
    free(qdata.data);

    // Pop data from the queue
    char buffer[256];
    qdata.data = buffer;
    qdata.length = sizeof(buffer);

    if (ioctl(fd, IOCTL_POP_DATA, &qdata) < 0) {
        std::cerr << "Failed to pop data" << std::endl;
        close(fd);
        return -1;
    }

    std::cout << "Popped data: " << buffer << std::endl;

    close(fd);
    return 0;
}