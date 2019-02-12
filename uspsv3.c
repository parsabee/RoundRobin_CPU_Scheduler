/*
 * Created by Parsa Bagheri
 *
 * This piece of software usess open source libraries
 * I am grateful for the contribution of the developers of these libraries to open source
 * the link to the libraries used: https://github.com/jsventek/ADTsv2 
 */

/*
 * PART III:
 *
 * this program takes a list of commands; creates as many processes as the commands, 
 * and assigns each process a command to run. Just before execution, 
 * every process is halted until all processes have their commands to run;
 * Then execution of all processes start concurrently.
 * 
 * The parent process is now in charge of scheduling based on a quantum time.
 * the parent process keep a queue of process to run.
 * the parent process allows a child process to run for a timeslice, 
 * then stops the process and adds it to the end of the queue; 
 * then takes a process from the front of the queue and lets that run for the duration of the timeslice
 * this cycle continues until every process is done executing their program.
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include "p1fxns.h"
#include "bqueue.h"

#define USAGE "usage: ./uspsv? [--quantum=<msec>] [workload_file]\n"
#define LINE_SIZE 128

int child_received = 0; /*flag to set when child received signal*/
pid_t ppid; /*The process ID of the parent process is stored here*/
int quantum = -1;/*environment variable or command line arguments get saved in here*/
int active_processes;
struct timespec ms20 = {0, 20000000}; /* 20 ms */ 

typedef struct args_q args_t;/*linked list for arguments*/
typedef struct proc proc_t;/*this struct will be used to keep track of child process and its status*/
BQueue *ready_q = NULL;/*ready queue*/
proc_t *ID;/*ID of the running process*/

struct args_q{
	args_t *next;
	char **args;
};

struct proc{
	pid_t pid;
	int status;/*1: alive, 0: not running/terminated*/
};

/*
	following set of fucntion are signal handlers to execute upon receiving a signals
*/
void sigchld_handler(int sig){

	/*
		upon receiveing a SIGCHLD,
		wait for dead processes
	*/
	sigset_t signal_set;
	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGALRM);
	sigprocmask(SIG_BLOCK, &signal_set, NULL); /*block child signals*/

	int status;

	while(waitpid(-1, &status, WNOHANG)>0){
		if(WIFEXITED(status)){
				active_processes--;
			}
	}


	sigprocmask(SIG_UNBLOCK, &signal_set, NULL);
}
void sigusr1_handler(int sig){

	/*
		upon receiving SIGUSR1 add the pid to ready queue
	*/
	pid_t ID = getpid();
	if(ID != ppid)
		child_received = 1;
	
}
void sigalrm_handler(int sig){

	/*	
		upon receiving SIGALRM stop the process,
		add it to the end of ready queue
		get the first element of the ready queue
		if has not started, start it; otherwise continue
	*/
	sigset_t signal_set;
	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGCHLD);
	sigprocmask(SIG_BLOCK, &signal_set, NULL); /*block child signals*/


	kill(ID->pid, SIGSTOP);
	bq_add(ready_q, ID); /*add process to the ready queue*/
	if(!bq_remove(ready_q, (void **)(&ID))){
	}
	
	if(ID->status == 1)
		kill(ID->pid, SIGCONT);

	if(!ID->status){
		ID->status = 1;
		kill(ID->pid, SIGUSR1);
	}
	

	sigprocmask(SIG_UNBLOCK, &signal_set, NULL);
}

/*
setting up timer in milliseconds
return 1 if sucessful, 0 otherwise
*/
int set_up_timer(){
	if(quantum < 20 || quantum > 1000){
		p1perror(2, "quantum value is not in this boundry: 20 <= quantum <=1000\n");
		return 0;
	}
	struct itimerval new_value;
	new_value.it_value.tv_sec = quantum/1000;;
	new_value.it_value.tv_usec = (quantum*1000) % 1000000;
	new_value.it_interval = new_value.it_value;

	if(setitimer(ITIMER_REAL, &new_value, NULL) == -1){
		p1perror(2, "error calling setitimer");
		return 0;
	}
	return 1;
}

