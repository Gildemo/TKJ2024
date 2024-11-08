#ifndef _BUZZER_H_
#define _BUZZER_H_

#include "Board.h"

#define BUZZER_FREQ_MIN            3
#define BUZZER_FREQ_MAX            8000

void buzzerOpen(PIN_Handle hPinGpio);
bool buzzerSetFrequency(uint16_t frequency);
void buzzerClose(void);

#endif
