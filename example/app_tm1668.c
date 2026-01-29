#include "rtconfig.h"
#include <rtthread.h>
#include <rtdevice.h>
#include <stdlib.h>

uint8_t ledMatrix[7][8] = {0};//对应到TM1668的显存


/* Nian-Dong 板级映射表（35 个LED灯） */
static const uint8_t mymap[35] = {
    52, 36, 20,  4, 50, 34,
    18,  2, 48, 32, 16,  0,
    55, 39, 23,  7,  8, 24,
    53, 37, 21,  5, 51, 35,
    19,  3, 49, 33, 17,  1,
    54, 38, 22,  6, 40
};



/**
 * @brief  将8个0/1元素的数组转为单个U8
 * @param  arry：输入数组（数组苛求:长度固定8，元素仅0或1）
 * @return 合并后的U8数据（arry[0]→bit0，arry[1]→bit1，…，arry[7]→bit7）
 * @note   示例：arry[8] = {1,0,1,0,0,0,0,1} → 输出 0x85（二进制10000101）
 */
uint8_t arr8_to_u8(uint8_t *arry) {
    uint8_t result = 0; 
    for(uint8_t i = 0; i < 8; i++) {
        // 若数组当前元素为1，将对应bit置1（i=0→bit0，i=7→bit7）
        if(arry[i] == 1) {
            result |= (1 << i);
        }
    }
    return result;
}


static void tm1668_test(int argc, char *argv[]) {
    if(argc != 2) {
        rt_kprintf("demo_tm1668 <lenNum>\n");
        return;
    }


    rt_memset(ledMatrix, 0, sizeof(ledMatrix));
    int userNum = atoi(argv[1]);
    rt_kprintf("tm1668 board led %d will on \n", userNum);
    int phyNum = mymap[userNum];
    uint8_t *p = &ledMatrix[0][0];
    p[phyNum] = 1;


    rt_device_t tm_dev = rt_device_find("tm1668");
    if(tm_dev == NULL) {
        rt_kprintf("tm1668: device not found!\n");
        return;
    }
    rt_err_t ret = rt_device_open(tm_dev, RT_DEVICE_OFLAG_WRONLY);
    if(ret != RT_EOK) {
        rt_kprintf("tm1668: open device failed! err=%d\n", ret);
        return;
    }
    rt_uint8_t dispBuf[9] = {0};
    dispBuf[0] = arr8_to_u8(&ledMatrix[0][0]);
    dispBuf[1] = arr8_to_u8(&ledMatrix[1][0]);
    dispBuf[2] = arr8_to_u8(&ledMatrix[2][0]);
    dispBuf[3] = arr8_to_u8(&ledMatrix[3][0]);
    dispBuf[4] = arr8_to_u8(&ledMatrix[4][0]);
    dispBuf[5] = arr8_to_u8(&ledMatrix[5][0]);
    dispBuf[6] = arr8_to_u8(&ledMatrix[6][0]);
    dispBuf[7] = 0XAB;
    dispBuf[8] = 0XAB;
    rt_device_write(tm_dev, 0, dispBuf, 9);
}

MSH_CMD_EXPORT(tm1668_test, "tm1668_test")

