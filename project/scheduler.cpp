#include "scheduler.h"
//#define schedUSE_TCB_ARRAY 1
#if(schedOverhead == 1)
	TaskHandle_t xLastTask = NULL;
static TickType_t SchedTimer = 0;
#endif 
#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF) // changed for HVDF
  #include "list.h"
#endif /* schedSCHEDULING_POLICY_EDF */

#define schedTHREAD_LOCAL_STORAGE_POINTER_INDEX 0

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )
	#define schedUSE_TCB_SORTED_LIST 1
#else
	#define schedUSE_TCB_ARRAY 1
#endif /* schedSCHEDULING_POLICY_EDF */

/* Extended Task control block for managing periodic tasks within this library. */
typedef struct xExtended_TCB
{
	TaskFunction_t pvTaskCode; 		/* Function pointer to the code that will be run periodically. */
	const char *pcName; 			/* Name of the task. */
	UBaseType_t uxStackDepth; 			/* Stack size of the task. */
	void *pvParameters; 			/* Parameters to the task function. */
	UBaseType_t uxPriority; 		/* Priority of the task. */
	TaskHandle_t *pxTaskHandle;		/* Task handle for the task. */
	TickType_t xReleaseTime;		/* Release time of the task. */
	TickType_t xRelativeDeadline;	/* Relative deadline of the task. */
	TickType_t xAbsoluteDeadline;	/* Absolute deadline of the task. */
	TickType_t xPeriod;				/* Task period. */
	TickType_t xLastWakeTime; 		/* Last time stamp when the task was running. */
	TickType_t xMaxExecTime;		/* Worst-case execution time of the task. */
	TickType_t xExecTime;			/* Current execution time of the task. */

	BaseType_t xWorkIsDone; 		/* pdFALSE if the job is not finished, pdTRUE if the job is finished. */

	#if( schedUSE_TCB_ARRAY == 1 )
		BaseType_t xPriorityIsSet; 	/* pdTRUE if the priority is assigned. */
		BaseType_t xInUse; 			/* pdFALSE if this extended TCB is empty. */
	#endif
#if( schedUSE_TCB_SORTED_LIST == 1 )
	ListItem_t xTCBListItem; 	/* Used to reference TCB from the TCB list. */
#endif /* schedUSE_TCB_SORTED_LIST */

	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		BaseType_t xExecutedOnce;	/* pdTRUE if the task has executed once. */
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 || schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		TickType_t xAbsoluteUnblockTime; /* The task will be unblocked at this time if it is blocked by the scheduler task. */
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME || schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
		BaseType_t xSuspended; 		/* pdTRUE if the task is suspended. */
		BaseType_t xMaxExecTimeExceeded; /* pdTRUE when execTime exceeds maxExecTime. */
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
	
	/* add if you need anything else */	
	
} SchedTCB_t;

TaskHandle_t xPreviousTaskHandle = NULL;


#if( schedUSE_TCB_ARRAY == 1 )
	static BaseType_t prvGetTCBIndexFromHandle( TaskHandle_t xTaskHandle );
	static void prvInitTCBArray( void );
	/* Find index for an empty entry in xTCBArray. Return -1 if there is no empty entry. */
	static BaseType_t prvFindEmptyElementIndexTCB( void );
	/* Remove a pointer to extended TCB from xTCBArray. */
	static void prvDeleteTCBFromArray( BaseType_t xIndex );
#endif /* schedUSE_TCB_ARRAY */

#if( schedUSE_TCB_SORTED_LIST == 1 )
static void prvAddTCBToList(SchedTCB_t *pxTCB);
static void prvDeleteTCBFromList(SchedTCB_t *pxTCB);
#endif /* schedUSE_TCB_LIST */

static TickType_t xSystemStartTime = 0;

static void prvPeriodicTaskCode( void *pvParameters );
static void prvCreateAllTasks( void );


#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
	static void prvSetFixedPriorities( void );	
#endif /* schedSCHEDULING_POLICY_RMS */
#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF)

static void prvInitEDF(void);
static void prvUpdatePrioritiesEDF(void);

#if( schedUSE_TCB_SORTED_LIST == 1 )
static void prvSwapList(List_t **ppxList1, List_t **ppxList2);
#endif /* schedUSE_TCB_SORTED_LIST */

#endif /* schedSCHEDULING_POLICY_EDF */

#if( schedUSE_SCHEDULER_TASK == 1 )
	static void prvSchedulerCheckTimingError( TickType_t xTickCount, SchedTCB_t *pxTCB );
	static void prvSchedulerFunction( void );
	static void prvCreateSchedulerTask( void );
	static void prvWakeScheduler( void );

	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		static void prvPeriodicTaskRecreate( SchedTCB_t *pxTCB );
		static void prvDeadlineMissedHook( SchedTCB_t *pxTCB, TickType_t xTickCount );
		static void prvCheckDeadline( SchedTCB_t *pxTCB, TickType_t xTickCount );				
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
		static void prvExecTimeExceedHook( TickType_t xTickCount, SchedTCB_t *pxCurrentTask );
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
	
