 /* ʵ��Ŀ��:
 * �ַ��豸���
 *
 * ʵ�ֲ���:
 * 1. �����ַ��豸
 *    1.1 ����
 *        �̳�cdev, �����Լ�����
 *
 *    1.2 ����
 *        file_operations
 *           
 * 2. �����ַ��豸
 *    2.1 �����ڴ�
 *        ���Դ�ŵ��ڴ�: ���䵽ֱ�ӵ�ַӳ������kmalloc����
 *        file_operations����: ��̬��
 *
 *    2.2 ��ʼ��
 *        ���Գ�ʼ����probe������
 *        file_operations���� ֱ�Ӷ���ʱ��ʼ��
 *
 * 3. �����ַ��豸���ں�
 *    3.1 �����豸��(���Ŀ����豸��: )
 *        register_chrdev_region/unregister_chrdev_region
 *        
 *    3.2 ����
 *        cdev_add/cdev_del
 *    
 *
 * ���Բ���:
 * 1. ����
 *    $ make
 *
 * 2. ������������
 *    $ cp led.ko /source/rootfs/lib/modules/2.6.35/
 *
 * 3. ����ģ��
 *    # insmod /lib/modules/2.6.35/led.ko
 *
 * 4. ȷ���豸��ע��ɹ�
 *    # cat /proc/devices
 *
 * 5. �����豸�ļ�
 *    # mknod /dev/led0 c 250 0
 *    # mknod /dev/led1 c 250 1
 *
 * 6. ȷ���ַ��豸�����ɹ�
 *    5.1 ����Ӧ�ó���
 *        $ arm-cortex_a8-linux-gnueabi-gcc led_app.c -o led_app
 *
 *    5.2 ������������
 *        cp led_app /source/rootfs/root/
 *
 *    5.3 ����, �۲�����
 *        # /root/led_app
 *
 * 7. ж��ģ��
 *    # rmmod led
 *
 * 8. ȷ���ַ�ɾ���ɹ�
 *    ���õ�6����Ӧ�ó���
 *
 * 9. ȷ���豸��ע���ɹ�
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
// 1.1 ����
struct led_cdev{
	struct cdev cdev;
	
	int on;                     // 0-turn off  1-turn on

    struct resource *res;  //�����õ���Դ(������ַ��)
    void __iomem *regs;   //�����豸��õ������ַ
};

/********************************************************************/
//����һ��ȫ����
struct class *led_class;

//�����Ų����ı���
int led_major = 250;

//����ģ�����
module_param(led_major, int, 0400);

//���Ӳ���������Ϣ
MODULE_PARM_DESC(led_major, "led device major");

/********************************************************************/

