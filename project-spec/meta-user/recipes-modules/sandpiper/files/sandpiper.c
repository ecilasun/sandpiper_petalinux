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
#define AUDIO_CTRL_REGS_ADDR	0x40000000
#define VIDEO_CTRL_REGS_ADDR	0x40001000
#define PALETTE_CTRL_REGS_ADDR	0x40002000
#define VCP_CTRL_REGS_ADDR		0x40003000

// 32Mbytes reserved for device access
#define RESERVED_MEMORY_SIZE	0x2000000
// Device region of access (4Kbytes each)
#define DEVICE_MEMORY_SIZE		0x1000

// Character device name
#define DEVICE_NAME "sandpiper"

// IOCTL command definition
#define SP_IOCTL_GET_VIDEO_CTL		_IOR('k', 0, void*)
#define SP_IOCTL_GET_AUDIO_CTL		_IOR('k', 1, void*)
#define SP_IOCTL_GET_PALETTE_CTL	_IOR('k', 2, void*)
#define SP_IOCTL_AUDIO_READ			_IOR('k', 3, void*)
#define SP_IOCTL_AUDIO_WRITE		_IOW('k', 4, void*)
#define SP_IOCTL_VIDEO_READ			_IOR('k', 5, void*)
#define SP_IOCTL_VIDEO_WRITE		_IOW('k', 6, void*)
#define SP_IOCTL_VCP_READ			_IOR('k', 7, void*)
#define SP_IOCTL_VCP_WRITE			_IOW('k', 8, void*)
#define SP_IOCTL_PALETTE_READ		_IOR('k', 9, void*)
#define SP_IOCTL_PALETTE_WRITE		_IOW('k', 10, void*)
#define SP_IOCTL_GET_VCP_CTL		_IOR('k', 11, void*)

// Video mode control word
#define MAKEVMODEINFO(_cmode, _vmode, _scanEnable) ((_cmode&0x1)<<2) | ((_vmode&0x1)<<1) | (_scanEnable&0x1)

// VPU command fifo commands
#define VPUCMD_SETVPAGE			0x00000000
#define VPUCMD_RESERVED			0x00000001
#define VPUCMD_SETVMODE			0x00000002
#define VPUCMD_SHIFTCACHE		0x00000003
#define VPUCMD_SHIFTSCANOUT		0x00000004
#define VPUCMD_SHIFTPIXEL		0x00000005
#define VPUCMD_SETVPAGE2		0x00000006
#define VPUCMD_SYNCSWAP			0x00000007
#define VPUCMD_WCONTROLREG		0x00000008
#define VPUCMD_WPROGADDR		0x00000009
#define VPUCMD_WPROGWORD		0x0000000A
#define VPUCMD_NOOP				0x000000FF

enum EVideoMode
{
	EVM_320_Wide,
	EVM_640_Wide,
	EVM_Count
};

enum EColorMode
{
	ECM_8bit_Indexed,
	ECM_16bit_RGB,
	ECM_Count
};

enum EVideoScanoutEnable
{
	EVS_Disable,
	EVS_Enable,
	EVS_Count
};

// APU command fifo commands
#define APUCMD_BUFFERSIZE   0x00000000
#define APUCMD_START        0x00000001
#define APUCMD_NOOP         0x00000002
#define APUCMD_SWAPCHANNELS 0x00000003
#define APUCMD_SETRATE      0x00000004

// VCP command fifo commands
#define VCPSETBUFFERSIZE	0x0
#define VCPSTARTDMA			0x1
#define VCPEXEC				0x2

enum EAPUSampleRate
{
	ASR_44_100_Hz = 0,	// 44.1000 KHz
	ASR_22_050_Hz = 1,	// 22.0500 KHz
	ASR_11_025_Hz = 2,	// 11.0250 KHz
	ASR_Halt = 3,		// Halt
};

struct SPIoctl
{
	uint32_t offset;	// Offset within the control register
	uint32_t value;		// Value read or value to write
};

struct my_driver_data {
	volatile uint32_t *audio_ctl;	// User side code has to mmap this address when accessing audio control registers
	volatile uint32_t *video_ctl;	// User side code has to mmap this address when accessing video control registers
	volatile uint32_t *palette_ctl;	// User side code has to mmap this address when accessing palette registers
	volatile uint32_t *vcp_ctl;		// User side code has to mmap this address when accessing VCP control registers
    struct cdev cdev;
    struct device *device;
	uint32_t open_count;
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

	// Reset open file handle count
	drvdata->open_count = 0;

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

	drvdata->palette_ctl = ioremap(PALETTE_CTRL_REGS_ADDR, DEVICE_MEMORY_SIZE);
	if (!drvdata->palette_ctl) {
		printk(KERN_INFO "%s: failed to map palette registers\n", DEVICE_NAME);
		return -ENOMEM;
	}

