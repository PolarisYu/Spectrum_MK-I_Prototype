/*
 * USB CDC ACM with CherryRingBuffer - Header File
 * 
 * Copyright (c) 2024
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CDC_ACM_RINGBUFFER_H
#define CDC_ACM_RINGBUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*****************************************************************************
 * 初始化函数
 *****************************************************************************/

/**
 * @brief 初始化USB CDC和环形缓冲区
 * 
 * @param busid USB总线ID（通常为0）
 * @param reg_base USB寄存器基地址
 * 
 * @note 应在系统启动时调用一次
 * 
 * @example
 *   cdc_acm_init(0, USB_BASE);
 */
void cdc_acm_init(uint8_t busid, uintptr_t reg_base);

/*****************************************************************************
 * 基础收发API
 *****************************************************************************/

/**
 * @brief 发送数据到USB CDC
 * 
 * @param busid USB总线ID
 * @param data 要发送的数据指针
 * @param len 数据长度（字节）
 * 
 * @return 实际写入发送缓冲区的字节数
 *         如果返回值 < len，说明发送缓冲区空间不足
 * 
 * @note 数据先写入发送环形缓冲区，然后自动触发USB发送
 * 
 * @example
 *   const char *msg = "Hello USB!\r\n";
 *   int sent = cdc_acm_send_data(0, (uint8_t *)msg, strlen(msg));
 *   if (sent < strlen(msg)) {
 *       printf("发送缓冲区满\n");
 *   }
 */
int cdc_acm_send_data(uint8_t busid, const uint8_t *data, uint32_t len);

/**
 * @brief 从USB CDC接收数据
 * 
 * @param buffer 接收缓冲区指针
 * @param max_len 缓冲区最大长度（字节）
 * 
 * @return 实际读取的字节数，0表示无数据
 * 
 * @note 此函数会从接收环形缓冲区移除读取的数据
 * 
 * @example
 *   uint8_t rx_buf[256];
 *   int len = cdc_acm_read_data(rx_buf, sizeof(rx_buf));
 *   if (len > 0) {
 *       process_data(rx_buf, len);
 *   }
 */
int cdc_acm_read_data(uint8_t *buffer, uint32_t max_len);

/**
 * @brief 查看接收数据但不移除
 * 
 * @param buffer 接收缓冲区指针
 * @param max_len 缓冲区最大长度（字节）
 * 
 * @return 实际查看的字节数，0表示无数据
 * 
 * @note 此函数不会从环形缓冲区移除数据，适合先检查协议头
 * 
 * @example
 *   uint8_t header[4];
 *   if (cdc_acm_peek_data(header, 4) == 4) {
 *       if (is_valid_header(header)) {
 *           // 确认有效后再读取完整数据
 *           cdc_acm_read_data(full_packet, packet_size);
 *       }
 *   }
 */
int cdc_acm_peek_data(uint8_t *buffer, uint32_t max_len);

/*****************************************************************************
 * 缓冲区状态查询API
 *****************************************************************************/

/**
 * @brief 获取接收缓冲区可用数据量
 * 
 * @return 接收缓冲区中可读取的字节数
 * 
 * @example
 *   if (cdc_acm_get_rx_available() >= 10) {
 *       // 至少有10字节可读
 *       uint8_t buf[10];
 *       cdc_acm_read_data(buf, 10);
 *   }
 */
uint32_t cdc_acm_get_rx_available(void);

/**
 * @brief 获取发送缓冲区剩余空间
 * 
 * @return 发送缓冲区剩余可写入的字节数
 * 
 * @example
 *   uint32_t free = cdc_acm_get_tx_free();
 *   if (free < 1024) {
 *       printf("警告：发送缓冲区仅剩 %lu 字节\n", free);
 *   }
 */
uint32_t cdc_acm_get_tx_free(void);

/**
 * @brief 检查接收缓冲区是否为空
 * 
 * @return true: 接收缓冲区为空, false: 有数据可读
 * 
 * @example
 *   if (!cdc_acm_is_rx_empty()) {
 *       // 有数据可读
 *       process_received_data();
 *   }
 */
bool cdc_acm_is_rx_empty(void);

/**
 * @brief 检查发送缓冲区是否已满
 * 
 * @return true: 发送缓冲区已满, false: 还有空间
 * 
 * @example
 *   if (cdc_acm_is_tx_full()) {
 *       printf("发送缓冲区已满，暂停数据生成\n");
 *   }
 */
