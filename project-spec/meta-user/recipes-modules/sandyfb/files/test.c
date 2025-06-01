#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/console.h>

#define PCI_VENDOR_ID_XXX   0x10F610F6
#define PCI_DEVICE_ID_XXX   0x2864C826
#define PCI_CLASS_MASK      0x00FF

#define FB_NAME             "MFCC8556_vfb_"
#define FB_MAJOR            29
#define VIDEOMEMSIZE        (480*800*3)

#define FBIO_TEST           _IO('F', 0x21)

/* Global variable */

/*
 * Driver data
 */
//static char *videomemory;
static int fb_count = 3;
static u_long videomemorysize = VIDEOMEMSIZE;

/* array of framebuffer */
static struct fb_info **g_fb_list;

static struct fb_fix_screeninfo fix_default __initdata = {
    .id         =   FB_NAME,
    .smem_len   =   VIDEOMEMSIZE,
    .type       =   FB_TYPE_PACKED_PIXELS,
    .visual     =   FB_VISUAL_PSEUDOCOLOR,
    .xpanstep   =   0,
    .ypanstep   =   0,
    .ywrapstep  =   0, 
    .accel      =   FB_ACCEL_NONE,
};


static struct fb_var_screeninfo var_default __initdata = {
    .xres           = 800,
    .yres           = 480,
    .xres_virtual   = 800,
    .yres_virtual   = 480,
    .bits_per_pixel = 24,
    .red            = {0, 8, 0},
    .green          = {0, 8, 0},
    .blue           = {0, 8, 0},
    .grayscale      = 0,
    .activate       = FB_ACTIVATE_TEST,
    .height         = -1,
    .width          = -1,
    .pixclock       = 30060,
    .vmode          = FB_VMODE_NONINTERLACED,
};

static int xxxfb_init(void);
static int register_fb(struct fb_info *info);
static int set_screen_base(struct fb_info* info);
static int init_fb_info(struct fb_info *info, struct fb_ops *fbops, unsigned int id_no);
static int alloc_fb_info (struct fb_info **info);


/* ------------ Accelerated Functions --------------------- */

/*
 * We provide our own functions if we have hardware acceleration
 * or non packed pixel format layouts. If we have no hardware 
 * acceleration, we can use a generic unaccelerated function. If using
 * a pack pixel format just use the functions in cfb_*.c. Each file 
 * has one of the three different accel functions we support.
 */

/**
 *      xxxfb_fillrect - REQUIRED function. Can use generic routines if 
 *           non acclerated hardware and packed pixel based.
 *           Draws a rectangle on the screen.       
 *
 *      @info: frame buffer structure that represents a single frame buffer
 *  @region: The structure representing the rectangular region we 
 *       wish to draw to.
 *
 *  This drawing operation places/removes a retangle on the screen 
 *  depending on the rastering operation with the value of color which
 *  is in the current color depth format.
 */
void xxxfb_fillrect(struct fb_info *info, const struct fb_fillrect *region)
{
/*  Meaning of struct fb_fillrect
 *
 *  @dx: The x and y coordinates of the upper left hand corner of the 
 *  @dy: area we want to draw to. 
 *  @width: How wide the rectangle is we want to draw.
 *  @height: How tall the rectangle is we want to draw.
 *  @color: The color to fill in the rectangle with. 
 *  @rop: The raster operation. We can draw the rectangle with a COPY
 *        of XOR which provides erasing effect. 
 */

    struct fb_fillrect *tmp_fillrect;

    /*ptr=(unsigned long*)info->screen_base;
    //fill the screen base ///
    for(i=0; i<800*10; i++){
        *ptr=0x0000FF00;
        ptr++;
    }*/

    printk(KERN_DEBUG "\nfb_fillrect()");
    printk(KERN_DEBUG "\nFix Screen Info.id =%s\n", info->fix.id); 
/*  printk(KERN_INFO    "\nstruct fb_fillrect:\n"
                        "dx = %d\n"
                        "dy = %d\n"
                        "width = %d\n"
                        "height = %d\n"
                        "color = 0x%08X\n"
                        "rop = 0x%X\n___\n"
                        , region->dx, region->dy, region->width, region->height, region->color, region->rop); 
                        */
//  printk(KERN_INFO "_in fill_rectangle : screen_base = 0x%X\n0x_%02X_%02X_%02X_%02X\n...\n",(unsigned int)info->screen_base, *info->screen_base, *(info->screen_base+1), *(info->screen_base+2),*(info->screen_base+3));


    tmp_fillrect = kmalloc(sizeof(struct fb_fillrect), GFP_KERNEL);
    *tmp_fillrect = *region;

/*tmp_fillrect->dx=400;
    tmp_fillrect->dy=200;
    tmp_fillrect->width=100;
    tmp_fillrect->height=50;
    tmp_fillrect->color=0x0000FF00;
    tmp_fillrect->rop=0x0;

*/
//tmp = copy_from_user(tmp_fillrect, region, sizeof(struct fb_fillrect));

    printk(KERN_INFO    "\nstruct fb_fillrect:\n"
                        "dx = %d\n"
                        "dy = %d\n"
                        "width = %d\n"
                        "height = %d\n"
                        "color = 0x%08X\n"
                        "rop = 0x%X\n___\n"
                        , tmp_fillrect->dx, tmp_fillrect->dy, tmp_fillrect->width, tmp_fillrect->height, tmp_fillrect->color, tmp_fillrect->rop);

    //if (tmp) printk(KERN_ERR "**ERROR: copy_from_user = %d\n", tmp);

    cfb_fillrect(info, region);

}

int xxxfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg){

    int ret = 0;

    printk(KERN_INFO "fb_ioctl()");
    mutex_lock(&info->lock);

    switch(cmd) {
    case FBIOGET_VSCREENINFO:
        printk(KERN_DEBUG "FBIOGET_VSCREENINFO");
        break;

    case FBIOGET_FSCREENINFO:
        printk(KERN_DEBUG "FBIOGET_FSCREENINFO");
        break;

    case FBIO_TEST:
        printk(KERN_DEBUG "FBIO_TEST");
        break;

    default: 
        printk(KERN_DEBUG "ioctl DEFAULT");
        break;
    }

    mutex_unlock(&info->lock);

    return ret; 
}


    /*
     *  Frame buffer operations
     */

static struct fb_ops xxxfb_ops = {
    .owner          = THIS_MODULE,
    .fb_open        = xxxfb_open,
    .fb_read        = fb_sys_read,
    .fb_write       = fb_sys_write,
    .fb_release     = xxxfb_release,
    .fb_check_var   = xxxfb_check_var,
    .fb_set_par     = xxxfb_set_par,
    //.fb_setcolreg = xxxfb_setcolreg,
    .fb_blank       = xxxfb_blank,
    .fb_pan_display = xxxfb_pan_display,
    .fb_fillrect    = xxxfb_fillrect,   /* Needed !!! */
    .fb_copyarea    = xxxfb_copyarea,   /* Needed !!! */
    .fb_imageblit   = xxxfb_imageblit,  /* Needed !!! */
    .fb_cursor      = xxxfb_cursor,     /* Optional !!! */
    .fb_sync        = xxxfb_sync,
    .fb_ioctl       = xxxfb_ioctl,
    .fb_mmap        = xxxfb_mmap,
};

/* ------------------------------------------------------------------------- */

...

    /*
     *  Modularization
     */

module_init(xxxfb_init);
module_exit(xxxfb_exit);

MODULE_LICENSE("GPL");
