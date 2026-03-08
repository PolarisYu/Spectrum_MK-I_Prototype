#include "PDO.h"
#include "stdio.h"
#include "SEGGER_RTT.h"
#include "app_main.h"

ch224q_handle_t ch224q;
uint8_t Voltage[] = {5,9,12,15,20,28};

void Ch224qInit(void)
{
    ch224q = ch224q_create(CM_I2C3, CH224Q_ADDR);
}
void Ch224qSetVolt(ch224q_voltage_t Voltage)
{
    ch224q_set_voltage(ch224q, Voltage);
}

void Ch224qGetPDO(PDO_info_t *pdo_info)
{
    PDO_GET(ch224q, pdo_info);
}

void Ch224qGetStatus(PDO_info_t *pdo_info)
{
    uint8_t status;
    ch224q_get_status_register(ch224q, &status);
    
    status = status & 0x0F;

    if((status == (0x01 << 3)) || (status == (0x01 << 4))){
        pdo_info->status = PD;
    }else if ((status == ((0x01 << 1) + 1)) || (status == ((0x01 << 2) + 1))){
        pdo_info->status = QC;
    }else{
        pdo_info->status = NONE;
    }
}

static void pdo_analysis(PDO_info_t *pdo_info)
{
    if (pdo_info->messageHeader.MessageHeader.NumberOfDataObjects == 0) {
        return;
    }

    uint8_t *pdo_data = &pdo_info->PDO[2];
    uint8_t pdo_count = pdo_info->messageHeader.MessageHeader.NumberOfDataObjects;

    USBPD_SourcePDO_t pdo_parse;
    for (uint8_t i = 0; i < pdo_count; i++) {
        pdo_parse.d32 = *(uint32_t *)(&pdo_data[i * 4]);

        // 首先检查 PDO 是否为空
        if (pdo_parse.d32 == 0) {
            SEGGER_RTT_printf(0, "Empty\n");
            continue;
        }

        switch (pdo_parse.General.PDO_Type) {
            case PDO_TYPE_FIXED_SUPPLY: {
                uint16_t voltage = POWER_DECODE_50MV(pdo_parse.SourceFixedSupplyPDO.VoltageIn50mVunits);
                uint16_t current = POWER_DECODE_10MA(pdo_parse.SourceFixedSupplyPDO.MaxCurrentIn10mAunits);

                SEGGER_RTT_printf(0, "%s Fixed: %dmV, %dmA\n", voltage <= 20000 ? "SPR" : "EPR", voltage, current);

                sprintf(pdo_info->result[i], "[%d]:Fixed:%dV,%.2fA", i, voltage/1000, current/1000.0f);
                if ((voltage / 1000) == Voltage[Iron.ch224q_volt]) {
                    Iron.pdo_info.curPwrLvl = current * voltage / (1000.0f * 1000.0f);
                }
                break;
            }
            case PDO_TYPE_APDO: {
                switch (pdo_parse.General.SubTypeOrOtherUsage) {
                    case APDO_TYPE_SPR_PPS: {
                        uint16_t min_voltage = POWER_DECODE_100MV(pdo_parse.SourceSPRProgrammablePowerAPDO.MinVoltageIn100mVunits);
                        uint16_t max_voltage = POWER_DECODE_100MV(pdo_parse.SourceSPRProgrammablePowerAPDO.MaxVoltageIn100mVunits);
                        uint16_t current     = POWER_DECODE_50MA(pdo_parse.SourceSPRProgrammablePowerAPDO.MaxCurrentIn50mAunits);
                        SEGGER_RTT_printf(0, "SPR PPS: %d-%dmV, %dmA\n", min_voltage, max_voltage, current);
                        sprintf(pdo_info->result[i], "[%d]:PPS:%d-%dV,%.2fA", i,  min_voltage / 1000, max_voltage / 1000, current / 1000.0f);
                        break;
                    }
                }
                break;
            }
        }
    }
}
void PDO_GET(ch224q_handle_t ch224q_handle, PDO_info_t *pdo_info)
{
    ch224q_get_pdo_data(ch224q_handle, pdo_info->PDO, 48);
    pdo_info->messageHeader.d16 = *(uint16_t *)(pdo_info->PDO);

    // pdo_analyse(&pdo_info->PDO[2],pdo_info->messageHeader.MessageHeader.NumberOfDataObjects);
    pdo_analysis(pdo_info);
}
