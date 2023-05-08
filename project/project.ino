#include "scheduler.h"

TaskHandle_t xHandle1 = NULL;
TaskHandle_t xHandle2 = NULL;
TaskHandle_t xHandle3 = NULL;
TaskHandle_t xHandle4 = NULL;
const char *T1 = "T1";
const char *T2 = "T2";
const char *T3 = "T3";
const char *T4 = "T4";

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

static void testFunc1( void *pvParameters )
{
  (void) pvParameters;
  int i,a;
  Serial.println("Task 1 is executing");
  vLocalDelay(100);
  Serial.println("Task 1 is completed");
 
}

static void testFunc2( void *pvParameters )
{ 
  (void) pvParameters;  
  int i, a; 
  Serial.println("Task 2 is executing");  
  vLocalDelay(200);
  Serial.println("Task 2 is completed");

}

static void testFunc3( void *pvParameters )
{ 
  (void) pvParameters;  
  int i, a; 
  Serial.println("Task 3 is executing");  
  vLocalDelay(150);
  Serial.println("Task 3 is completed");
 
}

static void testFunc4( void *pvParameters )
{ 
  (void) pvParameters;  
  int i, a; 
  Serial.println("Task 4 is executing");
  vLocalDelay(300);
  
  Serial.println("Task 4 is completed");
 
}

int main( void )
{
  init();
  Serial.begin(115200);
  
  char c1 = 'a';
  char c2 = 'b';    
  char c3 = 'c';
  char c4 = 'd';    

  vSchedulerInit(); 

  /* Args:                     FunctionName, Name, StackDepth, *pvParameters, Priority, TaskHandle, Phase,         Period,             MaxExecTime,       Deadline */
//TaskSet 1
  vSchedulerPeriodicTaskCreate(testFunc1, "T1", (configMINIMAL_STACK_SIZE), NULL, 0, &xHandle1, pdMS_TO_TICKS(0), pdMS_TO_TICKS(3000), pdMS_TO_TICKS(100), pdMS_TO_TICKS(1000));
  vSchedulerPeriodicTaskCreate(testFunc2, "T2", (configMINIMAL_STACK_SIZE), NULL, 0, &xHandle2, pdMS_TO_TICKS(0), pdMS_TO_TICKS(3000), pdMS_TO_TICKS(200), pdMS_TO_TICKS(1500));
  vSchedulerPeriodicTaskCreate(testFunc3, "T3", (configMINIMAL_STACK_SIZE), NULL, 0, &xHandle3, pdMS_TO_TICKS(0), pdMS_TO_TICKS(3000), pdMS_TO_TICKS(150), pdMS_TO_TICKS(2000));
  vSchedulerPeriodicTaskCreate(testFunc4, "T4", (configMINIMAL_STACK_SIZE), NULL, 0, &xHandle4, pdMS_TO_TICKS(0), pdMS_TO_TICKS(3000), pdMS_TO_TICKS(300), pdMS_TO_TICKS(1500));

//TaskSet 2 from HW5
  // vSchedulerPeriodicTaskCreate(testFunc1, "t1", configMINIMAL_STACK_SIZE, &c1, NULL, &xHandle1, pdMS_TO_TICKS(0), pdMS_TO_TICKS(1000), pdMS_TO_TICKS(600), pdMS_TO_TICKS(1000));
  // vSchedulerPeriodicTaskCreate(testFunc2, "t2", configMINIMAL_STACK_SIZE, &c2, NULL, &xHandle2, pdMS_TO_TICKS(0), pdMS_TO_TICKS(1500), pdMS_TO_TICKS(1000), pdMS_TO_TICKS(1500));



  vSchedulerStart();

  /* If all is well, the scheduler will now be running, and the following line
  will never be reached. */
  
  for( ;; );
}