/*
clean up routine
*/
void clean_up(args_t *head){
	args_t *tmp = head->next;
	if(head == NULL)
		return;
	if(head->args != NULL){
		int i;
		for(i=0; head->args[i] != NULL; ++i){
			free(head->args[i]);
		}
		free(head->args[i]);
		free(head->args);
	}
	free(head);
	if(tmp != NULL)
		clean_up(tmp);
}

/*
fork children and have them execute a command
*/
void execute_cmds(args_t *program, int num_progs){
	// pid_t pid[num_progs];
	proc_t pids[num_progs];
	int i;
	args_t *tmp = program;
	int status;
	/*subscribe SIGUSR1 and SGICHLD to sighandler*/
	if(signal(SIGUSR1, &sigusr1_handler) == SIG_ERR){
		p1perror(2, "SIGUSR1 SIGNAL SETUP FAILED\n");
		return;
	}
	if(signal(SIGCHLD, &sigchld_handler) == SIG_ERR){
		p1perror(2, "SIGCHLD SIGNAL SETUP FAILED\n");
		return;
	}
	if(signal(SIGALRM, &sigalrm_handler) == SIG_ERR){
		p1perror(2, "SIGALRM SIGNAL SETUP FAILED\n");
		return;
	}


	if(set_up_timer() == 0)/*set up time slice*/
		return;/*setting up timer failed*/
	active_processes = num_progs;
	for(i=0; i< num_progs; i++){
		pids[i].pid = fork();/*fork children*/
		pids[i].status = 0; /*set the status to 0 to show that it's not running*/
		if(pids[i].pid < 0){
			p1perror(2, "Failed to fork\n");
			return;
		}
		else if(pids[i].pid == 0){/*child, have them execute the command*/
			while(!child_received);/*wait till receiving signal*/
			execvp(*(tmp->args), tmp->args);
			/*failed to execute*/
			p1perror(2, "Execution failed\n");
			/*not exiting but retruning because we need to do the clean up routine*/
			return;
		}
		tmp = tmp->next;
	}

	/*
	 *	PARENT
	 */

	ppid = getpid();/*get the parents ID and set ppid global variable to it*/
	ready_q = bq_create(num_progs);/*parent makes the ready queue*/
	if(ready_q == NULL){
		p1perror(2, "Failed to create ready queue\n");
		return;
	}

	/*parent adds all child process IDs to the ready queue*/
	for(i=0; i<num_progs; i++){
		/*send signal to children*/
		if(!bq_add(ready_q, &(pids[i]))){
			p1perror(2, "Failed to add proccesses to ready queue");
		}
	}
	/*remove the first element of ready queue and run it*/
	bq_remove(ready_q, (void **)(&ID));
	ID->status = 1;/*about to start*/
	kill(ID->pid, SIGUSR1);
	while(active_processes){/*wait until all child processes are done*/
		(void)nanosleep(&ms20, NULL);
	}
}

/*
Helper function that counts the number of arguments in a line passed and returns it
it ccounts the arguments passed in quotation marks, as one word
*/
int get_word_count(char *line){
	int i=0;
	int count = 0;
	int word_char = 0;	/*this flag gets set everytime we see a char*/
					   	/* and resets when we see a whitespace*/
	int len=p1strlen(line);
	while(line[i]==' ' || line[i]=='\t' || line[i]=='\n' || line[i]=='\'' || line[i]=='\"')
		i++;
	if(i>=len)
		return count;
	while(line[i] != '\0'){
		if(line[i]!=' ' && line[i]!='\t' && line[i]!='\n' && line[i]!='\'' && line[i]!='\"'){
			count++;
			word_char = 1;/*now we're parsing a word*/
		}
		while(word_char){
			i++;
			if(line[i]=='\0' || line[i]==' ' || line[i]=='\t' || line[i]=='\n' || line[i]=='\'' || line[i]=='\"')
				word_char = 0;/*we're no longer processing the word*/
		}
		if(line[i] != '\0')
			i++;
	}
	return count;
}
/*
processes command, form stdin or workload file
returns an argumentLL if successful, NULL otherwise
*/
args_t *process_cmd(char *line){
	int len = p1strlen(line);
	if(line[len-1]=='\n')
		line[len-1]='\0';
	int count = get_word_count(line);
	if(count == 0)
		return NULL;

	count++; /*count will tell us how much memory to allocate for the args string array, add one to it for NULL at the end*/
	args_t *program = (args_t *)malloc(sizeof(args_t));
	if(program != NULL){
		char **tmp = (char **)malloc(count*sizeof(char *));
		if(tmp != NULL){
			int i = 0;
			int counter = 0;
			for(i=0; i<len; i++){
				char word[32];
				i = p1getword(line, i, word);
				char *str = p1strdup(word);
				if(str != NULL){
					tmp[counter] = str;
					counter++;
				}
				else{
					/*clean up everything that was allocated*/
					clean_up(program);
					return NULL;
				}
			}
			tmp[counter]=NULL;
			program->args = tmp;
			program->next = NULL;
		}	
		else{
			free(program);
			return NULL;
		}
	}
	return program;
}

