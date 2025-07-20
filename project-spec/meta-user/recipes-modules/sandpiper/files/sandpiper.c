#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/of.h>

// Reserved memory region start
#define PHYS_ADDR 0x18000000
// 32Mbytes of reserved memory
#define MEM_SIZE 0x2000000

// Character device name
#define DEVICE_NAME "sandpiper"

// IOCTL command definition
#define MY_IOCTL_GET_VIRT_ADDR _IOR('k', 0, void*)

struct my_driver_data {
	void __iomem *virt_addr;
    struct cdev cdev;
    struct device *device;
};

static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static long    dev_ioctl(struct file *, unsigned int, unsigned long);

static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .unlocked_ioctl = dev_ioctl,
    .release = dev_release,
};

static int sandpiper_probe(struct platform_device *pdev)
{
    struct my_driver_data *drvdata;
    int ret = 0;
    dev_t dev_num = 0;

    drvdata = devm_kzalloc(&pdev->dev, sizeof(struct my_driver_data), GFP_KERNEL);
    if (!drvdata)
	{
		printk(KERN_INFO "%s: failed to allocate memory for driver data\n", DEVICE_NAME);
        return -ENOMEM;
	}

    drvdata->virt_addr = ioremap(PHYS_ADDR, MEM_SIZE);
    if (!drvdata->virt_addr) {
        printk(KERN_INFO "%s: ioremap failed\n", DEVICE_NAME);
        return -ENOMEM;
    }

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_INFO "%s: failed to allocate character device region\n", DEVICE_NAME);
        iounmap(drvdata->virt_addr);
        return ret;
    }

    cdev_init(&drvdata->cdev, &fops);
    drvdata->cdev.owner = THIS_MODULE;

    ret = cdev_add(&drvdata->cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_INFO "%s: failed to add character device\n", DEVICE_NAME);
        unregister_chrdev_region(dev_num, 1);
        iounmap(drvdata->virt_addr);
        return ret;
    }

    drvdata->device = device_create(class_create(DEVICE_NAME), NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(drvdata->device)) {
        printk(KERN_INFO "%s: failed to create device\n", DEVICE_NAME);

		cdev_del(&drvdata->cdev);
        unregister_chrdev_region(dev_num, 1);
        iounmap(drvdata->virt_addr);

		return PTR_ERR(drvdata->device);
    }

    platform_set_drvdata(pdev, drvdata);

    printk(KERN_INFO "%s: physical address 0x%x mapped to virtual address 0x%p\n", DEVICE_NAME, PHYS_ADDR, drvdata->virt_addr);
    printk(KERN_INFO "%s: character device /dev/%s created\n", DEVICE_NAME, DEVICE_NAME);

    return 0;
}

static void sandpiper_remove(struct platform_device *pdev)
{
    struct my_driver_data *drvdata = platform_get_drvdata(pdev);

    device_destroy(class_create(DEVICE_NAME), MKDEV(MAJOR(drvdata->cdev.dev), MINOR(drvdata->cdev.dev)));
    class_destroy(class_create(DEVICE_NAME));

    cdev_del(&drvdata->cdev);
    unregister_chrdev_region(drvdata->cdev.dev, 1);
    iounmap(drvdata->virt_addr);

    printk(KERN_INFO "%s: virtual address unmapped and character device removed\n", DEVICE_NAME);
}

static int dev_open(struct inode *inode, struct file *file)
{
    struct my_driver_data *drvdata = container_of(inode->i_cdev, struct my_driver_data, cdev);
    file->private_data = drvdata;

	printk(KERN_INFO "%s: device opened\n", DEVICE_NAME);

	return 0;
}

static int dev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "%s: device released\n", DEVICE_NAME);

	return 0;
}

static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct my_driver_data *drvdata = (struct my_driver_data*)file->private_data;
    void __iomem *virt_addr = drvdata->virt_addr;

    switch (cmd) {
        case MY_IOCTL_GET_VIRT_ADDR:
            if (copy_to_user((void __user *)arg, &virt_addr, sizeof(virt_addr)))
                return -EFAULT;
            break;

        default:
            return -ENOTTY;
    }

    return 0;
}

static struct of_device_id sandpiper_of_match[] = {
	{ .compatible = "sandpiper", },
	{0},
};
MODULE_DEVICE_TABLE(of, sandpiper_of_match);

static struct platform_driver sandpiper_driver = {
	.probe		= sandpiper_probe,
	.remove		= sandpiper_remove,
	.driver = {
		.name = "sandpiper",
		.owner = THIS_MODULE,
		.of_match_table = sandpiper_of_match,
	},
};

static int __init sandpiper_init(void)
{
	int res = platform_driver_register(&sandpiper_driver);
	if (res < 0)
	{
		printk(KERN_ERR "%s: failed to register driver\n", DEVICE_NAME);
		return -ENODEV;
	}
  
	printk(KERN_INFO "%s: alive\n", DEVICE_NAME);
	return 0;
}


static void __exit sandpiper_exit(void)
{
	platform_driver_unregister(&sandpiper_driver);
	printk(KERN_ALERT "%s: retired\n", DEVICE_NAME);
}

module_init(sandpiper_init);
module_exit(sandpiper_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Engin Cilasun");
MODULE_DESCRIPTION("system driver for sandpiper device");
