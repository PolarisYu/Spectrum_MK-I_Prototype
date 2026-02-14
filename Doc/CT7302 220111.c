#include "Global.h"
#include "Mcu.h"
#include "mi2c.h"
#include "UserSetting.h"
#include "gpio.h"

extern bool MI2CWriteByte(int Addr, BYTE Data);
extern BYTE MI2CReadByte(int Addr);


void MI2CWriteByteMask(BYTE addr, BYTE value, BYTE mask)
{
    BYTE temp;
    temp = MI2CReadByte(addr) & (~mask);
    MI2CWriteByte(addr, ( temp | (value & mask)));
}


void InitCT7302(BYTE slave_addr)
{
    BYTE temp_value, time_count;

    mi2c_slaveaddr = 0x22;
    // Check for chip alive	
    MI2CWriteByteMask(0x2B, 0x08, 0x08);
    Delay500us(1);
    MI2CWriteByteMask(0x2B, 0x00, 0x08);

    time_count=0;
    temp_value = MI2CReadByte(0x00);
    while(temp_value==0 && time_count++<100);
    {
        Delay500us(1);
        temp_value = MI2CReadByte(0x00);
    }
    if(time_count<100)
    {
        MI2CWriteByte(0x00,temp_value);
    }

    MI2CWriteByte(0x06,0x48);
    MI2CWriteByte(0x10,0xC0);
    MI2CWriteByte(0x11,0x00);
    MI2CWriteByte(0x12,0x08);
    MI2CWriteByte(0x13,0x00);
    MI2CWriteByte(0x14,0x40);

    MI2CWriteByte(0x30,0x23);
    MI2CWriteByte(0x31,0x19);
    MI2CWriteByte(0x32,0x1E);

    MI2CWriteByte(0x39,0xF3);
    MI2CWriteByte(0x3B,0x77);
    MI2CWriteByte(0x40,0x02);
    MI2CWriteByte(0x45,0x00);
    MI2CWriteByte(0x4E,0x77);
    MI2CWriteByte(0x47,0xA4);
    MI2CWriteByte(0x49,0x00);   // disable dpop
    MI2CWriteByte(0x4D,0x37);
    MI2CWriteByte(0x59,0x2D);
    MI2CWriteByte(0x5F,0x4B);
    MI2CWriteByte(0x61,0x08);
    MI2CWriteByte(0x62,0x01);

    MI2CWriteByte(0x04,0x36);       // SRC mode 3, Input source I2S1
    MI2CWriteByte(0x05,0x09);       // Output Freq 192K
}

void SetCT7302Mute(BYTE enable)
{
    mi2c_slaveaddr = 0x22;
    if(enable)
        MI2CWriteByteMask(0x06, 0x80, 0x80);
    else
        MI2CWriteByteMask(0x06, 0x00, 0x80);
}

// Select Input Source (SPDIF_0-SPDIF_4, I2S_0-I2S_2/DSD)
void SetCT7302InputSource(BYTE value)
{
    MI2CWriteByteMask(0x04, value, 0x07);
} 

// Get Input Source Freq. 
BYTE GetCT7302InputFreq(void)
{
    BYTE result;
    result = MI2CReadByte(0x89)&0x0F;
    return result;
}

// Get Input Source Formate (DOP/32Bits) 
BYTE GetCT7302InputFormat(void)
{
    BYTE result;
    result  = MI2CReadByte(0x77)&0xF0;
    result |= MI2CReadByte(0x7A)&0x0F;
    return result;
} 


// Reg05[3:0]
void SetCT7302OutputFreq(BYTE freq_idx)
{
    MI2CWriteByteMask(0x05, freq_idx, 0x0F);	// Output Freq
}

// Reg04[7:4]
void CT73xxSetOutputSrc(BYTE index)
{
    MI2CWriteByteMask(0x04, index<<4, 0xF0);
}


void SetCT7302Volume(BYTE channel, WORD volume)
{
    if(channel==0)
    {
        MI2CWriteByte(0x09, (BYTE)(volume>>4));
        MI2CWriteByteMask(0x08, (BYTE)volume&0x0F, 0x0F);
    }
    else if(channel==1)
    {
        MI2CWriteByte(0x0A, (BYTE)(volume>>4));
        MI2CWriteByteMask(0x08, (BYTE)((volume&0x0F)<<4), 0xF0);
    }
}


