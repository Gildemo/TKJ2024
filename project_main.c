/* C Standard library */
#include <stdio.h>
#include <stdlib.h>
/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

/* Board Header files */
#include "Board.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"
/* Task */
#define STACKSIZE 4096
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];
enum state { IDLE =1 ,WAITING, DATA_READY, MESSAGE_READY };
enum state programState = IDLE;

//tulostettavan merkin alustus
char viesti  [] = "ei viestia";
//char *viestipointer = &viesti;


//button counter nahoittimelle
int buttoncounter = 0;
int *bcpointer = &buttoncounter;
int mflag1 = 0;
int mflag2 = 0;
//MPU tulosten muuttujien alustus
float ax, ay, az, gx, gy, gz;


// RTOS:n globaalit muuttujat pinnien käyttöön
static PIN_Handle buttonHandle0;
static PIN_State buttonState0;
static PIN_Handle buttonHandle1;
static PIN_State buttonState1;
static PIN_Handle ledHandle;
static PIN_State ledState;
static PIN_Handle hMpuPin;
static PIN_State MpuPinState;

// MPU power pin
PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

// Pinnien alustukset, molemmille pinneille oma konfiguraatio
// Vakio BOARD_BUTTON_0 vastaa toista painonappia
PIN_Config button0Config[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
};
PIN_Config button1Config[] = {
   Board_BUTTON1  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
};

// Vakio Board_LED0 vastaa toista lediä

PIN_Config ledConfig[] = {
   Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
};

// oma I2C väylä
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};



void buttonFxn(PIN_Handle handle, PIN_Id pinId) {

       programState = WAITING;
       *bcpointer +=1;
       if (buttoncounter == 3){
                  PIN_setOutputValue( ledHandle, Board_LED0, 0);
                  PIN_setOutputValue( ledHandle, Board_LED1, 0);
              *bcpointer = 0;
       }

       if (buttoncounter == 0){
           strcpy(viesti," \r\n\0");
           System_printf("buttoncounter: %d\n", buttoncounter);
           System_flush();
       }


       if (buttoncounter == 1){
           PIN_setOutputValue( ledHandle, Board_LED0, 1);
           strcpy(viesti,".\r\n\0");
           System_printf("buttoncounter: %d\n", buttoncounter);
           System_flush();
       }
       if (buttoncounter == 2){
           PIN_setOutputValue( ledHandle, Board_LED1, 1);
           strcpy(viesti,"-\r\n\0");
           System_printf("buttoncounter: %d\n", buttoncounter);
           System_flush();
       }
}

void buttonFxn2(PIN_Handle handle, PIN_Id pinId) {
    programState = DATA_READY;
    System_printf("button2");
    System_flush();
}

/* Task Functions */
void uartTaskFxn(UArg arg0, UArg arg1) {



       // UART-kirjaston asetukset
       UART_Handle uart;
       UART_Params uartParams;

       // Alustetaan sarjaliikenne
       UART_Params_init(&uartParams);
       uartParams.writeDataMode = UART_DATA_TEXT;
       uartParams.readDataMode = UART_DATA_TEXT;
       uartParams.readEcho = UART_ECHO_OFF;
       uartParams.readMode=UART_MODE_BLOCKING;
       uartParams.baudRate = 9600; // nopeus 9600baud
       uartParams.dataLength = UART_LEN_8; // 8
       uartParams.parityType = UART_PAR_NONE; // n
       uartParams.stopBits = UART_STOP_ONE; // 1

       // Avataan yhteys laitteen sarjaporttiin vakiossa Board_UART0
       uart = UART_open(Board_UART0, &uartParams);
       if (uart == NULL) {
          System_abort("Error opening the UART");}

    while (1) {

        //       Muista tilamuutos
        if(programState == DATA_READY){
            char merkkijono[4];

                    sprintf(merkkijono, "%s",viesti);
                    System_printf("viesti:%s",merkkijono);
                    UART_write(uart,merkkijono, sizeof(merkkijono));
                    PIN_setOutputValue( ledHandle, Board_LED0, 0 );
                    PIN_setOutputValue( ledHandle, Board_LED1, 0 );
                    *bcpointer = 0;
                    strcpy(viesti," \r\n\0");
                    System_flush();
                    programState = IDLE;
        }
        if(programState == MESSAGE_READY && mflag1 == 1){
            Task_sleep(50000);
            UART_write(uart,".\r\n\0",4);
            Task_sleep(50000);
            UART_write(uart,".\r\n\0",4);
            Task_sleep(50000);
            UART_write(uart,".\r\n\0",4);
            Task_sleep(50000);
            UART_write(uart," \r\n\0",4);
            Task_sleep(50000);
            UART_write(uart,"-\r\n\0",4);
            Task_sleep(50000);
            UART_write(uart,"-\r\n\0",4);
            Task_sleep(50000);
            UART_write(uart,"-\r\n\0",4);
            Task_sleep(50000);
            UART_write(uart," \r\n\0",4);
            Task_sleep(50000);
            UART_write(uart,".\r\n\0",4);
            Task_sleep(50000);
            UART_write(uart,".\r\n\0",4);
            Task_sleep(50000);
            UART_write(uart,".\r\n\0",4);
            Task_sleep(50000);
            UART_write(uart," \r\n\0",4);
            Task_sleep(50000);
            UART_write(uart," \r\n\0",4);
            Task_sleep(50000);
            System_flush();
            programState = IDLE;
            PIN_setOutputValue( ledHandle, Board_LED0, 0 );
            *bcpointer = 0;
            strcpy(viesti," \r\n\0");
        }
        if((programState == MESSAGE_READY && mflag2 == 1)){
            Task_sleep(50000);
            UART_write(uart,"-\r\n\0",4);
            Task_sleep(50000);
            UART_write(uart,"-\r\n\0",4);
            Task_sleep(50000);
            UART_write(uart,"-\r\n\0",4);
            Task_sleep(50000);
            UART_write(uart," \r\n\0",4);
            Task_sleep(50000);
            UART_write(uart,"-\r\n\0",4);
            Task_sleep(50000);
            UART_write(uart,".\r\n\0",4);
            Task_sleep(50000);
            UART_write(uart,"-\r\n\0",4);
            Task_sleep(50000);
            UART_write(uart," \r\n\0",4);
            Task_sleep(50000);
            UART_write(uart," \r\n\0",4);
            Task_sleep(50000);
            System_flush();
            programState = IDLE;
            PIN_setOutputValue( ledHandle, Board_LED0, 0 );
            PIN_setOutputValue( ledHandle, Board_LED1, 0 );
            *bcpointer = 0;
            strcpy(viesti," \r\n\0");

        }

        Task_sleep(100000 / Clock_tickPeriod);
    }
}

