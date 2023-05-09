#########Earliest Deadline First#############
Source tree
│   FreeRTOSConfig.h
│   README.txt
│
└───project
        project.ino
        scheduler.cpp
        scheduler.h

FreeRTOSConfig.h - replacyour config with this config has ot has some stack size changes.
project.ino has the set of 3 different sets of tasks to execute. modify the #define like below to select a task set.

#define TASK1   0
#define TASK2   0
#define TASK3   1
