/**
 * @file   drv_tm1668.c
 * @brief  RT-Thread driver for TM1668 7-segment LED display
 * @author GKoSon
 * @date   2025-12-22
 * @modify  适配老版本RT-Thread（通过rt_device_ops绑定设备接口）
 */
#include "rtconfig.h"
#include <rtthread.h>
#include <rtdevice.h>
/* -------------------------- 1. 基础定义 -------------------------- */
// 硬件参数
#define TM1668_DEV_NAME    "tm1668"  /* 设备名称 */
#define TM1668_SEG_CNT     9         /* SEG1~SEG9  （共9个） */
#define TM1668_GRID_CNT    4         /* GRID1~GRID4（共4个） */
#define REFRESH_INTERVAL   10        /* 定时器刷新间隔（ms）  */
// 引脚定义
#define TM1668_STB_PIN   rt_pin_get(TM1668_STB_PIN_NAME)
#define TM1668_CLK_PIN   rt_pin_get(TM1668_CLK_PIN_NAME)
#define TM1668_DIO_PIN   rt_pin_get(TM1668_DIO_PIN_NAME)

// TM1668命令
#define CMD_DATA          0x40    /* 数据命令：自动地址+写数据 */
#define CMD_ADDR          0xC0    /* 地址命令：起始地址0x00 */
#define CMD_DISP_ON_MAX   0x8F    /* 显示命令：开显示+最大亮度 */
/* -------------------------- 2. 设备结构体 -------------------------- */
struct tm1668_device {
    struct rt_device parent;              /* 继承RT-Thread设备结构体 */
    rt_uint8_t fb_front[TM1668_SEG_CNT];  /* 应用层缓冲区（用户写入） */
    rt_uint8_t fb_back[TM1668_SEG_CNT];   /* 硬件缓冲区（定时器发送） */
    rt_timer_t refresh_timer;             /* 周期刷新定时器 */
};
static struct tm1668_device *dev = RT_NULL;  /* 全局设备指针 */
/* -------------------------- 3. 底层时序 -------------------------- */
// 写1字节数据到TM1668
static void tm1668_write_byte(uint8_t dat) {
    for(uint8_t i = 0; i < 8; i++) {
        rt_pin_write(TM1668_CLK_PIN, PIN_LOW);
        rt_pin_write(TM1668_DIO_PIN, (dat >> i) & 1);
        rt_hw_us_delay(2);
        rt_pin_write(TM1668_CLK_PIN, PIN_HIGH);
        rt_hw_us_delay(2);
    }
}
// 发送命令/数据到TM1668
static void tm1668_send(uint8_t cmd, const uint8_t *data, uint8_t len) {
    // 起始信号
    rt_pin_write(TM1668_STB_PIN, PIN_LOW);
    rt_hw_us_delay(2);
    // 发命令
    tm1668_write_byte(cmd);
    // 发数据（有数据时）
    if(data && len > 0) {
        for(uint8_t i = 0; i < len; i++) {
            tm1668_write_byte(data[i]);
        }
    }
    // 停止信号
    rt_pin_write(TM1668_STB_PIN, PIN_HIGH);
    rt_hw_us_delay(2);
}
/* -------------------------- 4. 定时器回调（周期刷新） -------------------------- */
// 定时器触发时，同步缓冲区并更新硬件
static void refresh_timer_cb(void *parameter) {
    if(dev == RT_NULL) return;
    if(rt_memcmp(dev->fb_back, dev->fb_front,TM1668_SEG_CNT)==0)return;//无需刷新
    // 1. 同步前端缓冲区到后端（无锁，简单场景可靠）
    rt_memcpy(dev->fb_back, dev->fb_front, TM1668_SEG_CNT);
    // 2. 发送数据到TM1668
    tm1668_send(CMD_DATA, NULL, 0);                      // 写数据命令
    tm1668_send(CMD_ADDR, dev->fb_back, TM1668_SEG_CNT); // 发9字节显示数据
    tm1668_send(CMD_DISP_ON_MAX, NULL, 0);               // 确保显示开启+最大亮度
}
/* -------------------------- 5. 设备接口函数（直接绑定） -------------------------- */
// 设备初始化函数（直接关联到dev->parent.init）
static rt_err_t tm1668_dev_init(rt_device_t device) {
    // 空实现（硬件初始化在tm1668_hw_init中完成）
    return RT_EOK;
}