bool cdc_acm_is_tx_full(void);

/*****************************************************************************
 * 缓冲区管理API
 *****************************************************************************/

/**
 * @brief 清空接收缓冲区
 * 
 * @note 会丢弃所有未读取的接收数据
 * 
 * @example
 *   // 切换通信协议前清空旧数据
 *   cdc_acm_flush_rx();
 *   switch_to_new_protocol();
 */
void cdc_acm_flush_rx(void);

/**
 * @brief 清空发送缓冲区
 * 
 * @note 会丢弃所有未发送的数据
 * 
 * @example
 *   // 发生错误时清空待发送数据
 *   cdc_acm_flush_tx();
 */
void cdc_acm_flush_tx(void);

/**
 * @brief 丢弃指定字节的接收数据
 * 
 * @param size 要丢弃的字节数
 * 
 * @return 实际丢弃的字节数
 * 
 * @note 适合处理协议同步，丢弃无效数据
 * 
 * @example
 *   // 查找协议帧头
 *   uint8_t byte;
 *   while (cdc_acm_peek_data(&byte, 1) == 1) {
 *       if (byte == 0xAA) break;  // 找到帧头
 *       cdc_acm_drop_rx(1);       // 丢弃无效字节
 *   }
 */
uint32_t cdc_acm_drop_rx(uint32_t size);

/*****************************************************************************
 * 发送控制API
 *****************************************************************************/

/**
 * @brief 尝试发送缓冲区中的数据
 * 
 * @param busid USB总线ID
 * 
 * @note ⚠️ 非常重要：必须在主循环中定期调用此函数！
 *       此函数检查发送条件（DTR使能、非busy），并启动实际的USB传输
 * 
 * @example
 *   while (1) {
 *       // 业务逻辑
 *       process_tasks();
 *       
 *       // 必须定期调用，确保数据及时发送
 *       cdc_acm_try_send(0);
 *       
 *       delay_ms(1);
 *   }
 */
void cdc_acm_try_send(uint8_t busid);

/*****************************************************************************
 * 高级API - DMA零拷贝支持
 *****************************************************************************/

/**
 * @brief 设置线性读取，获取接收缓冲区连续内存指针
 * 
 * @param size 输出参数，返回可连续读取的最大字节数
 * 
 * @return 接收缓冲区的内存指针，可直接访问或用于DMA传输
 * 
 * @note 此函数返回环形缓冲区内部指针，避免数据拷贝，适合高性能场景
 *       读取完成后必须调用 cdc_acm_linear_read_done() 更新读指针
 * 
 * @example
 *   uint32_t size;
 *   uint8_t *ptr = cdc_acm_linear_read_setup(&size);
 *   if (size > 0) {
 *       // 方法1: CPU直接处理
 *       process_data_inplace(ptr, size);
 *       cdc_acm_linear_read_done(size);
 *       
 *       // 方法2: DMA传输
 *       dma_start(ptr, dst_buffer, size);
 *       // DMA完成中断中调用: cdc_acm_linear_read_done(size);
 *   }
 */
void *cdc_acm_linear_read_setup(uint32_t *size);

/**
 * @brief 完成线性读取，更新读指针
 * 
 * @param size 实际读取的字节数
 * 
 * @note 必须在 cdc_acm_linear_read_setup() 之后调用
 */
void cdc_acm_linear_read_done(uint32_t size);

/**
 * @brief 设置线性写入，获取发送缓冲区连续内存指针
 * 
 * @param size 输出参数，返回可连续写入的最大字节数
 * 
 * @return 发送缓冲区的内存指针，可直接写入或用于DMA传输
 * 
 * @note 此函数返回环形缓冲区内部指针，避免数据拷贝
 *       写入完成后必须调用 cdc_acm_linear_write_done() 更新写指针并触发发送
 * 
 * @example
 *   uint32_t size;
 *   uint8_t *ptr = cdc_acm_linear_write_setup(&size);
 *   if (size >= 100) {
 *       // 方法1: CPU直接写入
 *       memcpy(ptr, my_data, 100);
 *       cdc_acm_linear_write_done(0, 100);
 *       
 *       // 方法2: DMA传输
 *       dma_start(src_buffer, ptr, 100);
 *       // DMA完成中断中调用: cdc_acm_linear_write_done(0, 100);
 *   }
 */
