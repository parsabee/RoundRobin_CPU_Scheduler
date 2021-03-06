/*
 * Created by Parsa Bagheri
 *
 * This piece of software usess open source libraries
 * I am grateful for the contribution of the developers of these libraries to open source
 * the link to the libraries used: https://github.com/jsventek/ADTsv2 
 */


/*
 * PART I:
 *
 * this program takes a list of commands; creates as many processes as the commands, 
 * and assigns each process a command to run.
 */


#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "p1fxns.h"
#include <stdio.h>

#define USAGE "usage: ./uspsv? [--quantum=<msec>] [workload_file]\n"
#define LINE_SIZE 128

int quantum= -1;/*environment variable or command line arguments get saved in here*/
/*
this is where the arguments of the workload file will be stored
*/
typedef struct argumentsLL LL;

struct argumentsLL{
	LL *next;
	char **args;
};

void clean_up(LL *head){
	LL *tmp = head->next;
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

void execute_cmds(LL *program, int num_progs){
	pid_t pid[num_progs];
	int i, status;
	LL *tmp = program;
	for(i=0; i< num_progs; i++){
		pid[i] = fork();/*fork children*/
		if(pid[i] < 0){
			p1perror(2, "Failed to fork\n");
			return;
		}
		else if(pid[i] == 0){/*child, have them execute the command*/
			execvp(*(tmp->args), tmp->args);
			/*failed to execute*/
			p1perror(2, "Execution failed\n");
			/*not exiting but retruning*/
			return;
		}
		tmp = tmp->next;
	}
	for(i=0; i<num_progs; i++){
		/*parent waits on the children to finish*/
		if(pid[i]>0){
			waitpid(pid[i], &status, 0);
		}
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
LL *process_cmd(char *line){
	int len = p1strlen(line);
	if(line[len-1]=='\n')
		line[len-1]='\0';
	int count = get_word_count(line);
	if(count == 0)
		return NULL;

	count++; /*count will tell us how much memory to allocate for the args string array, add one to it for NULL at the end*/
	LL *program = (LL *)malloc(sizeof(LL));
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
	LL *head = NULL;
	LL *current = NULL;

	while(file){
		char nextLine[LINE_SIZE];/*workload file or stdins lines get saved here*/
		LL *program;
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
	clean_up(head);
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
			/*if quantum is out of bounds*/
			if(quantum < 20 || quantum > 1000){
				p1perror(2, "The minimum quantum is 20 ms, the maximum quantum is 1000 ms\n");
				return 0;
			}
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