// 设备写函数（直接关联到dev->parent.write，用户通过此函数更新显示）
static rt_ssize_t tm1668_dev_write(rt_device_t device, rt_off_t pos, const void *buffer, rt_size_t size) {
    if(dev == RT_NULL || buffer == NULL || size != TM1668_SEG_CNT) {
        return 0;
    }
    // 写入应用层缓冲区（fb_front）
    rt_memcpy(dev->fb_front, buffer, TM1668_SEG_CNT);
    return size; 
}

/* -------------------------- 6. 硬件+设备初始化 -------------------------- */
static rt_err_t tm1668_dev_open(rt_device_t device, rt_uint16_t oflag) {
    // 1. 检查设备是否初始化（避免非法访问）
    if(dev == NULL) {
        rt_kprintf("tm1668: device not initialized!\n");
        return -RT_ERROR;
    }
    // 2. 检查设备是否已被打开（避免重复open）
    if(device->open_flag & RT_DEVICE_OFLAG_OPEN) {
        return RT_EOK; // 重复open返回成功（兼容多线程）
    }
    // 3. 标记设备为“已打开”状态
    device->open_flag |= RT_DEVICE_OFLAG_OPEN;

    return RT_EOK;
}
#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops ops =
{
    tm1668_dev_init,
    tm1668_dev_open,
    RT_NULL,
    tm1668_dev_write,
    RT_NULL,
    RT_NULL,
	RT_NULL
};
#endif
static int tm1668_hw_init(void) {
    // 1. 初始化GPIO
    rt_pin_mode(TM1668_STB_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(TM1668_CLK_PIN, PIN_MODE_OUTPUT);
    rt_pin_mode(TM1668_DIO_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(TM1668_STB_PIN, PIN_HIGH); // 初始空闲状态
    // 2. 分配设备内存
    dev = rt_calloc(1, sizeof(struct tm1668_device));
    if(dev == RT_NULL) {
        rt_kprintf("tm1668: alloc memory failed!\n");
        return -RT_ENOMEM;
    }
	// 3. 直接绑定设备接口函数
#ifdef RT_USING_DEVICE_OPS
    dev->parent.ops = &ops; 
#else
    dev->parent.init  = tm1668_dev_init;  // 绑定初始化函数
    dev->parent.open  = tm1668_dev_open;  // 绑定open函数
    dev->parent.write = tm1668_dev_write; // 绑定写函数
#endif
    dev->parent.type  = RT_Device_Class_Miscellaneous; // 设备类型（杂项设备）
    dev->parent.flag = RT_DEVICE_FLAG_WRONLY;         // 设备属性：只写设备（LED显示无需读取）
    dev->parent.ref_count = 0;                        // 引用计数初始化
    // 4. 注册设备到RT-Thread内核
    rt_err_t ret = rt_device_register(&dev->parent, TM1668_DEV_NAME, RT_DEVICE_FLAG_WRONLY);
    if(ret != RT_EOK) {
        rt_kprintf("tm1668: register device failed! err=%d\n", ret);
        rt_free(dev);
        dev = RT_NULL;
        return ret;
    }
    // 5. 创建周期刷新定时器（软定时器，线程上下文执行）
    dev->refresh_timer = rt_timer_create("1668Refresh",     // 定时器名称
                                         refresh_timer_cb,  // 回调函数
                                         RT_NULL,           // 回调参数（无）
                                         rt_tick_from_millisecond(REFRESH_INTERVAL), // 周期
                                         RT_TIMER_FLAG_PERIODIC); // 周期模式
    if(dev->refresh_timer == RT_NULL) {
        rt_kprintf("tm1668: create timer failed!\n");
        rt_device_unregister(&dev->parent);
        rt_free(dev);
        dev = RT_NULL;
        return -RT_ERROR;
    }
    // 6. 初始化缓冲区
    // 6. 初始化显示缓冲区（初始值0x00，所有LED熄灭；改为0xFF可全亮）
    rt_memset(dev->fb_front, 0x00, TM1668_SEG_CNT);
    rt_memset(dev->fb_back, 0x00, TM1668_SEG_CNT);
    // 7. 启动定时器
    rt_timer_start(dev->refresh_timer);
    rt_kprintf("tm1668: init success! \n");
    return RT_EOK;
}
// 设备启动时自动初始化（优先级：设备层初始化，早于应用）
INIT_DEVICE_EXPORT(tm1668_hw_init);

