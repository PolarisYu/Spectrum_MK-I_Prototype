#ifndef __AUDIO_MANAGER_H__
#define __AUDIO_MANAGER_H__

#include "main.h"
#include "ak4493.h"
#include "njw1195a.h"
#include "ct7302.h"

/* Exported handles */
extern AK4493_HandleTypeDef hak;
extern NJW1195A_HandleTypeDef hnjw;
extern CT7302_HandleTypeDef hct;

/* Exported functions */
void Audio_Init(void);
void Audio_Task(void);
void Audio_USB_Detected(uint8_t detected);

#endif /* __AUDIO_MANAGER_H__ */