#endif /* schedUSE_SCHEDULER_TASK */



#if( schedUSE_TCB_ARRAY == 1 )
	/* Array for extended TCBs. */
	static SchedTCB_t xTCBArray[ schedMAX_NUMBER_OF_PERIODIC_TASKS ] = { 0 };
	/* Counter for number of periodic tasks. */
	static BaseType_t xTaskCounter = 0;
#endif /* schedUSE_TCB_ARRAY */

#if( schedUSE_TCB_SORTED_LIST == 1 )

static List_t xTCBList;				/* Sorted linked list for all periodic tasks. */
static List_t xTCBTempList;			/* A temporary list used for switching lists. */
static List_t xTCBOverflowedList; 	/* Sorted linked list for periodic tasks that have overflowed deadline. */
static List_t *pxTCBList = NULL;  			/* Pointer to xTCBList. */
static List_t *pxTCBTempList = NULL;		/* Pointer to xTCBTempList. */
static List_t *pxTCBOverflowedList = NULL;	/* Pointer to xTCBOverflowedList. */

#endif /* schedUSE_TCB_LIST */

#if( schedUSE_SCHEDULER_TASK )
	static TickType_t xSchedulerWakeCounter = 0; /* useful. why? */
	static TaskHandle_t xSchedulerHandle = NULL; /* useful. why? */
#endif /* schedUSE_SCHEDULER_TASK */


#if( schedUSE_TCB_ARRAY == 1 )
	/* Returns index position in xTCBArray of TCB with same task handle as parameter. */
	static BaseType_t prvGetTCBIndexFromHandle( TaskHandle_t xTaskHandle )
	{
		static BaseType_t xIndex = 0;
		BaseType_t xIterator;

		for( xIterator = 0; xIterator < schedMAX_NUMBER_OF_PERIODIC_TASKS; xIterator++ )
		{
		
			if( pdTRUE == xTCBArray[ xIndex ].xInUse && *xTCBArray[ xIndex ].pxTaskHandle == xTaskHandle )
			{
				return xIndex;
			}
		
			xIndex++;
			if( schedMAX_NUMBER_OF_PERIODIC_TASKS == xIndex )
			{
				xIndex = 0;
			}
		}
		return -1;
	}

	/* Initializes xTCBArray. */
	static void prvInitTCBArray( void )
	{
	UBaseType_t uxIndex;
		for( uxIndex = 0; uxIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; uxIndex++)
		{
			xTCBArray[ uxIndex ].xInUse = pdFALSE;
		}
	}

	/* Find index for an empty entry in xTCBArray. Returns -1 if there is no empty entry. */
	static BaseType_t prvFindEmptyElementIndexTCB( void )
	{
		/* your implementation goes here */
		BaseType_t uxIndex;
		for( uxIndex = 0; uxIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; uxIndex++)
		{
		if (pdFALSE == xTCBArray[uxIndex].xInUse)
			{
				return uxIndex;
			}
		}
		return -1;
	}

	/* Remove a pointer to extended TCB from xTCBArray. */
	static void prvDeleteTCBFromArray( BaseType_t xIndex )
	{
		/* your implementation goes here */
		configASSERT( xIndex >= 0 && xIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS );
		configASSERT( pdTRUE == xTCBArray[ xIndex ].xInUse );

		if( xTCBArray[xIndex].xInUse == pdTRUE)
		{
			xTCBArray[xIndex].xInUse = pdFALSE;
			xTaskCounter--;
		}
	}
#endif
#if( schedUSE_TCB_SORTED_LIST == 1 )
	/* Add an extended TCB to sorted linked list. */
	static void prvAddTCBToList( SchedTCB_t *pxTCB )
	{
		/* Initialise TCB list item. */
		vListInitialiseItem( &pxTCB->xTCBListItem );
		/* Set owner of list item to the TCB. */
		listSET_LIST_ITEM_OWNER( &pxTCB->xTCBListItem, pxTCB );
		/* List is sorted by absolute deadline value. */
		listSET_LIST_ITEM_VALUE( &pxTCB->xTCBListItem, pxTCB->xAbsoluteDeadline );

			/* Insert TCB into list. */
		vListInsert( pxTCBList, &pxTCB->xTCBListItem );
	}
	
	/* Delete an extended TCB from sorted linked list. */
	static void prvDeleteTCBFromList(  SchedTCB_t *pxTCB )
	{
		uxListRemove( &pxTCB->xTCBListItem );
		vPortFree( pxTCB );
	}
#endif /* schedUSE_TCB_SORTED_LIST */

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )

