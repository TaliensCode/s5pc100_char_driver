 /* 实现目标:
 * 字符设备框架
 *
 * 实现步骤:
 * 1. 描述字符设备
 *    1.1 属性
 *        继承cdev, 增加自己属性
 *
 *    1.2 方法
 *        file_operations
 *           
 * 2. 创建字符设备
 *    2.1 分配内存
 *        属性存放的内存: 分配到直接地址映射区，kmalloc分配
 *        file_operations变量: 静态区
 *
 *    2.2 初始化
 *        属性初始化在probe函数中
 *        file_operations变量 直接定义时初始化
 *
 * 3. 添加字符设备到内核
 *    3.1 申请设备号(查阅空闲设备号: )
 *        register_chrdev_region/unregister_chrdev_region
 *        
 *    3.2 添加
 *        cdev_add/cdev_del
 *    
 *
 * 调试步骤:
 * 1. 编译
 *    $ make
 *
 * 2. 拷贝到开发板
 *    $ cp led.ko /source/rootfs/lib/modules/2.6.35/
 *
 * 3. 加载模块
 *    # insmod /lib/modules/2.6.35/led.ko
 *
 * 4. 确认设备号注册成功
 *    # cat /proc/devices
 *
 * 5. 创建设备文件
 *    # mknod /dev/led0 c 250 0
 *    # mknod /dev/led1 c 250 1
 *
 * 6. 确认字符设备操作成功
 *    5.1 编译应用程序
 *        $ arm-cortex_a8-linux-gnueabi-gcc led_app.c -o led_app
 *
 *    5.2 拷贝到开发板
 *        cp led_app /source/rootfs/root/
 *
 *    5.3 运行, 观察现象
 *        # /root/led_app
 *
 * 7. 卸载模块
 *    # rmmod led
 *
 * 8. 确认字符删除成功
 *    调用第6步的应用程序
 *
 * 9. 确认设备号注销成功
 *    # cat /proc/devices
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
// kmalloc
#include <linux/slab.h>
// cdev
#include <linux/cdev.h>
#include <linux/fs.h>
//class
#include <linux/device.h>
//resource 
#include <linux/ioport.h>
#include <linux/io.h>
//自旋锁
#include <linux/spinlock.h>

// 1.1 属性
struct led_cdev{
	struct cdev cdev;
	
	int on;                     // 0-turn off  1-turn on

    spinlock_t lock;    
    struct resource *res;  //保存获得的资源(物理地址等)
    void __iomem *regs;   //保存设备获得的虚拟地址
};

/********************************************************************/
//定义一个全局类
struct class *led_class;

//定义存放参数的变量
int led_major = 250;

//声明模块参数
module_param(led_major, int, 0400);

//添加参数描述信息
MODULE_PARM_DESC(led_major, "led device major");

/********************************************************************/

/********************LED 操作****************************************/
void led_dev_init(struct led_cdev *led, int minor)
{
    int pos = (minor + 1) << 2;
    unsigned long LED_CON;
    unsigned long LED_DAT;

    //gpio ctrl configuration
    LED_CON = readl(led->regs);
    LED_CON &= ~(0xf << pos);
    LED_CON |= (0x1 << pos);
    writel(LED_CON, led->regs);
    //gpio data

    LED_DAT = readl(led->regs + 4);
    LED_DAT &= ~(0x1 << (minor + 1));
    writel(LED_DAT, led->regs + 4);
    
    led->on = 0;
}

//on 1
void led_dev_on(struct led_cdev *led)
{
        
    unsigned long LED_DAT;
    LED_DAT = readl(led->regs + 4);
    LED_DAT |= (0x1 << (MINOR(led->cdev.dev) + 1));
    writel(LED_DAT, led->regs + 4);
}



//off 0
void led_dev_off(struct led_cdev *led)
{
    unsigned long LED_DAT;
    LED_DAT = readl(led->regs + 4);
    LED_DAT &= ~(0x1 << (MINOR(led->cdev.dev) + 1));
    writel(LED_DAT, led->regs + 4);
}

/************************************************************/

// 1.2 方法
/*
 * fd = open("/dev/led0", O_RDWR);
 * @brief						被open系统调用间接(vfs直接)调用
 * @param[in]				inode													未打开的文件结构体指针
 * @param[in | out] filp													打开的文件结构指针
 * @return					@li	0													成功
 *									@li < 0												错误码
 * @notes						返回值，返回给vfs，vfs返回给应用程序
 *                  @li 0                         vfs返回文件描述符给应用程序
 *                  @li < 0												vfs将错误码放入errno，并且返回-1给应用程序
 */
