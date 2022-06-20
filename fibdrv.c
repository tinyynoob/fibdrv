#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include "base.h"
#include "ubignum.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

#define MAX_LENGTH 100000

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static ubn_t *fib_sequence(long long k)
{
    ubn_t *fib[2];
    bool flag = true;
    fib[0] = ubignum_init(UBN_DEFAULT_CAPACITY);
    flag &= !!fib[0];
    ubignum_set_zero(fib[0]);
    fib[1] = ubignum_init(UBN_DEFAULT_CAPACITY);
    flag &= !!fib[1];
    ubignum_set_u64(fib[1], 1);

    for (int i = 2; i <= k; i++)
        flag &= ubignum_add(fib[0], fib[1], &fib[i & 1]);
    ubignum_free(fib[(k & 1) ^ 1]);
    if (unlikely(!flag))
        printk(KERN_INFO "@flag in fib_sequence() reported false\n");
    return fib[k & 1];
}

static ubn_t *fib_fast(long long k)
{
    ubn_t *fast[5];
    bool flag = true;
    if (k == 0) {
        fast[2] = ubignum_init(UBN_DEFAULT_CAPACITY);
        flag &= !!fast[2];
        ubignum_set_zero(fast[2]);
        goto end;
    } else if (k == 1) {
        fast[2] = ubignum_init(UBN_DEFAULT_CAPACITY);
        flag &= !!fast[2];
        ubignum_set_u64(fast[2], 1);
        goto end;
    }

    for (int i = 0; i < 5; i++) {
        fast[i] = ubignum_init(UBN_DEFAULT_CAPACITY);
        flag &= !!fast[i];
    }
    ubignum_set_zero(fast[1]);
    ubignum_set_u64(fast[2], 1);
    int n = 1;
    for (int currbit = 1 << (32 - __builtin_clzll(k) - 1 - 1); currbit;
         currbit = currbit >> 1) {
        /* compute 2n-1 */
        flag &= ubignum_square(fast[1], &fast[0]);
        flag &= ubignum_square(fast[2], &fast[3]);
        // flag &= ubignum_mult(fast[1], fast[1], &fast[0]);
        // flag &= ubignum_mult(fast[2], fast[2], &fast[3]);
        flag &= ubignum_add(fast[0], fast[3], &fast[3]);
        /* compute 2n */
        flag &= ubignum_left_shift(fast[1], 1, &fast[4]);
        flag &= ubignum_add(fast[4], fast[2], &fast[4]);
        flag &= ubignum_mult(fast[4], fast[2], &fast[4]);
        n *= 2;
        if (k & currbit) {
            flag &= ubignum_add(fast[3], fast[4], &fast[0]);
            n++;
            ubignum_swapptr(&fast[2], &fast[0]);
            ubignum_swapptr(&fast[1], &fast[4]);
        } else {
            ubignum_swapptr(&fast[2], &fast[4]);
            ubignum_swapptr(&fast[1], &fast[3]);
        }
    }
    ubignum_free(fast[0]);
    ubignum_free(fast[1]);
    ubignum_free(fast[3]);
    ubignum_free(fast[4]);
end:;
    if (unlikely(!flag))
        printk(KERN_INFO "@flag in fib_fast() reported false\n");
    return fast[2];
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    ubn_t *N = fib_sequence(*offset);
    char *s = ubignum_2decimal(N);
    ubignum_free(N);
    int len = strlen(s) + 1;
    if (copy_to_user(buf, s, len))
        return -EFAULT;
    kfree(s);
    return (ssize_t) len;
}

/* write operation is skipped */
/* try to measure time within this function */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    ktime_t kt;
    ubn_t *N = NULL;  // do not initialize
    switch (size) {
    case 0:
        kt = ktime_get();
        N = fib_sequence(*offset);
        kt = ktime_sub(ktime_get(), kt);
        break;
    case 1:
        kt = ktime_get();
        N = fib_fast(*offset);
        kt = ktime_sub(ktime_get(), kt);
        break;
    default:
        return 0;
    }
    ubignum_free(N);
    return (ssize_t) ktime_to_ns(kt);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);  // add device to system

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