/* Swap content of two lists. */
static void prvSwapList( List_t **ppxList1, List_t **ppxList2 )
{
List_t *pxTemp;
  pxTemp = *ppxList1;
  *ppxList1 = *ppxList2;
  *ppxList2 = pxTemp;
}


/* Update priorities of all periodic tasks with respect to EDF policy. */
static void prvUpdatePrioritiesEDF( void )
{
	SchedTCB_t *pxTCB;

	ListItem_t *pxTCBListItem;
	ListItem_t *pxTCBListItemTemp;
  //Serial.println("update priorities called");
	if( listLIST_IS_EMPTY( pxTCBList ) && !listLIST_IS_EMPTY( pxTCBOverflowedList ) )
	{
		prvSwapList( &pxTCBList, &pxTCBOverflowedList );
	}

	const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER( pxTCBList );
	pxTCBListItem = listGET_HEAD_ENTRY( pxTCBList );

	while( pxTCBListItem != pxTCBListEndMarker )
	{
		pxTCB = listGET_LIST_ITEM_OWNER( pxTCBListItem );

		/* Update priority in the SchedTCB list. */
		listSET_LIST_ITEM_VALUE( pxTCBListItem, pxTCB->xAbsoluteDeadline );

		pxTCBListItemTemp = pxTCBListItem;
		pxTCBListItem = listGET_NEXT( pxTCBListItem );
		uxListRemove( pxTCBListItem->pxPrevious );

		/* If absolute deadline overflowed, insert TCB to overflowed list. */
		if( pxTCB->xAbsoluteDeadline < pxTCB->xLastWakeTime )
		{
      taskENTER_CRITICAL();
      Serial.print(pxTCB->pcName);
      Serial.println(" misses");
      taskEXIT_CRITICAL();
			vListInsert( pxTCBOverflowedList, pxTCBListItemTemp );
		}
		else /* Insert TCB into temp list in usual case. */
		{
			vListInsert( pxTCBTempList, pxTCBListItemTemp );
		}
	}

	/* Swap list with temp list. */
	prvSwapList( &pxTCBList, &pxTCBTempList );

	#if( schedUSE_SCHEDULER_TASK == 1 )
		BaseType_t xHighestPriority = schedSCHEDULER_PRIORITY - 1;
	#else
		BaseType_t xHighestPriority = configMAX_PRIORITIES - 1;
	#endif /* schedUSE_SCHEDULER_TASK */

	/* assign priorities to tasks */
	const ListItem_t *pxTCBListEndMarkerAfterSwap = listGET_END_MARKER(pxTCBList);
	pxTCBListItem = listGET_HEAD_ENTRY(pxTCBList);
  Serial.println("Task\tP\tD");
  
	while (pxTCBListItem != pxTCBListEndMarkerAfterSwap)
	{
		pxTCB = listGET_LIST_ITEM_OWNER( pxTCBListItem );
    if(-1 > xHighestPriority) {
      Serial.println("priority cannot be < -1");
    }
		configASSERT( -1 <= xHighestPriority );
		pxTCB->uxPriority = xHighestPriority;
		vTaskPrioritySet( *pxTCB->pxTaskHandle, pxTCB->uxPriority );
    //taskENTER_CRITICAL();
    Serial.print(pxTCB->pcName);
    Serial.print("\t");
    Serial.print(pxTCB->uxPriority);
    Serial.print("\t");
    Serial.println(pxTCB->xAbsoluteDeadline);
    //Serial.flush();
    //taskEXIT_CRITICAL();

		xHighestPriority--;
		pxTCBListItem = listGET_NEXT( pxTCBListItem );
	}

}
#endif /* schedSCHEDULING_POLICY_EDF */


/* The whole function code that is executed by every periodic task.
 * This function wraps the task code specified by the user. */
