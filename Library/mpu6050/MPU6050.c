/*
 * MPU6050.c
 *
 *  Created on: Oct 26, 2021
 *      Author: Administrator
 */

#include "MPU6050.h"
#include "../../Library/SoftwareI2C/softwarei2c.h"
#include "dmp/inv_mpu.h"
#include "dmp/inv_mpu_dmp_motion_driver.h"
#include <stdio.h>
#include <math.h>   /* 增加数学库支持 */

// ---------------- DMP 相关的方向转换矩阵 ----------------
static signed char gyro_orientation[9] = { 1, 0, 0,
                                           0, 1, 0,
                                           0, 0, 1 };

// ---------------- 内部工具函数 ----------------
static unsigned short inv_row_2_scale(const signed char *row)
{
    unsigned short b;

    if (row[0] > 0) b = 0;
    else if (row[0] < 0) b = 4;
    else if (row[1] > 0) b = 1;
    else if (row[1] < 0) b = 5;
    else if (row[2] > 0) b = 2;
    else if (row[2] < 0) b = 6;
    else b = 7;      // error
    return b;
}

static unsigned short inv_orientation_matrix_to_scalar(const signed char *mtx)
{
    unsigned short scalar;
    scalar = inv_row_2_scale(mtx);
    scalar |= inv_row_2_scale(mtx + 3) << 3;
    scalar |= inv_row_2_scale(mtx + 6) << 6;
    return scalar;
}

// ---------------- DMP 初始化封装函数 ----------------
// 返回值: 0,成功; 其他,失败
u8 mpu_dmp_init(void)
{
    struct int_param_s int_param; 

    // 1. 底层初始化
    if (mpu_init(&int_param) != 0) return 1;

    // 2. 设置传感器 (加速度计+陀螺仪)
    if (mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL) != 0) return 2;

    // 3. 设置 FIFO
    if (mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL) != 0) return 3;

    // 4. 设置采样率 (默认 100Hz)
    if (mpu_set_sample_rate(20) != 0) return 4;

    // 5. 加载 DMP 固件
    if (dmp_load_motion_driver_firmware() != 0) return 5;

    // 6. 设置方向矩阵
    if (dmp_set_orientation(inv_orientation_matrix_to_scalar(gyro_orientation)) != 0) return 6;

    // 7. 设置 FIFO 功能
    if (dmp_enable_feature(DMP_FEATURE_6X_LP_QUAT | DMP_FEATURE_TAP |
                           DMP_FEATURE_ANDROID_ORIENT | DMP_FEATURE_SEND_RAW_ACCEL |
                           DMP_FEATURE_SEND_CAL_GYRO | DMP_FEATURE_GYRO_CAL) != 0) return 7;

    // 8. 设置 FIFO 刷新率
    if (dmp_set_fifo_rate(20) != 0) return 8;

    // 9. 启动 DMP
    if (mpu_set_dmp_state(1) != 0) return 9;

    return 0;
}

// ---------------- 读取 DMP 解算结果 ----------------
// 返回值: 0,成功; 其他,失败
u8 mpu_dmp_get_data(float *pitch, float *roll, float *yaw)
{
    float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;
    unsigned long sensor_timestamp;
    short gyro[3], accel[3], sensors;
    unsigned char more;
    long quat[4];

    if (dmp_read_fifo(gyro, accel, quat, &sensor_timestamp, &sensors, &more)) return 1;

    if (sensors & INV_WXYZ_QUAT)
    {
        q0 = quat[0] / 1073741824.0f;
        q1 = quat[1] / 1073741824.0f;
        q2 = quat[2] / 1073741824.0f;
        q3 = quat[3] / 1073741824.0f;

        // 计算欧拉角 (需要 math.h)
        // 修正：为 Pitch 和 Roll 添加负号，解决与实物动作对称的问题
        *pitch = -asin(-2 * q1 * q3 + 2 * q0 * q2) * 57.3f; // pitch (Inverted)
        *roll = -atan2(2 * q2 * q3 + 2 * q0 * q1, -2 * q1 * q1 - 2 * q2 * q2 + 1) * 57.3f; // roll (Inverted)
        *yaw = atan2(2 * (q1 * q2 + q0 * q3), q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3) * 57.3f; // yaw
    }

    return 0;
}

u8 IIC_Write_Byte(u8 reg, u8 value)
{
    return IIC_Write_Len(MPU_ADDR, reg, 1, &value);
}

u8 IIC_Read_Byte(u8 reg)
{
    u8 value = 0;
    IIC_Read_Len(MPU_ADDR, reg, 1, &value);
    return value;
}

// IIC连续写 (适配软件I2C)
u8 IIC_Write_Len(u8 addr, u8 reg, u8 len, u8 *buf)
{
    SoftwareI2C_Start();
    SoftwareI2C_SendByte((addr << 1) | 0); 
    if (SoftwareI2C_WaitACK()) {
        SoftwareI2C_Stop();
        return 1;
    }

    SoftwareI2C_SendByte(reg);
    SoftwareI2C_WaitACK();

    for (u8 i = 0; i < len; i++) {
        SoftwareI2C_SendByte(buf[i]);
        if (SoftwareI2C_WaitACK()) {
            SoftwareI2C_Stop();
            return 1;
        }
    }
    SoftwareI2C_Stop();
    return 0;
}