/********************LED ����****************************************/
void led_dev_init(struct led_cdev *led)
{
    int pos = (MINOR(led->cdev.dev) + 1) << 2;
    unsigned long LED_CON;
    unsigned long LED_DAT;

    //gpio ctrl configuration
    LED_CON = readl(led->regs);
    LED_CON &= ~(0xf << pos);
    LED_CON |= (0x1 << pos);
    writel(LED_CON, led->regs);
    //gpio data

    LED_DAT = readl(led->regs + 4);
    LED_DAT &= ~(0x1 << (MINOR(led->cdev.dev) + 1));
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

// 1.2 ����
/*
 * fd = open("/dev/led0", O_RDWR);
 * @brief						��openϵͳ���ü��(vfsֱ��)����
 * @param[in]				inode													δ�򿪵��ļ��ṹ��ָ��
 * @param[in | out] filp													�򿪵��ļ��ṹָ��
 * @return					@li	0													�ɹ�
 *									@li < 0												������
 * @notes						����ֵ�����ظ�vfs��vfs���ظ�Ӧ�ó���
 *                  @li 0                         vfs�����ļ���������Ӧ�ó���
 *                  @li < 0												vfs�����������errno�����ҷ���-1��Ӧ�ó���
 */
int led_open(struct inode *inode, struct file *filp)
{
    /*********************************************************/
    struct led_cdev *led = container_of(inode->i_cdev, struct led_cdev, cdev);
    led_dev_on(led);
    /*********************************************************/
	printk("%s\n", __func__);
	
	return 0;
}

/*
 * close(fd);
 * @brief						��closeϵͳ���ü��(vfsֱ��)����
 * @param[in]				inode													δ�򿪵��ļ��ṹ��ָ��
 * @param[in | out] filp													�򿪵��ļ��ṹָ��
 * @return					@li	0													�ɹ�
 *									@li < 0												������
 * @notes						����ֵ�����ظ�vfs��vfs���ظ�Ӧ�ó���
 *                  @li 0                         vfs�����ļ���������Ӧ�ó���
 *                  @li < 0												vfs�����������errno�����ҷ���-1��Ӧ�ó���
 */
int led_release(struct inode *inode, struct file *filp)
{
    struct led_cdev *led = container_of(inode->i_cdev, struct led_cdev, cdev);

    led_dev_off(led);	
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

// 2.1 �����ڴ�
/*
 * @brief 			�����з����ڴ�
 * @param[in]		size												�����ڴ��ֽ���
 * @param[in]		flags												�����ڴ��׼(���Ʒ����ڴ����Ϊ)
 *							GFP_ATOMIC									�����ڴ��ڼ䲻������
 *							GFP_KERNEL									�����ڴ��ڼ���ܻ�����
 *              GFP_DMA											������ڴ��DMA��
 * @return	  	�����ڴ�ĵ�ַ(�Ѿ�ӳ�������ڴ�������ַ)
 *              NULL												����ʧ��
 * @notes			  ������ڴ��Ӧ�������ڴ�����
 *              ������ڴ�����32�ֽ�Ϊ��λ����������Ϊ128k
 * void *kmalloc(size_t size, gfp_t flags);
 */
	led = (struct led_cdev *)kmalloc(sizeof(struct led_cdev), GFP_KERNEL);
	if (NULL == led){
		ret = -ENOMEM;
		goto exit;
	}
	
/*
 * @brief 			���������ݷ���ƽ̨�豸
 * @param[out]	pdev												ƽ̨�豸ָ��
 * @param[in]		drvdata											��������
 * void platform_set_drvdata(struct platform_device *pdev, void *drvdata);
 */
	platform_set_drvdata(pdev, led);
	
    /******************************************************************/


    // ��ȡ�Ĵ�����Դ(������ַ��)
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (res == NULL){
        ret = -ENOENT;
        goto err_platform_get_resource;
    }
    // ������Դ
    led->res = res;


    // �����ڴ�ӳ��
    led->regs = ioremap(res->start, resource_size(res));
    if (led->regs == NULL){
        ret = -EINVAL;
        goto err_ioremap;
    }


    
    /******************************************************************/

	// 2.2 ��ʼ��
/*
 * @brief 			��ʼ���ַ��豸
 * @param[out]	cdev												�ַ��豸ָ��
 * @param[in]		fops												��������ָ��
 * @notes				������û��.owner������Ҫ�ں������.owner = THIS_MODULE
 * void cdev_init(struct cdev *cdev, const struct file_operations *fops)
 */
	cdev_init(&led->cdev, &fops);
	led->cdev.owner = THIS_MODULE;
	
	
	
	// 3.1 �����豸��(˼��: ����ʧ�ܣ���Ҫ��ʲô���飿)
/*
 * @brief						�����豸���
 * @param[in]				first												��ʼ�豸���
 * @param[in]				count												����������
 * @param[in]				name												�豸����(��/proc/devices�ļ��пɼ�) 
 * @return					=0													����ɹ�
 * 									<0													������
 * int register_chrdev_region(dev_t first, unsigned int count, char *name);
 */
	ret = register_chrdev_region(devno, 1, "led");
	if (ret < 0){
		goto err_register_chrdev_region;
	}
	
	// 3.2 ����
/*
 * @brief						����һ���ַ��豸��ϵͳ
 * @param[in]				p														�����ַ��豸��struct cdev�ṹ
 * @param[in]				dev													����������豸�ĵ�һ���豸��
 * @param[in]				count												����������豸���豸�Ÿ���
 * @return					0														���ӳɹ�
 *									<0													������
 * @notes						����ִ�гɹ�����������ʹ�ø��ַ��豸��Ҳ����˵�������֮ǰ�豸�����Ѿ���ȫ����
 * int cdev_add(struct cdev *p, dev_t dev, unsigned count);
 */
	ret = cdev_add(&led->cdev, devno, 1);
	if (ret < 0){
		goto err_cdev_add;
	}

    //�����豸��ע�ᵽsysfs��
   device = device_create(led_class, NULL, devno, NULL, "led%d", pdev->id);
   if (IS_ERR(device)){
       ret = PTR_ERR(device);
       goto err_device_create;
   }
	
   /************************led_init*******************************/
    led_dev_init(led);
   /************************led_init*******************************/
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
    struct resource *res;
	printk("%s\n", __func__);

	
/*
 * @brief 			��ƽ̨�豸�����������
 * @param[in]		pdev												ƽ̨�豸ָ��
 * @return			��������
 * 	void *platform_get_drvdata(const struct platform_device *pdev);
 */
	led = platform_get_drvdata(pdev);

//�豸����
    device_destroy(led_class, led->cdev.dev);
	
/*
 * @brief						ɾ��һ���ַ��豸
 * @param[in]				cdev												�����ַ��豸��struct cdev�ṹ
 * void cdev_del(struct cdev *dev);
 */
	cdev_del(&led->cdev);

/*
 * @brief						�����豸���
 * @param[in]				first												��һ���α��
 * @param[in]				count												����������
 * void unregister_chrdev_region(dev_t first, unsigned int count);
 */
	unregister_chrdev_region(led->cdev.dev, 1);

    //ȡ��ӳ��
    iounmap(led->regs);
    //�ͷ�io�ڴ�
    res = led->res;
    release_mem_region(res->start, resource_size(res));

/*
 * @brief 			�ͷ��ڴ�
 * @param[in]		addr													�ڴ�ĵ�ַ
 * void kfree(const void *addr);
 */


	kfree(led);
    
	return 0;
}

// �豸�б�(������������豸ʱ)
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
	
	// ֻ����һ���豸
	// .id_table = NULL,
	
	// ������������豸ʱ
	.id_table = led_ids,
	.driver = {
    // �����������豸�б���������ʱ�����������ƥ���豸
		.name = "led",
		.owner = THIS_MODULE,
		
		// �豸�б�(���豸���е��豸�б�����ƥ��)
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