static void prvPeriodicTaskCode( void *pvParameters )
{
 
	//SchedTCB_t *pxThisTask;
  SchedTCB_t* pxThisTask = ( SchedTCB_t * ) pvTaskGetThreadLocalStoragePointer( xTaskGetCurrentTaskHandle(), schedTHREAD_LOCAL_STORAGE_POINTER_INDEX );	
	TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();  
	
    /* your implementation goes here */
    
    /* Check the handle is not NULL. */
	
    /* If required, use the handle to obtain further information about the task. */
    /* You may find the following code helpful...
	BaseType_t xIndex;
	for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
	{
		
	}		
    */
#if( schedUSE_TCB_ARRAY == 1 )
   	BaseType_t Index = prvGetTCBIndexFromHandle(xCurrentTaskHandle);

	pxThisTask = &xTCBArray[Index];
	
   	if( pxThisTask == NULL){
		   return;
	}
#endif
#if 0 //(schedUSE_TCB_SORTED_LIST == 0)
  const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER(pxTCBList);
	ListItem_t *pxTCBListItem = listGET_HEAD_ENTRY(pxTCBList);
  Serial.println("execute task ");
	//Serial.flush();

	while (pxTCBListItem != pxTCBListEndMarker)
	{
    Serial.println("handle1");
		pxThisTask = listGET_LIST_ITEM_OWNER(pxTCBListItem);
		/* your implementation goes here. */	  //TODO
		if (xCurrentTaskHandle == *pxThisTask->pxTaskHandle) {
			break;
		}
		//Serial.flush();
		Serial.println("handle2");
		//Serial.flush();
		/*Done*/
		pxTCBListItem = listGET_NEXT(pxTCBListItem);
	}
  Serial.println("execute tasks out");
  if(pxThisTask == NULL) {
    Serial.println("task handle is NULL");
  }
#endif
  taskENTER_CRITICAL();
  Serial.println("New task");
  if(pxThisTask == NULL) {
    Serial.println("task handle is NULL");
  }
  Serial.print("delay until ");
  Serial.println(pxThisTask->xReleaseTime);
  taskEXIT_CRITICAL();  
	if( pxThisTask->xReleaseTime != 0 )
	{
    //Serial.print("delay until ");
    //Serial.println(pxThisTask->xReleaseTime);
    //pxThisTask->xReleaseTime = 0;
		xTaskDelayUntil( &pxThisTask->xLastWakeTime, pxThisTask->xReleaseTime );
	} 
	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
        /* your implementation goes here */
		pxThisTask->xExecutedOnce = pdTRUE;
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
    
	if( 0 == pxThisTask->xReleaseTime )
	{
		pxThisTask->xLastWakeTime = xSystemStartTime;
	}

	for( ; ; )
	{
    delay(100);
    //Serial.println("in for loop ");
		#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )	
				/* Wake up the scheduler task to update priorities of all periodic tasks. */
			prvWakeScheduler();
		#endif /* schedSCHEDULING_POLICY_EDF */	
		/* Execute the task function specified by the user. */

		//Serial.println("executing task ");
		pxThisTask->xWorkIsDone = pdFALSE;
		pxThisTask->pvTaskCode( pvParameters );
		pxThisTask->xWorkIsDone = pdTRUE;  
		pxThisTask->xExecTime = 0 ;

		pxThisTask->xAbsoluteDeadline = pxThisTask->xLastWakeTime + pxThisTask->xRelativeDeadline + pxThisTask->xPeriod;
		
    #if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )
			prvWakeScheduler();
		#endif /* schedSCHEDULING_POLICY_EDF */
        
		xTaskDelayUntil(&pxThisTask->xLastWakeTime, pxThisTask->xPeriod);
	}
}

/* Creates a periodic task. */
void vSchedulerPeriodicTaskCreate( TaskFunction_t pvTaskCode, const char *pcName, UBaseType_t uxStackDepth, void *pvParameters, UBaseType_t uxPriority,
		TaskHandle_t *pxCreatedTask, TickType_t xPhaseTick, TickType_t xPeriodTick, TickType_t xMaxExecTimeTick, TickType_t xDeadlineTick )
{
	taskENTER_CRITICAL();
	SchedTCB_t *pxNewTCB;
	
	#if( schedUSE_TCB_ARRAY == 1 )
		BaseType_t xIndex = prvFindEmptyElementIndexTCB();
		configASSERT( xTaskCounter < schedMAX_NUMBER_OF_PERIODIC_TASKS );
		configASSERT( xIndex != -1 );
		pxNewTCB = &xTCBArray[ xIndex ];	
	#else /* schedUSE_TCB_ARRAY */
  		pxNewTCB = pvPortMalloc( sizeof( SchedTCB_t ) );
	#endif /* schedUSE_TCB_ARRAY */

	/* Intialize item. */
		
	pxNewTCB->pvTaskCode = pvTaskCode;
	pxNewTCB->pcName = pcName;
	pxNewTCB->uxStackDepth = uxStackDepth;
	pxNewTCB->pvParameters = pvParameters;
	pxNewTCB->uxPriority = uxPriority;
	pxNewTCB->pxTaskHandle = pxCreatedTask;
	pxNewTCB->xReleaseTime = xPhaseTick;
	pxNewTCB->xPeriod = xPeriodTick;
	
    /* Populate the rest */
    /* your implementation goes here */
	pxNewTCB->xAbsoluteDeadline = xDeadlineTick;
  pxNewTCB->xMaxExecTime = xMaxExecTimeTick;
	pxNewTCB->xRelativeDeadline =xDeadlineTick;
	pxNewTCB->xWorkIsDone = pdTRUE;
	pxNewTCB->xExecTime = 0;

    
	#if( schedUSE_TCB_ARRAY == 1 )
		pxNewTCB->xInUse = pdTRUE;
	#endif /* schedUSE_TCB_ARRAY */
	
	#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
		/* member initialization */
        /* your implementation goes here */
		pxNewTCB->xPriorityIsSet = pdFALSE;
  #endif
	#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )
		pxNewTCB->xAbsoluteDeadline = pxNewTCB->xRelativeDeadline + pxNewTCB->xReleaseTime + xSystemStartTime;
		pxNewTCB->uxPriority = 0;
	#endif /*schedSCHEDULING_POLICY_EDF*/ 
	
	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		/* member initialization */
        /* your implementation goes here */
		pxNewTCB->xExecutedOnce = pdFALSE;
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
	
	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
    pxNewTCB->xSuspended = pdFALSE;
    pxNewTCB->xMaxExecTimeExceeded = pdFALSE;
		//Serial.println(pxNewTCB->xMaxExecTimeExceeded);
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */	

	#if( schedUSE_TCB_ARRAY == 1 )
		xTaskCounter++;
  #endif
	#if( schedUSE_TCB_SORTED_LIST == 1 )
		prvAddTCBToList( pxNewTCB );	
	#endif /* schedUSE_TCB_SORTED_LIST */
	
    Serial.print("TASK: ");
    Serial.print(pxNewTCB->pcName);
  	Serial.print(" MaxExecTime: ");
    Serial.print(pxNewTCB->xMaxExecTime);
    Serial.print(", Released at- ");
    Serial.print(pxNewTCB->xReleaseTime);
	  Serial.println(" is recieved.");
    taskEXIT_CRITICAL();
}