/*
processes file or stdin
return NULL if unsuccessful
*/
void process_fd(int fd){
	int file = 1;
	int num_progs = 0;
	args_t *head = NULL;
	args_t *current = NULL;

	while(file){
		char nextLine[LINE_SIZE];/*workload file or stdins lines get saved here*/
		args_t *program;
		file = p1getline(fd, nextLine, LINE_SIZE);
		if(file){
			program = process_cmd(nextLine);
			if(program != NULL){
				num_progs++;
				if(head == NULL)
					head = current = program;
				else
					current->next = program;
				current = program;
			}
		}
	}
	execute_cmds(head, num_progs);
	clean_up(head);/*after clean up, parent process and failed child processes exit*/
	if(ready_q != NULL){
		bq_destroy(ready_q, NULL);
	}
}


int main(int argc, char *argv[]){
	if(argc > 3){
		p1perror(2, USAGE);
		return 0;
	}

	if(argc==1){

		/*quantum value not specified, get it from the environment*/
		char *c = getenv("USPS_QUANTUM_MSEC");

		if(c == NULL){
			p1putstr(2, USAGE);
			p1perror(2, "environment variable 'USPS_QUANTUM_MSEC' not detected nor specified\n");
			return 0;
		}
		quantum = p1atoi(c);/*got it from the environment*/

		/*process stdin*/
		process_fd(0);

	}
	else if(argc==2){

		/*check if the first argument specifies the quantum value*/
		if(p1strneq(argv[1], "--quantum=", 10)){

			char c[10];
			p1strcpy(c, argv[1]+10);

			if(c[0]=='-'){
				p1perror(2, "Negative number is not accepted\n");
				return 0;
			}
			quantum = p1atoi(c);/*if the quantum argument is specified, we set it*/

			/*process stdin*/
			process_fd(0);
		}
		/*first argument is not the quantum value, check the environment*/
		else{

			/*quantum value not specified, get it from the environment*/
			char *c = getenv("USPS_QUANTUM_MSEC");

			if(c == NULL){
				p1putstr(2, USAGE);
				p1perror(2, "environment variable 'USPS_QUANTUM_MSEC' not detected nor specified\n");
				return 0;
			}
			quantum = p1atoi(c);/*got it from the environment*/
			/*if quantum is out of bounds*/
			if(quantum < 20 || quantum > 1000){
				p1perror(2, "The minimum quantum is 20 ms, the maximum quantum is 1000 ms\n");
				return 0;
			}
			/*process the argument*/
			int fd;
			fd = open(argv[1], O_RDONLY);

			if(fd == -1){
				p1perror(2, "Error occured while openning file\n");
				return 0;
			}
			process_fd(fd);
			close(fd);
		}
	}
	else{

		/*both quantum and workload file specified*/
		if(!p1strneq(argv[1], "--quantum=", 10)){
			p1putstr(2, USAGE);
			return 0;
		}
		char c[10];
		p1strcpy(c, argv[1]+10);

		if(c[0]=='-'){
			p1perror(2, "Negative number is not accepted\n");
			return 0;
		}
		quantum = p1atoi(c);
		/*if quantum is out of bounds*/
		if(quantum < 20 || quantum > 1000){
			p1perror(2, "The minimum quantum is 20 ms, the maximum quantum is 1000 ms\n");
			return 0;
		}
		/*process the 2nd argument*/
		int fd;
		fd = open(argv[2], O_RDONLY);

		if(fd == -1){
			p1perror(2, "Error occured while openning file\n");
			return 0;
		}
		process_fd(fd);
		close(fd);
	}

	return 1;

}