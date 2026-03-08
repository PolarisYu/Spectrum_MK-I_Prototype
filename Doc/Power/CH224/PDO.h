#ifndef PDO_H
#define PDO_H

#include "ch224q.h"

typedef struct
{
    uint8_t PDO[48];
    USBPD_MessageHeader_t messageHeader;
    USBPD_ExtendedMessageHeader_t extHeader;
    char result[CH224Q_AVS_ENABLE + 1][25];
    uint8_t curPwrLvl;  // 当前挡位功率
    uint8_t status;     // 协议握手状态
} PDO_info_t;

typedef enum
{
    NONE,
    PD,
    QC,
} ch224q_status_t;

void PDO_GET(ch224q_handle_t ch224q_handle, PDO_info_t *pdo_info);
void Ch224qInit(void);
void Ch224qSetVolt(ch224q_voltage_t Voltage);
void Ch224qGetPDO(PDO_info_t *pdo_info);
void Ch224qGetStatus(PDO_info_t *pdo_info);
#endif