/* Deletes a periodic task. */
void vSchedulerPeriodicTaskDelete( TaskHandle_t xTaskHandle )
{
	/* your implementation goes here */
	
	BaseType_t xIndex = -1;
	
	/* your implementation goes here */
	if(xTaskHandle != NULL)
	{
  #if(schedUSE_TCB_ARRAY == 1)
		xIndex = prvGetTCBIndexFromHandle(xTaskHandle);
		if(xIndex < 0)
		{
			return;
		}
	  
		prvDeleteTCBFromArray( xIndex );
  #endif
  #if( schedUSE_TCB_SORTED_LIST == 1 )

		SchedTCB_t *pxThisTask;
		const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER(pxTCBList);
		const ListItem_t *pxTCBListItem = listGET_HEAD_ENTRY(pxTCBList);

		while (pxTCBListItem != pxTCBListEndMarker)
		{
			pxThisTask = listGET_LIST_ITEM_OWNER(pxTCBListItem);
			/* your implementation goes here. */	  //TODO
			Serial.print("Delete");
			Serial.flush();
			if (xTaskHandle == *pxThisTask->pxTaskHandle) {
				break;
			}
			pxTCBListItem = listGET_NEXT(pxTCBListItem);
			// DONE
		}

		prvDeleteTCBFromList(pxThisTask);

#endif /* schedUSE_TCB_SORTED_LIST */
	vTaskDelete( xTaskHandle );
	}
}

/* Creates all periodic tasks stored in TCB array, or TCB list. */
static void prvCreateAllTasks( void )
{
	SchedTCB_t *pxTCB;

	#if( schedUSE_TCB_ARRAY == 1 )
		BaseType_t xIndex;
		for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
		{
			configASSERT( pdTRUE == xTCBArray[ xIndex ].xInUse );
			pxTCB = &xTCBArray[ xIndex ];

			BaseType_t xReturnValue = xTaskCreate(prvPeriodicTaskCode, pxTCB->pcName, pxTCB->uxStackDepth, pxTCB->pvParameters, pxTCB->uxPriority, pxTCB->pxTaskHandle );
					
		}
	#endif //schedUSE_TCB_ARRAY
  #if( schedUSE_TCB_SORTED_LIST == 1 )
		const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER( pxTCBList );
		ListItem_t *pxTCBListItem = listGET_HEAD_ENTRY( pxTCBList );


		while( pxTCBListItem != pxTCBListEndMarker )
		{
			pxTCB = listGET_LIST_ITEM_OWNER( pxTCBListItem );
			configASSERT( NULL != pxTCB );
			BaseType_t xReturnValue = xTaskCreate( prvPeriodicTaskCode, pxTCB->pcName, pxTCB->uxStackDepth, pxTCB->pvParameters, pxTCB->uxPriority, pxTCB->pxTaskHandle );
			if( pdPASS == xReturnValue )
			{
          Serial.print(pxTCB->pcName);
          Serial.print(", Period- ");
          Serial.print(pxTCB->xPeriod);
          Serial.print(", Released at- ");
          Serial.print(pxTCB->xReleaseTime);
          Serial.print(", *Priority- ");
          Serial.print(pxTCB->uxPriority);
          Serial.print(", WCET- ");
          Serial.print(pxTCB->xMaxExecTime);
          Serial.print(", Deadline- ");
          Serial.println(pxTCB->xRelativeDeadline);
          //Serial.println();
			}
			else
			{
				/* if task creation failed */
				Serial.print(pxTCB->pcName);
				Serial.println(" failed");
			}

      vTaskSetThreadLocalStoragePointer( *pxTCB->pxTaskHandle, schedTHREAD_LOCAL_STORAGE_POINTER_INDEX, pxTCB );
			pxTCBListItem = listGET_NEXT( pxTCBListItem );
		}	
	#endif /* schedUSE_TCB_SORTED_LIST */
}

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
	/* Initiazes fixed priorities of all periodic tasks with respect to RMS policy. */
