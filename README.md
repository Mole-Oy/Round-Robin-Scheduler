## Round Robin Scheduler
---

This program simulates an operating system utilizing the **Round Robin Scheduling Algorithm** to schedule incoming processes.  The algorithm cycles through each active process and allocates a fixed interval of CPU execution time, aslso known as a *Time Quantum*. Round Robin gives every process and equal opportunity to execute, preventing the starvation of low-priority processes.

The simulation keeps track of the CPU tick, which is tracked in seconds using the `time.h` module. The simulation also keeps track of two queues: the ready queue (RQ), which collects all incoming processes at the beginning of the new CPU cycle; n and the execution queue (EQ) storing all active processes to be completed. 

Once the time quantum is elapsed, the active process is halted and