// IIC连续读 (适配软件I2C)
u8 IIC_Read_Len(u8 addr, u8 reg, u8 len, u8 *buf)
{
    SoftwareI2C_Start();
    SoftwareI2C_SendByte((addr << 1) | 0);
    if (SoftwareI2C_WaitACK()) {
        SoftwareI2C_Stop();
        return 1;
    }

    SoftwareI2C_SendByte(reg);
    SoftwareI2C_WaitACK();

    SoftwareI2C_Start(); 
    SoftwareI2C_SendByte((addr << 1) | 1);
    if (SoftwareI2C_WaitACK()) {
        SoftwareI2C_Stop();
        return 1;
    }

    for (u8 i = 0; i < len; i++) {
        buf[i] = SoftwareI2C_ReceiveByte();
        if (i < len - 1) SoftwareI2C_SendACK(0); 
        else SoftwareI2C_SendACK(1); 
    }
    SoftwareI2C_Stop();
    return 0;
}

//初始化MPU6050
u8 MPU_Init(void)
{
    u8 res = 0;
    
    SoftwareI2C_Init(); 
    SoftwareI2C_SetDelay(I2C_DELAY_400K); 

    IIC_Write_Byte(MPU_PWR_MGMT1_REG, 0X80); 
    HAL_Delay(100);
    IIC_Write_Byte(MPU_PWR_MGMT1_REG, 0X00); 

    MPU_Set_Gyro_Fsr(3);  
    MPU_Set_Accel_Fsr(0); 
    MPU_Set_Rate(50);     
    IIC_Write_Byte(MPU_INT_EN_REG, 0X00);   
    IIC_Write_Byte(MPU_USER_CTRL_REG, 0X00); 
    IIC_Write_Byte(MPU_FIFO_EN_REG, 0X00);   
    IIC_Write_Byte(MPU_INTBP_CFG_REG, 0X80); 

    res = IIC_Read_Byte(MPU_DEVICE_ID_REG);
    printf("MPU6050 ID: 0x%02X\r\n", res);

    if (res == MPU_ADDR) 
    {
        IIC_Write_Byte(MPU_PWR_MGMT1_REG, 0X01); 
        IIC_Write_Byte(MPU_PWR_MGMT2_REG, 0X00); 
        MPU_Set_Rate(50); 
        return 0;
    }
    else
    {
        return 1;
    }
}

u8 MPU_Set_Gyro_Fsr(u8 fsr)
{
    return IIC_Write_Byte(MPU_GYRO_CFG_REG, fsr << 3); 
}

u8 MPU_Set_Accel_Fsr(u8 fsr)
{
    return IIC_Write_Byte(MPU_ACCEL_CFG_REG, fsr << 3); 
}

u8 MPU_Set_LPF(u16 lpf)
{
    u8 data = 0;
    if (lpf >= 188) data = 1;
    else if (lpf >= 98) data = 2;
    else if (lpf >= 42) data = 3;
    else if (lpf >= 20) data = 4;
    else if (lpf >= 10) data = 5;
    else data = 6;
    return IIC_Write_Byte(MPU_CFG_REG, data); 
}

//设置MPU6050的采样率
u8 MPU_Set_Rate(u16 rate)
{
    u8 data;
    if (rate > 1000) rate = 1000;
    if (rate < 4) rate = 4;
    data = 1000 / rate - 1;
    /* 修正宏定义名称: MPU_SMPLRT_DIV_REG -> MPU_SAMPLE_RATE_REG */
    data = IIC_Write_Byte(MPU_SAMPLE_RATE_REG, data); 
    return MPU_Set_LPF(rate / 2); 
}

short MPU_Get_Temperature(void)
{
    u8 buf[2];
    short raw;
    float temp;
    IIC_Read_Len(MPU_ADDR, MPU_TEMP_OUTH_REG, 2, buf);
    raw = ((u16)buf[0] << 8) | buf[1];
    temp = 36.53 + ((double)raw) / 340;
    return (short)(temp * 100);
}

u8 MPU_Get_Gyroscope(short *gx, short *gy, short *gz)
{
    u8 buf[6], res;
    res = IIC_Read_Len(MPU_ADDR, MPU_GYRO_XOUTH_REG, 6, buf);
    if (res == 0)
    {
        *gx = ((u16)buf[0] << 8) | buf[1];
        *gy = ((u16)buf[2] << 8) | buf[3];
        *gz = ((u16)buf[4] << 8) | buf[5];
    }
    return res;
}

u8 MPU_Get_Accelerometer(short *ax, short *ay, short *az)
{
    u8 buf[6], res;
    res = IIC_Read_Len(MPU_ADDR, MPU_ACCEL_XOUTH_REG, 6, buf);
    if (res == 0)
    {
        *ax = ((u16)buf[0] << 8) | buf[1];
        *ay = ((u16)buf[2] << 8) | buf[3];
        *az = ((u16)buf[4] << 8) | buf[5];
    }
    return res;
}

/**
 * @brief  获取当前系统毫秒计数 (对接 DMP 驱动)
 * @param  count: 存储计数的指针
 * @return 0: 成功
 */
int get_tick_count(unsigned long *count)
{
    *count = HAL_GetTick();
    return 0;
}