static void prvSetFixedPriorities( void )
{
	BaseType_t xIter, xIndex;
	TickType_t xShortest, xPreviousShortest=0;
	SchedTCB_t *pxShortestTaskPointer, *pxTCB;

	#if( schedUSE_SCHEDULER_TASK == 1 )
		BaseType_t xHighestPriority = schedSCHEDULER_PRIORITY - 1; 
	#else
		BaseType_t xHighestPriority = configMAX_PRIORITIES - 1;
	#endif /* schedUSE_SCHEDULER_TASK */

	for( xIter = 0; xIter < xTaskCounter; xIter++ )
	{
		xShortest = portMAX_DELAY;

		/* search for shortest period */
		for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
		{
			/* your implementation goes here */
			pxTCB = &xTCBArray[ xIndex ];
			#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS )
				/* your implementation goes here */
				if( pxTCB->xPeriod <= xShortest )
				{
					xShortest = pxTCB->xPeriod;
					pxShortestTaskPointer = pxTCB;
				}
			#endif /* schedSCHEDULING_POLICY */

			#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
				/* your implementation goes here */
				if( pxTCB->xRelativeDeadline <= xShortest )
				{
					xShortest = pxTCB->xPeriod;
					pxShortestTaskPointer = pxTCB;
				}
			#endif /* schedSCHEDULING_POLICY */
		}
		
		/* set highest priority to task with xShortest period (the highest priority is configMAX_PRIORITIES-1) */		
		
		/* your implementation goes here */
		if( xPreviousShortest != xShortest && xHighestPriority > 0)
		{
			xHighestPriority--;
		}

		pxShortestTaskPointer->uxPriority = xHighestPriority;
		pxShortestTaskPointer->xPriorityIsSet = pdTRUE;
		xPreviousShortest = xShortest;
	}
}
#endif /* schedSCHEDULING_POLICY */

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF) 
/* Initializes priorities of all periodic tasks with respect to EDF policy. */
static void prvInitEDF(void)
{
	SchedTCB_t *pxTCB;

#if( schedUSE_SCHEDULER_TASK == 1 )
	UBaseType_t uxHighestPriority = schedSCHEDULER_PRIORITY - 1;
#else
	UBaseType_t uxHighestPriority = configMAX_PRIORITIES - 1;
#endif /* schedUSE_SCHEDULER_TASK */

	const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER(pxTCBList);
	ListItem_t *pxTCBListItem = listGET_HEAD_ENTRY(pxTCBList);

	while (pxTCBListItem != pxTCBListEndMarker)
	{
		/* assigning priorities to sorted tasks */
		/* your implementation goes here. */
		pxTCB = listGET_LIST_ITEM_OWNER(pxTCBListItem);
		pxTCB->uxPriority = uxHighestPriority;
    pxTCB->xReleaseTime = 0;
    Serial.print(pxTCB->pcName);
    Serial.print(" got priority ");
    Serial.println(pxTCB->uxPriority);
		uxHighestPriority--;
		/*Done*/

		pxTCBListItem = listGET_NEXT(pxTCBListItem);
	}

}
#endif /* schedSCHEDULING_POLICY */


