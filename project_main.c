/* C Standard library */
#include <stdio.h>

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
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

/* Board Header files */
#include "Board.h"
#include "sensors/opt3001.h"

/* Task */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];
enum state { WAITING=1, DATA_READY };
enum state programState = WAITING;

//tulostettavan merkin alustus
char merkki;
char *merkkipointer = &merkki;

//valon arvon alustus
double ambientLight = -1000.0;

// RTOS:n globaalit muuttujat pinnien käyttöön
static PIN_Handle buttonHandle;
static PIN_State buttonState;
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
PIN_Config buttonConfig[] = {
   Board_BUTTON1  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
};

// Vakio Board_LED0 vastaa toista lediä

PIN_Config ledConfig[] = {
   Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
};

// oma I2C väylä
//static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
//    .pinSDA = Board_I2C0_SDA1,
//    .pinSCL = Board_I2C0_SCL1
//};


//button counter nahoittimelle
int buttoncounter = 0;
int *bcpointer = &buttoncounter;

void buttonFxn(PIN_Handle handle, PIN_Id pinId) {

       *bcpointer +=1;
       if (buttoncounter == 2){
       *bcpointer = 0;}


       PIN_setOutputValue( ledHandle, Board_LED0, buttoncounter );


}

/* Task Functions */
Void uartTaskFxn(UArg arg0, UArg arg1) {



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
        if(programState == DATA_READY && buttoncounter == 1){
            char merkkijono[20];

                    sprintf(merkkijono, "%10s\n\r",*merkkipointer);
                    System_printf(*merkkipointer);
                    UART_write(uart,merkkijono, sizeof(merkkijono));
                    PIN_setOutputValue( ledHandle, Board_LED0, 0 );
                    *bcpointer = 0;
                    *merkkipointer = '\0';
                    System_flush();
                    programState = WAITING;
                    }

        Task_sleep(1000000 / Clock_tickPeriod);

    }
}

Void sensorTaskFxn(UArg arg0, UArg arg1) {

    I2C_Handle      i2c;
    I2C_Params      i2cParams;
    I2C_Transaction i2cMessage;

    // Alustetaan i2c-väylä
       I2C_Params_init(&i2cParams);
       i2cParams.bitRate = I2C_400kHz;
       // Avataan yhteys
          i2c = I2C_open(Board_I2C_TMP, &i2cParams);
          if (i2c == NULL) {
             System_abort("Error Initializing I2C\n");
          }

          Task_sleep(100000 / Clock_tickPeriod);
          opt3001_setup(&i2c);


    while (1) {
        if(programState == WAITING){

           ambientLight = opt3001_get_data(&i2c);
           if (ambientLight > 0 && ambientLight < 30){
               *merkkipointer = ",";
           }
           programState = DATA_READY;
        }
           Task_sleep(1000000 / Clock_tickPeriod);

    }
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
    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if(!buttonHandle) {
       System_abort("Error initializing button pin\n");
    }

    // Painonapille keskeytyksen käsittellijä
    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
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