void *cdc_acm_linear_write_setup(uint32_t *size);

/**
 * @brief 完成线性写入，更新写指针并触发发送
 * 
 * @param busid USB总线ID
 * @param size 实际写入的字节数
 * 
 * @note 必须在 cdc_acm_linear_write_setup() 之后调用
 *       此函数会自动触发 cdc_acm_try_send()
 */
void cdc_acm_linear_write_done(uint8_t busid, uint32_t size);

/*****************************************************************************
 * USB CDC回调函数（由CherryUSB协议栈调用，用户无需直接调用）
 *****************************************************************************/

/**
 * @brief USB批量输出端点回调（主机->设备）
 * @note 此函数由USB协议栈自动调用，用户代码无需调用
 */
void usbd_cdc_acm_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes);

/**
 * @brief USB批量输入端点回调（设备->主机）
 * @note 此函数由USB协议栈自动调用，用户代码无需调用
 */
void usbd_cdc_acm_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes);

/**
 * @brief USB DTR控制回调
 * @note 此函数由USB协议栈自动调用，用户代码无需调用
 */
void usbd_cdc_acm_set_dtr(uint8_t busid, uint8_t intf, bool dtr);

/*****************************************************************************
 * 配置参数说明
 *****************************************************************************/

/*
 * 缓冲区大小配置（在.c文件中定义）：
 * 
 * #define CDC_RX_RINGBUF_SIZE  (4096)  // 接收环形缓冲区大小
 * #define CDC_TX_RINGBUF_SIZE  (4096)  // 发送环形缓冲区大小
 * #define CDC_USB_READ_SIZE    (2048)  // USB单次读取大小
 * 
 * ⚠️ 重要：缓冲区大小必须是2的幂次方（512, 1024, 2048, 4096, 8192...）
 * 
 * 推荐配置：
 * - 低速应用（命令行）：   RX=1024,  TX=1024
 * - 中速应用（日志输出）： RX=2048,  TX=4096
 * - 高速应用（数据传输）： RX=8192,  TX=8192
 */

/*****************************************************************************
 * 使用示例
 *****************************************************************************/

#if 0  // 示例代码，不会被编译

/* 示例1：基本初始化和主循环 */
int main(void)
{
    uint8_t busid = 0;
    
    // 系统初始化
    system_init();
    
    // USB CDC初始化
    cdc_acm_init(busid, USB_BASE);
    
    while (1) {
        // 处理接收数据
        if (!cdc_acm_is_rx_empty()) {
            uint8_t buf[256];
            int len = cdc_acm_read_data(buf, sizeof(buf));
            if (len > 0) {
                // 处理数据（例如回显）
                cdc_acm_send_data(busid, buf, len);
            }
        }
        
        // ⚠️ 重要：必须定期调用
        cdc_acm_try_send(busid);
        
        delay_ms(1);
    }
}

/* 示例2：发送字符串 */
void send_message(void)
{
    const char *msg = "Hello USB CDC!\r\n";
    cdc_acm_send_data(0, (uint8_t *)msg, strlen(msg));
}

/* 示例3：按行读取命令 */
void command_handler(void)
{
    static char cmd_buf[128];
    static uint8_t cmd_pos = 0;
    uint8_t byte;
    
    while (cdc_acm_read_data(&byte, 1) == 1) {
        if (byte == '\r' || byte == '\n') {
            if (cmd_pos > 0) {
                cmd_buf[cmd_pos] = '\0';
                execute_command(cmd_buf);
                cmd_pos = 0;
            }
        } else if (cmd_pos < sizeof(cmd_buf) - 1) {
            cmd_buf[cmd_pos++] = byte;
        }
    }
}

/* 示例4：检查缓冲区状态 */
void check_buffer_status(void)
{
    uint32_t rx_avail = cdc_acm_get_rx_available();
    uint32_t tx_free = cdc_acm_get_tx_free();
    
    printf("RX available: %lu bytes\n", rx_avail);
    printf("TX free: %lu bytes\n", tx_free);
    
    if (cdc_acm_is_tx_full()) {
        printf("Warning: TX buffer full!\n");
    }
}

#endif  // 示例代码结束

#ifdef __cplusplus
}
#endif

#endif /* CDC_ACM_RINGBUFFER_H */