#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )

	/* Recreates a deleted task that still has its information left in the task array (or list). */
	static void prvPeriodicTaskRecreate( SchedTCB_t *pxTCB )
	{
		BaseType_t xReturnValue = xTaskCreate(prvPeriodicTaskCode, pxTCB->pcName, pxTCB->uxStackDepth, pxTCB->pvParameters, pxTCB->uxPriority, pxTCB->pxTaskHandle );
		if( pdPASS == xReturnValue )
		{
      vTaskSetThreadLocalStoragePointer( *pxTCB->pxTaskHandle, schedTHREAD_LOCAL_STORAGE_POINTER_INDEX, ( SchedTCB_t * ) pxTCB );
			/* your implementation goes here */			
			Serial.print(pxTCB->pcName);
			Serial.print(" is recreated. ");	
			pxTCB->xExecutedOnce = pdFALSE;
			#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
				pxTCB->xSuspended = pdFALSE;
				pxTCB->xMaxExecTimeExceeded = pdFALSE;
			#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */	
		}
		// else
		// {
		// 	/* if task creation failed */
		// }
	}

	/* Called when a deadline of a periodic task is missed.
	 * Deletes the periodic task that has missed it's deadline and recreate it.
	 * The periodic task is released during next period. */
	static void prvDeadlineMissedHook( SchedTCB_t *pxTCB, TickType_t xTickCount )
	{
		/* Delete the pxTask and recreate it. */
		Serial.print(pxTCB->pcName);
		Serial.print(" has missed deadline. ");
		vTaskDelete( *pxTCB->pxTaskHandle);
		pxTCB->xExecTime = 0;
		prvPeriodicTaskRecreate( pxTCB );	
		
		/* Need to reset next WakeTime for correct release. */
		/* your implementation goes here */
		pxTCB->xReleaseTime = pxTCB->xLastWakeTime + pxTCB->xPeriod;
		pxTCB->xLastWakeTime = 0;
		pxTCB->xAbsoluteDeadline = pxTCB->xRelativeDeadline + pxTCB->xReleaseTime;
		
	}

	/* Checks whether given task has missed deadline or not. */
	static void prvCheckDeadline( SchedTCB_t *pxTCB, TickType_t xTickCount )
	{ 
		/* check whether deadline is missed. */     		
		/* your implementation goes here */
		if( (pxTCB != NULL) && (pxTCB->xWorkIsDone == pdFALSE) && (pxTCB->xExecutedOnce == pdTRUE))
		{
			pxTCB->xAbsoluteDeadline = pxTCB->xLastWakeTime + pxTCB->xRelativeDeadline;
		
			if( ( signed ) ( pxTCB->xAbsoluteDeadline - xTickCount ) < 0 )
			{
				
		    prvDeadlineMissedHook( pxTCB, xTickCount );
			}
		}
			
	}	
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */


#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )

	/* Called if a periodic task has exceeded its worst-case execution time.
	 * The periodic task is blocked until next period. A context switch to
	 * the scheduler task occur to block the periodic task. */
	static void prvExecTimeExceedHook( TickType_t xTickCount, SchedTCB_t *pxCurrentTask )
	{
        pxCurrentTask->xMaxExecTimeExceeded = pdTRUE;
		Serial.print(pxCurrentTask->pcName);
		Serial.print(" has exceeded. Suspending the task. ");
        /* Is not suspended yet, but will be suspended by the scheduler later. */
        pxCurrentTask->xSuspended = pdTRUE;
        pxCurrentTask->xAbsoluteUnblockTime = pxCurrentTask->xLastWakeTime + pxCurrentTask->xPeriod;
        pxCurrentTask->xExecTime = 0;
        
        BaseType_t xHigherPriorityTaskWoken;
        vTaskNotifyGiveFromISR( xSchedulerHandle, &xHigherPriorityTaskWoken );
        xTaskResumeFromISR(xSchedulerHandle);
	}
#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */


#if( schedUSE_SCHEDULER_TASK == 1 )
	/* Called by the scheduler task. Checks all tasks for any enabled
	 * Timing Error Detection feature. */
	static void prvSchedulerCheckTimingError( TickType_t xTickCount, SchedTCB_t *pxTCB )
	{
		/* your implementation goes here */

		#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )						
			/* check if task missed deadline */
            /* your implementation goes here */
        
			prvCheckDeadline( pxTCB, xTickCount );						
		#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
		

		#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
        if( pdTRUE == pxTCB->xMaxExecTimeExceeded )
        {
            pxTCB->xMaxExecTimeExceeded = pdFALSE;
            vTaskSuspend( *pxTCB->pxTaskHandle );
        }
        if( pdTRUE == pxTCB->xSuspended )
        {
            if( ( signed ) ( pxTCB->xAbsoluteUnblockTime - xTickCount ) <= 0 )
            {
                pxTCB->xSuspended = pdFALSE;
                pxTCB->xLastWakeTime = xTickCount;
                vTaskResume( *pxTCB->pxTaskHandle );
            }
        }
		#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

		return;
	}

	/* Function code for the scheduler task. */
	static void prvSchedulerFunction( void *pvParameters )
	{

    
		for( ; ; )
		{ 
			#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )
				prvUpdatePrioritiesEDF();
			#endif
			#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
				TickType_t xTickCount = xTaskGetTickCount();
        		SchedTCB_t *pxTCB;
        		
				/* your implementation goes here. */			
				/* You may find the following helpful...
                    prvSchedulerCheckTimingError( xTickCount, pxTCB );
                 */
				#if( schedUSE_TCB_ARRAY == 1 )
				BaseType_t xIndex;
				for( xIndex = 0; xIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; xIndex++ )
				{
					pxTCB = &xTCBArray[ xIndex ];
					prvSchedulerCheckTimingError( xTickCount, pxTCB );
				}
        #endif
				#if( schedUSE_TCB_SORTED_LIST == 1 )
					const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER( pxTCBList );
					ListItem_t *pxTCBListItem = listGET_HEAD_ENTRY( pxTCBList );
					while( pxTCBListItem != pxTCBListEndMarker )
					{
						pxTCB = listGET_LIST_ITEM_OWNER( pxTCBListItem);

						prvSchedulerCheckTimingError( xTickCount, pxTCB );

						pxTCBListItem = listGET_NEXT( pxTCBListItem );
					}
				#endif /* schedUSE_TCB_SORTED_LIST */

			
			#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

			ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
		}
	}

	/* Creates the scheduler task. */
	static void prvCreateSchedulerTask( void )
	{
		xTaskCreate( (TaskFunction_t) prvSchedulerFunction, "Scheduler", schedSCHEDULER_TASK_STACK_SIZE, NULL, schedSCHEDULER_PRIORITY, &xSchedulerHandle );                             
	}
