#include "scheduler.h"
#define TASK1   0
#define TASK2   0
#define TASK3   1
TaskHandle_t xHandle1 = NULL;
TaskHandle_t xHandle2 = NULL;
TaskHandle_t xHandle3 = NULL;
TaskHandle_t xHandle4 = NULL;
const char *T1 = "T1";
const char *T2 = "T2";
const char *T3 = "T3";
const char *T4 = "T4";

#if configUSE_16_BIT_TICKS == 1
    #define pdTICKS_TO_MS( xTimeInTic )   ( ( TickType_t ) ( ( ( TickType_t ) ( xTimeInTic ) * ( TickType_t ) 1000U ) / ( TickType_t ) configTICK_RATE_HZ ) )
#else
    #define pdTICKS_TO_MS( xTimeInTic )   ( ( TickType_t ) ( ( ( TickType_t ) ( xTimeInTic ) * ( TickType_t ) 1000U ) / ( TickType_t ) configTICK_RATE_HZ ) )
#endif

// the loop function runs over and over again forever
void loop() {}

void vLocalDelay(TickType_t xTickCountInterval)
{
  volatile int i;
  TickType_t xTickCountStart = xTaskGetTickCount();
  TickType_t xTickCountEnd = xTaskGetTickCount();
  xTickCountInterval = pdMS_TO_TICKS(xTickCountInterval);
  
  while ((xTickCountEnd - xTickCountStart) < xTickCountInterval) 
  {
    for (i = 0; i < 1000; i++) {  }
    xTickCountEnd = xTaskGetTickCount();
  }
}
void vLocalTickDelay(TickType_t xTickCountInterval)
{
  volatile int i;
  TickType_t xTickCountStart = xTaskGetTickCount();
  TickType_t xTickCountEnd = xTaskGetTickCount();
  //xTickCountInterval = pdMS_TO_TICKS(xTickCountInterval);
  
  while ((xTickCountEnd - xTickCountStart) < xTickCountInterval) 
  {
    for (i = 0; i < 1000; i++) {  }
    xTickCountEnd = xTaskGetTickCount();
  }
}
static void testFunc1( void *pvParameters )
{
  (void) pvParameters;
  int i,a;
  Serial.println("S => T1");
#if( TASK1 == 1 )
  vLocalDelay(200);
#elif( TASK2 == 1 )
  //vLocalDelay(240);
  //delay(pdTICKS_TO_MS(24));
  vLocalTickDelay(24);
#elif( TASK3 == 1)
  //vLocalDelay(40*2*7);
  vLocalTickDelay(36);
  //delay(pdTICKS_TO_MS(28));
#endif

  Serial.println("E => T1");
 
}

static void testFunc2( void *pvParameters )
{ 
  (void) pvParameters;  
  int i, a; 
  Serial.println("S => T2");  
#if( TASK1 == 1 )
  vLocalDelay(300);
#elif( TASK2 == 1 )
  //vLocalDelay(70);
  //delay(pdTICKS_TO_MS(7));
  vLocalTickDelay(7);
#elif( TASK3 == 1)
  //vLocalDelay(40 * 3 * 2);
  //delay(pdTICKS_TO_MS(12));
  vLocalTickDelay(12);
#endif
  Serial.println("E => T2");

}

static void testFunc3( void *pvParameters )
{ 
  (void) pvParameters;  
  int i, a; 
  Serial.println("S => T3");  
#if( TASK1 == 1 )
  vLocalDelay(350);
#elif( TASK2 == 1 )
  vLocalDelay(70);
#elif( TASK3 == 1)
  //vLocalDelay(40 * 2);
  //delay(pdTICKS_TO_MS(4));
  vLocalTickDelay(4);
#endif
  Serial.println("E => T3");
 
}

static void testFunc4( void *pvParameters )
{ 
  (void) pvParameters;  
  int i, a; 
  Serial.println("S => T4");
  vLocalDelay(550);
  
  Serial.println("E => T4");
 
}

