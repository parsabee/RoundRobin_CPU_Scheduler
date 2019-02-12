# Round Roobin Scheduler

User Space Process Scheduler (USPS, not to be confused with the US Postal Service) reads a list of programs, with arguments, from a file specified as an argument to the scheduler (uses standard input if unspecified), starts up the programs in processes, and then schedules the processes to run concurrently in a time-sliced manner.  It will also monitor the processes, keeping track of how the processes are using system resources.
The usage string for uspsv? is:usage: ./uspsv? [–-quantum=<msec>] [workload_file] where ? is replaced by 1, 2, or 3.  The quantum (in milliseconds) is optional, since uspsv? will look in the environment for an environment variable named USPS_QUANTUM_MSEC,before processing the command line arguments. Thus, if the environment variable is defined, then uspsv? takes the value from the environment variable; if the corresponding argument is also specified, the argument overrides the environment variable; if neither is specified, the program should exit with an error message. If the workload_file argument is not specified, the program is to read the commands and arguments from its standard input.

# USPSv1

USPS v1 will perform the following steps:
•Read the program workload from the specified file/standard input.  Each line in the file contains the name of the program and its arguments (just as you would present them to bash).  
•For each program, launch the program to run as a process using the fork(), execvp(), and any other required system calls – see below.  To make things simpler, assume that the programs will run in the same environment as used by USPS.  
•Once all of the programs are running, wait for each process to terminate.  
•After all of the processes have terminated, the USPS exits.  

# USPSv2

•Immediately after each process is created using fork(), the child process waits on the SIGUSR1 signal before calling execvp().  
•After all of the processes have been created and are awaiting the SIGUSR1 signal, the USPS parent process sends each program a SIGUSR1 signal to wake them up.  Each child process will then invoke execvp() to run the workload process.  
•After all of the processes have been awakened and are executing, the USPS sends each process a SIGSTOP signal to suspend it.  
•After all of the workload processes have been suspended, the USPS sends each process a SIGCONT signal to resume it.  
•Once all processes are back up and running, the USPS waits for each process to terminate.  After all have terminated, USPS exits

# USPSv3

Now that the USPS can suspend and resume workload processes, we want to implement a scheduler that runs the processes according to some scheduling policy.  The simplest policy is toequally share the processor by giving each process the same amount of time to run (e.g., 250 ms).  In this case, there is 1 workload process executing at any given time.  After its time slice has completed, we need to suspend that process and start up another ready process.  The USPS decides the next workload process to run, starts a timer, and resumes that process.USPS v2 knows how to resume a process, but we still need a way to have it run for only a certain amount of time.  Note, if some workload process is running, it is still the case that the USPS is running concurrently with it.  Thus, one way to approach the problem is for the USPS to poll the system time to determine when the time slice has expired.  This is inefficient, as it is a form of busy waiting.  Alternatively, you can set an alarm using the alarm(2) system call.  This tells the operating system to deliver a SIGALRM signal after some specified time; unfortunately, the finest time granularity that can be specified to the alarm system call is 1 second.  The setitimer(2)system call enables one to establish an interval timer.  Signal handling is done by registering a signal handling function with the operating system. This SIGALRM signal handler is implemented in the USPS.  When the signal is delivered, the USPS is interrupted and the signal handling function is executed.  When it does, the USPS will suspend the running workload process, determine the next workload process to run, and send it a SIGCONT signal, and continue with whatever else it is doing.Your new and improved USPS v3 is now a working process scheduler.