#endif /* schedUSE_SCHEDULER_TASK */


#if( schedUSE_SCHEDULER_TASK == 1 )
	/* Wakes up (context switches to) the scheduler task. */
	static void prvWakeScheduler( void )
	{
    Serial.println("wake scheduler");
		BaseType_t xHigherPriorityTaskWoken;
		vTaskNotifyGiveFromISR( xSchedulerHandle, &xHigherPriorityTaskWoken );
		xTaskResumeFromISR(xSchedulerHandle);    
	}

#if 0
	/* Called every software tick. */
	// In FreeRTOSConfig.h,
	// Enable configUSE_TICK_HOOK
	// Enable INCLUDE_xTaskGetIdleTaskHandle
	// Enable INCLUDE_xTaskGetCurrentTaskHandle
	// void vApplicationTickHook( void )
	// {            
	// 	SchedTCB_t *pxCurrentTask;		
	// 	TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();		
  //       UBaseType_t flag = 0;
  //       BaseType_t xIndex;
	// 	BaseType_t prioCurrentTask = uxTaskPriorityGet(xCurrentTaskHandle);

	// 	for(xIndex = 0; xIndex < xTaskCounter ; xIndex++){
	// 		pxCurrentTask = &xTCBArray[xIndex];
	// 		if(pxCurrentTask -> uxPriority == prioCurrentTask){
	// 			flag = 1;
	// 			break;
	// 		}
	// 	}
		
	// 	if( xCurrentTaskHandle != xSchedulerHandle && xCurrentTaskHandle != xTaskGetIdleTaskHandle() && flag == 1)
	// 	{
	// 		pxCurrentTask->xExecTime++;     
     
	// 		#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
  //           if( pxCurrentTask->xMaxExecTime <= pxCurrentTask->xExecTime )
  //           {
  //               if( pdFALSE == pxCurrentTask->xMaxExecTimeExceeded )
  //               {
  //                   if( pdFALSE == pxCurrentTask->xSuspended )
  //                   {
  //                       prvExecTimeExceedHook( xTaskGetTickCountFromISR(), pxCurrentTask );
  //                   }
  //               }
  //           }
	// 		#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
	// 	}

	// 	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )    
	// 		xSchedulerWakeCounter++;      
	// 		if( xSchedulerWakeCounter == schedSCHEDULER_TASK_PERIOD )
	// 		{
	// 			xSchedulerWakeCounter = 0;        
	// 			prvWakeScheduler();
	// 		}
	// 	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
	// }
  #endif
#endif /* schedUSE_SCHEDULER_TASK */


/* This function must be called before any other function call from this module. */
void vSchedulerInit( void )
{
	#if( schedUSE_TCB_ARRAY == 1 )
		prvInitTCBArray();
	#endif
  #if( schedUSE_TCB_SORTED_LIST == 1 )
		vListInitialise( &xTCBList );
		vListInitialise( &xTCBTempList );
		vListInitialise( &xTCBOverflowedList );
		pxTCBList = &xTCBList;
		pxTCBTempList = &xTCBTempList;
		pxTCBOverflowedList = &xTCBOverflowedList;
	#endif /* schedUSE_TCB_ARRAY */
}

/* Starts scheduling tasks. All periodic tasks (including polling server) must
 * have been created with API function before calling this function. */
void vSchedulerStart( void )
{
	Serial.println("Scheduler Started");	
	#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS)
		prvSetFixedPriorities();	
	#endif
  #if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )
		//prvInitEDF();
	#endif /* schedSCHEDULING_POLICY */
  #if( schedUSE_TCB_SORTED_LIST == 1 )
    //Serial.println("schedUSE_TCB_SORTED_LIST = 1");
  #endif
  #if( schedUSE_TCB_ARRAY == 1 )
		//Serial.println("schedUSE_TCB_ARRAY = 1");
	#endif
	#if( schedUSE_SCHEDULER_TASK == 1 )
		prvCreateSchedulerTask();
	#endif /* schedUSE_SCHEDULER_TASK */

  Serial.println("prvCreateAllTasks");
	prvCreateAllTasks();
	  
	xSystemStartTime = xTaskGetTickCount();

  vTaskStartScheduler();
}
