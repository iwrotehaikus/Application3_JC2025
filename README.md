# Application3_JC2025
Real Time Systems Application 3

John Crawford

5286620

Context: Space Station / Satellite Beacon

# Analysis Questions – RTOS Application 3

### Polling vs. Interrupt
Using an ISR with a semaphore is more efficient because the logger task only runs when the button is pressed, avoiding continuous CPU cycles checking the button. Polling wastes CPU time, can miss quick presses if the delay is too long, and increases system latency.

### ISR Design
Inside an ISR, we must use FreeRTOS FromISR APIs because regular calls can block or call scheduler functions, which is unsafe in interrupt context. Calling xSemaphoreTake in an ISR could cause deadlocks, crashes, or corrupt the scheduler.

### Real-Time Behavior
When the button is pressed while the LightSensorTask is running, the ISR executes immediately, giving the semaphore and setting xHigherPriorityTaskWoken. portYIELD_FROM_ISR causes the scheduler to preempt the sensor task immediately because the LoggerTask has higher priority. The sensor task is paused mid-execution, demonstrating true preemptive behavior.

### Core Affinity
Without pinning, the ISR might run on one core and the LoggerTask could run on another. This could create race conditions or delays in log processing due to cross-core scheduling. Pinning all tasks to Core 1 ensures deterministic behavior and simplifies reasoning about preemption.

### Light Sensor Logging
The sensor task writes readings to a circular buffer protected by a mutex. The LoggerTask acquires the same mutex before processing to prevent conflicts. Without mutual exclusion, the logger could read partial or inconsistent data. A more robust design could use a double buffer or copy the data before processing.

### Task Priorities
If the LoggerTask had lower priority (1) and Blink had higher priority (3), pressing the button would not immediately dump the log. The Blink task would continue until it blocks, delaying the LoggerTask. This shows the importance of proper priority assignment in preemptive scheduling.

### Resource Usage
ISRs should be short because:  
1. Long ISRs can delay other interrupts, increasing latency for time-critical tasks.  
2. They can interfere with scheduler ticks, causing jitter or delayed task switching.  
In this lab, the ISR only gives a semaphore, deferring heavy log processing to the task.

### Chapter Connections
From Mastering the FreeRTOS Kernel Ch. 7:  

“A binary semaphore can be used to synchronize an ISR with a task, avoiding polling.”  

Our implementation applies this by having the button ISR give a semaphore, and the LoggerTask blocks until the semaphore is given, demonstrating deferred interrupt processing and event-driven design.
