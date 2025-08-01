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

// Shared memory physical address
#define PHYS_ADDR 0x18000000

// Control registers physical address
#define AUDIO_CTRL_REGS_ADDR 0x40000000
#define VIDEO_CTRL_REGS_ADDR 0x40001000

// 32Mbytes reserved for device access
#define RESERVED_MEMORY_SIZE	0x2000000
// Device region of access
#define DEVICE_MEMORY_SIZE		0x1000

// Character device name
#define DEVICE_NAME "sandpiper"

// IOCTL command definition
#define MY_IOCTL_GET_VIDEO_CTL	_IOR('k', 0, void*)
#define MY_IOCTL_GET_AUDIO_CTL	_IOR('k', 1, void*)
#define SP_IOCTL_AUDIO_READ		_IOR('k', 2, uint32_t*)
#define SP_IOCTL_AUDIO_WRITE	_IOW('k', 3, uint32_t*)
#define SP_IOCTL_VIDEO_READ		_IOR('k', 4, uint32_t*)
#define SP_IOCTL_VIDEO_WRITE	_IOW('k', 5, uint32_t*)

struct my_driver_data {
	volatile uint32_t *audio_ctl;	// User side code has to mmap this address when accessing audio control registers
	volatile uint32_t *video_ctl;	// User side code has to mmap this address when accessing video control registers
    struct cdev cdev;
    struct device *device;
};

static int		dev_open(struct inode *, struct file *);
static int		dev_release(struct inode *, struct file *);
static long		dev_ioctl(struct file *, unsigned int, unsigned long);
static int		dev_mmap(struct file *file, struct vm_area_struct *vma);

static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .unlocked_ioctl = dev_ioctl,
	.mmap = dev_mmap,
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

	drvdata->audio_ctl = ioremap(AUDIO_CTRL_REGS_ADDR, DEVICE_MEMORY_SIZE);
	if (!drvdata->audio_ctl) {
		printk(KERN_INFO "%s: failed to map audio control registers\n", DEVICE_NAME);
		return -ENOMEM;
	}

	drvdata->video_ctl = ioremap(VIDEO_CTRL_REGS_ADDR, DEVICE_MEMORY_SIZE);
	if (!drvdata->video_ctl) {
		printk(KERN_INFO "%s: failed to map video control registers\n", DEVICE_NAME);
		return -ENOMEM;
	}

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_INFO "%s: failed to allocate character device region\n", DEVICE_NAME);
        return ret;
    }

    cdev_init(&drvdata->cdev, &fops);
    drvdata->cdev.owner = THIS_MODULE;

    ret = cdev_add(&drvdata->cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_INFO "%s: failed to add character device\n", DEVICE_NAME);

		cdev_del(&drvdata->cdev);
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }

    drvdata->device = device_create(class_create(DEVICE_NAME), NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(drvdata->device)) {
        printk(KERN_INFO "%s: failed to create device\n", DEVICE_NAME);

		cdev_del(&drvdata->cdev);
        unregister_chrdev_region(dev_num, 1);
		return PTR_ERR(drvdata->device);
    }

    platform_set_drvdata(pdev, drvdata);

    printk(KERN_INFO "%s: audio control registers at 0x%x\n", DEVICE_NAME, (uint32_t)drvdata->audio_ctl);
    printk(KERN_INFO "%s: video control registers at 0x%x\n", DEVICE_NAME, (uint32_t)drvdata->video_ctl);
	printk(KERN_INFO "%s: character device /dev/%s created\n", DEVICE_NAME, DEVICE_NAME);

    return 0;
}

static void sandpiper_remove(struct platform_device *pdev)
{
    struct my_driver_data *drvdata = platform_get_drvdata(pdev);

    device_destroy(class_create(DEVICE_NAME), MKDEV(MAJOR(drvdata->cdev.dev), MINOR(drvdata->cdev.dev)));
    class_destroy(class_create(DEVICE_NAME));

	iounmap(drvdata->video_ctl);
	iounmap(drvdata->audio_ctl);

    cdev_del(&drvdata->cdev);
    unregister_chrdev_region(drvdata->cdev.dev, 1);

    printk(KERN_INFO "%s: control registers unmapped and character device removed\n", DEVICE_NAME);
}

static int dev_open(struct inode *inode, struct file *file)
{
    struct my_driver_data *drvdata = container_of(inode->i_cdev, struct my_driver_data, cdev);
    file->private_data = drvdata;
	return 0;
}

static int dev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct my_driver_data *drvdata = (struct my_driver_data*)file->private_data;

    switch (cmd) {
        case MY_IOCTL_GET_VIDEO_CTL:
		{
            if (copy_to_user((void __user *)arg, &drvdata->video_ctl, sizeof(drvdata->video_ctl)))
                return -EFAULT;
		}
		break;

		case MY_IOCTL_GET_AUDIO_CTL:
		{
            if (copy_to_user((void __user *)arg, &drvdata->audio_ctl, sizeof(drvdata->audio_ctl)))
                return -EFAULT;
		}
		break;

		case SP_IOCTL_AUDIO_READ:
		{
			uint32_t value = ioread32((volatile uint32_t*)(drvdata->audio_ctl));
			if (copy_to_user((void __user *)arg, &value, sizeof(value)))
				return -EFAULT;
		}
		break;

		case SP_IOCTL_AUDIO_WRITE:
		{
			uint32_t value;
			if (copy_from_user(&value, (void __user *)arg, sizeof(value)))
				return -EFAULT;
			iowrite32(value, (volatile uint32_t*)(drvdata->audio_ctl));
		}
		break;

		case SP_IOCTL_VIDEO_READ:
		{
			uint32_t value = ioread32((volatile uint32_t*)(drvdata->video_ctl));
			if (copy_to_user((void __user *)arg, &value, sizeof(value)))
				return -EFAULT;
		}
		break;

		case SP_IOCTL_VIDEO_WRITE:
		{
			uint32_t value;
			if (copy_from_user(&value, (void __user *)arg, sizeof(value)))
				return -EFAULT;
			iowrite32(value, (volatile uint32_t*)(drvdata->video_ctl));
		}
		break;

		default:
            return -ENOTTY;
    }

    return 0;
}

static int dev_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct my_driver_data *drvdata = (struct my_driver_data*)file->private_data;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long physical_addr = 0;

	if (offset == PHYS_ADDR)
	{
		physical_addr = PHYS_ADDR;
		if (size > RESERVED_MEMORY_SIZE)
		{
			printk(KERN_INFO "%s: mmap request exceeds memory region\n", DEVICE_NAME);
			return -EINVAL;
		}
	}
	else
	{
		printk(KERN_INFO "%s: invalid mmap offset 0x%lx\n", DEVICE_NAME, offset);
		return -EINVAL;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, physical_addr >> PAGE_SHIFT, size, vma->vm_page_prot))
	{
		printk(KERN_INFO "%s: failed to remap page\n", DEVICE_NAME);
		return -EAGAIN;
	}

	//printk(KERN_INFO "%s: mmap successful, mapped physical address 0x%lx to virtual address 0x%x\n", DEVICE_NAME, physical_addr, (uint32_t)vma->vm_start);

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
MODULE_DESCRIPTION("platform driver for sandpiper");
