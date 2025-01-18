// circular_queue.cpp
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/semaphore.h>

#define DEVICE_NAME "circular queue"
#define IOCTL_SET_SIZE _IOW('q', 1, int)
#define IOCTL_PUSH_DATA _IOW('q', 2, struct queue_data)
#define IOCTL_POP_DATA _IOR('q', 3, struct queue_data)

struct queue_data {
    char *data;
    int length;
};

class CircularQueue {
public:
    CircularQueue() : buffer(nullptr), head(0), tail(0), max_size(0), current_size(0) {
        init_waitqueue_head(&read_queue);
        init_waitqueue_head(&write_queue);
        sema_init(&sem, 1);
    }

    ~CircularQueue() {
        if (buffer) {
            kfree(buffer);
        }
    }

    bool set_size(int size) {
        if (buffer) {
            kfree(buffer);
        }
        buffer = (char *)kmalloc(size, GFP_KERNEL);
        if (!buffer) {
            return false;
        }
        max_size = size;
        head = tail = current_size = 0;
        return true;
    }

    bool push_data(const char *data, int length) {
        if (current_size + length > max_size) {
            return false; // Not enough space
        }
        memcpy(buffer + head, data, length);
        head = (head + length) % max_size;
        current_size += length;
        return true;
    }

    bool pop_data(char *data, int length) {
        if (current_size == 0) {
            return false; // No data to pop
        }
        int length_to_pop = min(length, current_size);
        memcpy(data, buffer + tail, length_to_pop);
        tail = (tail + length_to_pop) % max_size;
        current_size -= length_to_pop;
        return true;
    }

    void wait_for_data() {
        wait_event_interruptible(read_queue, current_size > 0);
    }

    void notify_data() {
        wake_up_interruptible(&write_queue);
    }

    void wait_for_space() {
        wait_event_interruptible(write_queue, current_size < max_size);
    }

    void notify_space() {
        wake_up_interruptible(&read_queue);
    }

    int get_current_size() const {
        return current_size;
    }

private:
    char *buffer;
    int head;
    int tail;
    int max_size;
    int current_size;
    wait_queue_head_t read_queue;
    wait_queue_head_t write_queue;
    struct semaphore sem;
};

static CircularQueue *cqueue;
static int major_number;

static int device_open(struct inode *inode, struct file *file) {
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    return 0;
}

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case IOCTL_SET_SIZE: {
            int size;
            if (copy_from_user(&size, (int __user *)arg, sizeof(int))) {
                return -EFAULT;
            }
            if (!cqueue->set_size(size)) {
                return -ENOMEM;
            }
            break;
        }
        case IOCTL_PUSH_DATA: {
            struct queue_data qdata;
            if (copy_from_user(&qdata, (struct queue_data __user *)arg, sizeof(struct queue_data))) {
                return -EFAULT;
            }
            down(&cqueue->sem);
            cqueue->wait_for_space();
            if (!cqueue->push_data(qdata.data, qdata.length)) {
                up(&cqueue->sem);
                return -EINVAL;
            }
            cqueue->notify_data();
            up(&cqueue->sem);
            break;
        }
        case IOCTL_POP_DATA: {
            struct queue_data qdata;
            if (copy_from_user(&qdata, (struct queue_data __user *)arg, sizeof(struct queue_data))) {
 return -EFAULT;
            }
            down(&cqueue->sem);
            cqueue->wait_for_data();
            if (!cqueue->pop_data(qdata.data, qdata.length)) {
                up(&cqueue->sem);
                return -EINVAL;
            }
            if (copy_to_user((struct queue_data __user *)arg, &qdata, sizeof(struct queue_data))) {
                up(&cqueue->sem);
                return -EFAULT;
            }
            cqueue->notify_space();
            up(&cqueue->sem);
            break;
        }
        default:
            return -EINVAL;
    }
    return 0;
}

static struct file_operations fops = {
    .open = device_open,
    .release = device_release,
    .unlocked_ioctl = device_ioctl,
};

static int __init circular_queue_init(void) {
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        return major_number;
    }
    cqueue = new CircularQueue();
    return 0;
}

static void __exit circular_queue_exit(void) {
    delete cqueue;
    unregister_chrdev(major_number, DEVICE_NAME);
}

module_init(circular_queue_init);
module_exit(circular_queue_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dynamic Circular Queue Character Device");
MODULE_AUTHOR("Parmeshwar Bodake");