int main( void )
{
  init();
  Serial.begin(9600);
  
  char c1 = 'a';
  char c2 = 'b';    
  char c3 = 'c';
  char c4 = 'd';    

  vSchedulerInit(); 

  /* Args:                     FunctionName, Name, StackDepth, *pvParameters, Priority, TaskHandle, Phase,         Period,             MaxExecTime,       Deadline */
#if( TASK1 == 1 )
//TaskSet 1
  vSchedulerPeriodicTaskCreate(testFunc1, "T1", (configMINIMAL_STACK_SIZE), NULL, 0, &xHandle1, pdMS_TO_TICKS(0), pdMS_TO_TICKS(3000), pdMS_TO_TICKS(100), pdMS_TO_TICKS(1000));
  vSchedulerPeriodicTaskCreate(testFunc2, "T2", (configMINIMAL_STACK_SIZE), NULL, 0, &xHandle2, pdMS_TO_TICKS(0), pdMS_TO_TICKS(3000), pdMS_TO_TICKS(200), pdMS_TO_TICKS(1500));
  vSchedulerPeriodicTaskCreate(testFunc3, "T3", (configMINIMAL_STACK_SIZE), NULL, 0, &xHandle3, pdMS_TO_TICKS(0), pdMS_TO_TICKS(3000), pdMS_TO_TICKS(150), pdMS_TO_TICKS(2000));
  vSchedulerPeriodicTaskCreate(testFunc4, "T4", (configMINIMAL_STACK_SIZE), NULL, 0, &xHandle4, pdMS_TO_TICKS(0), pdMS_TO_TICKS(3000), pdMS_TO_TICKS(300), pdMS_TO_TICKS(1500));
#elif( TASK2 == 1 )
//TaskSet 2
  vSchedulerPeriodicTaskCreate(testFunc1, "T1", (configMINIMAL_STACK_SIZE), NULL, 0, &xHandle1, pdMS_TO_TICKS(0), 50/*pdMS_TO_TICKS(500)*/, 24 /*pdMS_TO_TICKS(240)*/, 40/*pdMS_TO_TICKS(400)*/);
  vSchedulerPeriodicTaskCreate(testFunc2, "T2", (configMINIMAL_STACK_SIZE), NULL, 0, &xHandle2, pdMS_TO_TICKS(0), 20/*pdMS_TO_TICKS(200)*/, 7/*pdMS_TO_TICKS(70)*/, 10/*pdMS_TO_TICKS(100)*/);
#elif( TASK3 == 1)
//TaskSet 3
  Serial.println(pdTICKS_TO_MS(28));
  Serial.println(pdTICKS_TO_MS(12));
  Serial.println(pdTICKS_TO_MS(4));

  vSchedulerPeriodicTaskCreate(testFunc1, "T1", (configMINIMAL_STACK_SIZE), NULL, 0, &xHandle1, pdMS_TO_TICKS(0), 70/*pdMS_TO_TICKS(700 * 2)*/, 36 /*pdMS_TO_TICKS(40 * 5 * 2)*/, 70/*pdMS_TO_TICKS(700 * 2)*/);
  vSchedulerPeriodicTaskCreate(testFunc2, "T2", (configMINIMAL_STACK_SIZE), NULL, 0, &xHandle2, pdMS_TO_TICKS(0), 40 /*pdMS_TO_TICKS(400 * 2)*/, 12 /*pdMS_TO_TICKS(40 * 2 * 2)*/, 40/*pdMS_TO_TICKS(400 * 2)*/);
  vSchedulerPeriodicTaskCreate(testFunc3, "T3", (configMINIMAL_STACK_SIZE), NULL, 0, &xHandle3, pdMS_TO_TICKS(0), 20 /*pdMS_TO_TICKS(200 * 2)*/, 4 /*pdMS_TO_TICKS(40 * 2)*/, 20/*pdMS_TO_TICKS(200 * 2)*/);
#endif
  vSchedulerStart();

  /* If all is well, the scheduler will now be running, and the following line
  will never be reached. */
  
  for( ;; );
}