//SensorFxn from mpu9250_example.c from jtkj-sensortag-examples
void sensorTaskFxn(UArg arg0, UArg arg1) {

    float ax, ay, az, gx, gy, gz;

    I2C_Handle i2cMPU; // Own i2c-interface for MPU9250 sensor
    I2C_Params i2cMPUParams;

    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    // Note the different configuration below
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

    // MPU power on
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

    // Wait 100ms for the MPU sensor to power up
    Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // MPU open i2c
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL) {
        System_abort("Error Initializing I2CMPU\n");
    }

    // MPU setup and calibration
    System_printf("MPU9250: Setup and calibration...\n");
    System_flush();

    mpu9250_setup(&i2cMPU);

    System_printf("MPU9250: Setup and calibration OK\n");
    System_flush();

    // Loop forever
    while (1) {

        // MPU ask data
        mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
        char arvot[25];
        sprintf(arvot, "z-kiihtyvyy %2.3f y-kiihtyvyys %2.3f\n", az, ay);
        System_printf(arvot);
        System_flush();
        if (buttoncounter == 1 && abs(az) >= 2.0){
            mflag1 = 1;
            mflag2 = 0;
            programState = MESSAGE_READY;
        }
        if (buttoncounter == 2 && ( abs(ay) >= 0.5 || abs(ax) >= 0.5)){
            mflag1 = 0;
            mflag2 = 1;
            programState = MESSAGE_READY;
                }
        // Sleep 100ms
        Task_sleep(100000 / Clock_tickPeriod);
    }

    // Program never gets here..
    // MPU close i2c
    // I2C_close(i2cMPU);
    // MPU power off
    // PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_OFF);
}


    Int main(void) {

    // Task variables
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    // Initialize board, I2C and UART
    Board_initGeneral();

    Board_initI2C();
    Board_initUART();

    // Ledi käyttöön ohjelmassa
    ledHandle = PIN_open( &ledState, ledConfig );
    if(!ledHandle) {
       System_abort("Error initializing LED pin\n");
    }

    // Painonappi käyttöön ohjelmassa
    buttonHandle0 = PIN_open(&buttonState0, button0Config);
    if(!buttonHandle0) {
       System_abort("Error initializing button pin\n");
    }

    // Painonapille keskeytyksen käsittellijä
    if (PIN_registerIntCb(buttonHandle0, &buttonFxn) != 0) {
       System_abort("Error registering button callback function");
    }

    buttonHandle1 = PIN_open(&buttonState1, button1Config);
        if(!buttonHandle1) {
           System_abort("Error initializing button pin\n");
        }

        // Painonapille keskeytyksen käsittellijä
        if (PIN_registerIntCb(buttonHandle1, &buttonFxn2) != 0) {
           System_abort("Error registering button callback function");
        }


    /* Task */
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority=2;
    sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);
    if (sensorTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority=2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);
    if (uartTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