	drvdata->vcp_ctl = ioremap(VCP_CTRL_REGS_ADDR, DEVICE_MEMORY_SIZE);
	if (!drvdata->vcp_ctl) {
		printk(KERN_INFO "%s: failed to map VCP control registers\n", DEVICE_NAME);
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
	printk(KERN_INFO "%s: palette registers at 0x%x\n", DEVICE_NAME, (uint32_t)drvdata->palette_ctl);
	printk(KERN_INFO "%s: VCP control registers at 0x%x\n", DEVICE_NAME, (uint32_t)drvdata->vcp_ctl);
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
	iounmap(drvdata->palette_ctl);
	iounmap(drvdata->vcp_ctl);

    cdev_del(&drvdata->cdev);
    unregister_chrdev_region(drvdata->cdev.dev, 1);

    printk(KERN_INFO "%s: control registers unmapped and character device removed\n", DEVICE_NAME);
}

static int dev_open(struct inode *inode, struct file *file)
{
    struct my_driver_data *drvdata = container_of(inode->i_cdev, struct my_driver_data, cdev);
    file->private_data = drvdata;

	// Inc reference count
	drvdata->open_count++;

	return 0;
}

static int dev_release(struct inode *inode, struct file *file)
{
	struct my_driver_data *drvdata = container_of(inode->i_cdev, struct my_driver_data, cdev);

	// Decrement reference count
	drvdata->open_count--;

	// Tear down device states
	// This should allow us to restore device state without having to install signal handlers in user space
	if (drvdata->open_count == 0)
	{
		// VPU
		{
			// Set video mode to 640x480x16 RGB and scanout pointing at linux framebuffer
			uint32_t modeflags = MAKEVMODEINFO((uint32_t)ECM_16bit_RGB, (uint32_t)EVM_640_Wide, (uint32_t)EVS_Enable);
			iowrite32(VPUCMD_SETVPAGE, (volatile uint32_t*)(drvdata->video_ctl));
			iowrite32(0x18000000, (volatile uint32_t*)(drvdata->video_ctl));
			iowrite32(VPUCMD_SETVMODE, (volatile uint32_t*)(drvdata->video_ctl));
			iowrite32(modeflags, (volatile uint32_t*)(drvdata->video_ctl));

			// Reset VPU control registers
			iowrite32(VPUCMD_WCONTROLREG | 0, (volatile uint32_t*)(drvdata->video_ctl));

			// Reset video scroll registers
			iowrite32(VPUCMD_SHIFTCACHE, (volatile uint32_t*)(drvdata->video_ctl));
			iowrite32(0, (volatile uint32_t*)(drvdata->video_ctl));
			iowrite32(VPUCMD_SHIFTSCANOUT, (volatile uint32_t*)(drvdata->video_ctl));
			iowrite32(0, (volatile uint32_t*)(drvdata->video_ctl));
			iowrite32(VPUCMD_SHIFTPIXEL, (volatile uint32_t*)(drvdata->video_ctl));
			iowrite32(0, (volatile uint32_t*)(drvdata->video_ctl));
		}

		// APU
		{
			// Stop all audio channels
			iowrite32(APUCMD_SETRATE, (volatile uint32_t*)(drvdata->audio_ctl));
			iowrite32(ASR_Halt, (volatile uint32_t*)(drvdata->audio_ctl));
		}

		// VCP
		{
			// Stop all VCP program activity
			iowrite32(VCPEXEC | 0, (volatile uint32_t*)(drvdata->vcp_ctl));
		}
	}

	return 0;
}

static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct my_driver_data *drvdata = (struct my_driver_data*)file->private_data;

	struct SPIoctl ioctl_data;
	// Copy data from user space
	copy_from_user(&ioctl_data, (void __user *)arg, sizeof(ioctl_data));

    switch (cmd) {
        case SP_IOCTL_GET_VIDEO_CTL:
		{
			ioctl_data.value = drvdata->video_ctl;
		}
		break;

		case SP_IOCTL_GET_AUDIO_CTL:
		{
			ioctl_data.value = drvdata->audio_ctl;
		}
		break;

		case SP_IOCTL_GET_PALETTE_CTL:
		{
			ioctl_data.value = drvdata->palette_ctl;
		}
		break;

		case SP_IOCTL_GET_VCP_CTL:
		{
			ioctl_data.value = drvdata->vcp_ctl;
		}
		break;

		case SP_IOCTL_AUDIO_READ:
		{
			ioctl_data.value = ioread32((volatile uint32_t*)(drvdata->audio_ctl + ioctl_data.offset));
		}
		break;

		case SP_IOCTL_AUDIO_WRITE:
		{
			iowrite32(ioctl_data.value, (volatile uint32_t*)(drvdata->audio_ctl + ioctl_data.offset));
		}
		break;

		case SP_IOCTL_VIDEO_READ:
		{
			ioctl_data.value = ioread32((volatile uint32_t*)(drvdata->video_ctl + ioctl_data.offset));
		}
		break;

		case SP_IOCTL_VIDEO_WRITE:
		{
			iowrite32(ioctl_data.value, (volatile uint32_t*)(drvdata->video_ctl + ioctl_data.offset));
		}
		break;

		case SP_IOCTL_PALETTE_READ:
		{
			ioctl_data.value = ioread32((volatile uint32_t*)(drvdata->palette_ctl + ioctl_data.offset));
		}
		break;

		case SP_IOCTL_PALETTE_WRITE:
		{
			iowrite32(ioctl_data.value, (volatile uint32_t*)(drvdata->palette_ctl + ioctl_data.offset));
		}
		break;

		case SP_IOCTL_VCP_READ:
		{
			ioctl_data.value = ioread32((volatile uint32_t*)(drvdata->vcp_ctl + ioctl_data.offset));
		}
		break;

		case SP_IOCTL_VCP_WRITE:
		{
			iowrite32(ioctl_data.value, (volatile uint32_t*)(drvdata->vcp_ctl + ioctl_data.offset));
		}
		break;

		default:
            return -ENOTTY;
    }

	// Copy the ioctl_data structure back to user space
	copy_to_user((void __user *)arg, &ioctl_data, sizeof(ioctl_data));

    return 0;
}

static int dev_mmap(struct file *file, struct vm_area_struct *vma)
{
	//struct my_driver_data *drvdata = (struct my_driver_data*)file->private_data;
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