//缺陷：每次调用open都创建一个文件描述符
int led_open(struct inode *inode, struct file *filp)
{
    /*********************************************************/
    struct led_cdev *led = container_of(inode->i_cdev, struct led_cdev, cdev);
    inode->i_private = led;

    //获得与释放锁
    spin_lock(&led->lock);
    led_dev_on(led);
    spin_unlock(&led->lock);
    /*********************************************************/
	printk("%s\n", __func__);
	
	return 0;
}

/*
 * close(fd);
 * @brief						被close系统调用间接(vfs直接)调用
 * @param[in]				inode													未打开的文件结构体指针
 * @param[in | out] filp													打开的文件结构指针
 * @return					@li	0													成功
 *									@li < 0												错误码
 * @notes						返回值，返回给vfs，vfs返回给应用程序
 *                  @li 0                         vfs返回文件描述符给应用程序
 *                  @li < 0												vfs将错误码放入errno，并且返回-1给应用程序
 */
int led_release(struct inode *inode, struct file *filp)
{
    //struct led_cdev *led = container_of(inode->i_cdev, struct led_cdev, cdev);
    struct led_cdev *led = inode->i_private;

    //获得与释放锁
    spin_lock(&led->lock);
    led_dev_off(led);	
    spin_unlock(&led->lock);

	printk("%s\n", __func__);
	return 0;
}


const struct file_operations fops = {
	.open = led_open,
	.release = led_release,
};

int led_probe(struct platform_device *pdev)
{
	int ret = 0;
	dev_t devno = MKDEV(led_major, pdev->id);
	struct led_cdev *led;
    struct device *device;
    struct resource *res;
	
	printk("%s\n", __func__);

// 2.1 分配内存
/*
 * @brief 			驱动中分配内存
 * @param[in]		size												分配内存字节数
 * @param[in]		flags												分配内存标准(控制分配内存的行为)
 *							GFP_ATOMIC									分配内存期间不会休眠
 *							GFP_KERNEL									分配内存期间可能会休眠
 *              GFP_DMA											分配的内存给DMA用
 * @return	  	分配内存的地址(已经映射物理内存的虚拟地址)
 *              NULL												分配失败
 * @notes			  分配的内存对应的物理内存连续
 *              分配的内存是以32字节为单位，分配的最大为128k
 * void *kmalloc(size_t size, gfp_t flags);
 */
	led = (struct led_cdev *)kmalloc(sizeof(struct led_cdev), GFP_KERNEL);
	if (NULL == led){
		ret = -ENOMEM;
		goto exit;
	}
	
/*
 * @brief 			将驱动数据放入平台设备
 * @param[out]	pdev												平台设备指针
 * @param[in]		drvdata											驱动数据
 * void platform_set_drvdata(struct platform_device *pdev, void *drvdata);
 */
	platform_set_drvdata(pdev, led);
	
    /******************************************************************/


    // 获取寄存器资源(物理地址等)
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (res == NULL){
        ret = -ENOENT;
        goto err_platform_get_resource;
    }
    // 保存资源
    led->res = res;


    // 虚拟内存映射
    led->regs = ioremap(res->start, resource_size(res));
    if (led->regs == NULL){
        ret = -EINVAL;
        goto err_ioremap;
    }


    
    /******************************************************************/

	// 2.2 初始化
/*
 * @brief 			初始化字符设备
 * @param[out]	cdev												字符设备指针
 * @param[in]		fops												操作函数指针
 * @notes				本函数没有.owner程序，需要在函数后加.owner = THIS_MODULE
 * void cdev_init(struct cdev *cdev, const struct file_operations *fops)
 */
	cdev_init(&led->cdev, &fops);
	led->cdev.owner = THIS_MODULE;
	
   /************************led_init*******************************/
    led_dev_init(led, MINOR(devno));
   /************************led_init*******************************/
    	
    //初始化自旋锁
    spin_lock_init(&led->lock);
	
/*
 * @brief						分配设备编号
 * @param[in]				first												起始设备编号
 * @param[in]				count												分配编号数量
 * @param[in]				name												设备名称(在/proc/devices文件中可见) 
 * @return					=0													分配成功
 * 									<0													错误码
 * int register_chrdev_region(dev_t first, unsigned int count, char *name);
 */
	ret = register_chrdev_region(devno, 1, "led");
	if (ret < 0){
		goto err_register_chrdev_region;
	}
	
	// 3.2 添加
/*
 * @brief						增加一个字符设备到系统
 * @param[in]				p														描述字符设备的struct cdev结构
 * @param[in]				dev													关联到这个设备的第一个设备号
 * @param[in]				count												关联到这个设备的设备号个数
 * @return					0														添加成功
 *									<0													错误码
 * @notes						函数执行成功后，立即可以使用该字符设备，也就是说调用这个之前设备驱动已经完全就绪
 * int cdev_add(struct cdev *p, dev_t dev, unsigned count);
 */
	ret = cdev_add(&led->cdev, devno, 1);
	if (ret < 0){
		goto err_cdev_add;
	}

    //建立设备并注册到sysfs中
   device = device_create(led_class, NULL, devno, NULL, "led%d", pdev->id);
   if (IS_ERR(device)){
       ret = PTR_ERR(device);
       goto err_device_create;
   }
	
	goto exit;

err_device_create:
    cdev_del(&led->cdev);

err_cdev_add:
	unregister_chrdev_region(led->cdev.dev, 1);
    printk("err_cdev_add\n");
	
err_register_chrdev_region:
    printk("err_register_chrdev_region\n");
	kfree(led);

err_ioremap:
    printk("err_ioremap\n");
    release_mem_region(res->start, resource_size(res));

err_platform_get_resource:
    printk("err_platform_get_resource\n");
    kfree(led);

	
exit:
	return ret;
}

int led_remove(struct platform_device *pdev)
{
	struct led_cdev *led;
    //struct resource *res;
	printk("%s\n", __func__);

	
/*
 * @brief 			从平台设备获得驱动数据
 * @param[in]		pdev												平台设备指针
 * @return			驱动数据
 * 	void *platform_get_drvdata(const struct platform_device *pdev);
 */
	led = platform_get_drvdata(pdev);

//设备销毁
    device_destroy(led_class, led->cdev.dev);
	
/*
 * @brief						删除一个字符设备
 * @param[in]				cdev												描述字符设备的struct cdev结构
 * void cdev_del(struct cdev *dev);
 */
	cdev_del(&led->cdev);

/*
 * @brief						回收设备编号
 * @param[in]				first												第一个次编号
 * @param[in]				count												分配编号数量
 * void unregister_chrdev_region(dev_t first, unsigned int count);
 */
	unregister_chrdev_region(led->cdev.dev, 1);

    //取消映射
    iounmap(led->regs);
    //释放io内存
  //  res = led->res;
 //   release_mem_region(res->start, resource_size(res));

/*
 * @brief 			释放内存
 * @param[in]		addr													内存的地址
 * void kfree(const void *addr);
 */


	kfree(led);
    
	return 0;
}

// 设备列表(驱动多类相近设备时)
struct platform_device_id led_ids[] = {
	[0] = {
		.name = "led",
	},
/*[1] = {
		.name = "led1",
	},*/
	{/* end */}
};
MODULE_DEVICE_TABLE(platform, led_ids);

struct platform_driver led_driver = {
	.probe = led_probe,
	.remove = led_remove,
	
	// 只驱动一类设备
	// .id_table = NULL,
	
	// 驱动多类相近设备时
	.id_table = led_ids,
	.driver = {
    // 驱动名，当设备列表都不存在时，用这个名字匹配设备
		.name = "led",
		.owner = THIS_MODULE,
		
		// 设备列表(和设备树中的设备列表进行匹配)
//		.of_match_table = ...
	}
};

int __init led_init(void)
{
	int ret = 0;
	
	printk("%s\n", __func__);
	
    led_class = class_create(THIS_MODULE, "led");
    if (IS_ERR(led_class)){
        ret = PTR_ERR(led_class);
        goto exit;
    }
	ret = platform_driver_register(&led_driver);
    if (ret < 0){
        class_destroy(led_class);
    }

exit:	
	return ret;
}

void __exit led_exit(void)
{
	printk("%s\n", __func__);

	platform_driver_unregister(&led_driver);

    class_destroy(led_class);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");

MODULE_AUTHOR("farsight");
MODULE_DESCRIPTION("A led driver.");
MODULE_ALIAS("led driver.");

