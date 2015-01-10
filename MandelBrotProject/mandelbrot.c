#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<signal.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<string.h>
#include<sys/shm.h>


/* mandelbrot.c by Bryan Mendoza (bmendo6) */


int mqid1 = 0, mqid2 = 0;
int smid = 0;

pid_t child1, child2;

//free message Qs
void release_messageQ(int msg1, int msg2){

	msgctl(msg1,IPC_RMID, NULL);
	msgctl(msg2,IPC_RMID, NULL);
}

//Struct for receiving message
typedef struct{

	long int mtype;
	char message[60];


}MessageReceive;


//Struct for sending message
typedef struct{

	long int mtype;
	char filename[1];

}MessageStruct;


//Hnadler for Control C and SIGCHLD
void handler(int sig){

	int reaped;
	int status;

	kill(child1,SIGUSR1);
	kill(child2,SIGUSR1);

	while ((reaped = wait(&status)) > 0){

		if(WIFEXITED(status)){
			printf("Child %d exit: %d.\n",reaped,WEXITSTATUS(status));
		}
	}
	
	
	shmctl(smid,IPC_RMID,NULL);
	release_messageQ(mqid1, mqid2);
	exit(1);
}


//Check input for xmin xmax

int check_X(char* string, double* value){

	int check;

	check = sscanf(string,"%lf",value);

	if(check <= 0){
		fprintf(stderr,"\nError:Enter number between -2.0 and 2.0\n");
		return 0;
	}
	else{

		if(*value >= -2.0 && *value <= 2.0){
			return 1;
		} 
		else{
			fprintf(stderr,"\nError:Enter number between -2.0 and 2.0\n");
			return 0;

		}

	}

} 


//Check input for ymin ymax

int check_Y(char* string, double* value){

	int check;

	check = sscanf(string,"%lf",value);

	if(check <= 0){
		fprintf(stderr,"\nError:Enter number between -1.5 and 1.5\n");
		return 0;
	}
	else{
		if(*value >= -1.5 && *value <= 1.5){
			return 1;
		} 
		else{
			fprintf(stderr,"\nError:Enter number between -1.5 and 1.5\n");
			return 0;

		}

	}

} 


//Check input for max iterations

int check_maxIter(char* string, int* value){

	int check;
	check = sscanf(string,"%d",value);
	if(check <= 0){
		fprintf(stderr,"\nError:Enter max iterations\n");
		return 0;
	}
	else{
		if(*value <= 0){
			fprintf(stderr,"\nError:Enter max iteration greater than zero\n");
			return 0;
		} 
		else{
			return 1;

		}
	}

} 


//Check input for rows and columns
//Max size of 1024 x 1024
int check_rows_cols(char* string, int* value){

	int check;
		
	check = sscanf(string,"%d",value);

	if(check <= 0){
		fprintf(stderr,"\nError:Enter a number.\n");
		return 0;
	}
	else{
 
		if(*value <= 0 || *value > 1024){
			fprintf(stderr,"\nError: Number must be greater than 0 and no bigger than 1024\n");
			return 0;
		} 

		else{
			return 1;

		}
	}

} 


//Calculate pipe string length

int pipe_string_len(char* xmin, char* xmax, char* ymin, char* ymax, char* maxiter, char* C, char* R){

	int len = 0;
	len = len + strlen(xmin) + strlen(xmax) +  strlen(ymin) + strlen(ymax) + strlen(maxiter) +
		 strlen(C) + strlen(R);

	return len;

}


//Create new string for pipe using user input strings.

char* new_pipe_string(char* xmin, char* xmax, char* ymin, char* ymax, char* maxiter, char* C, char* R, int size){

	char* new_string = malloc(size+1);

	int len;
	len = strlen(xmin);
	xmin[len-1] = ' ';

	len = strlen(xmax);
	xmax[len-1] = ' ';

	len = strlen(ymin);
	ymin[len-1] = ' ';

	len = strlen(ymax);
	ymax[len-1] = ' ';

	len = strlen(maxiter);
	maxiter[len-1] = ' ';

	len = strlen(C);
	C[len-1] = ' ';

	snprintf(new_string,size+1,"%s%s%s%s%s%s%s",
		xmin,xmax,ymin,ymax,maxiter,C,R);

	return new_string;
	
}

//Checks if user is done with the program

int check_command(char* command_string){

	if(strcmp(command_string,"yes") == 0){
		return 1;
	}
	else if(strcmp(command_string,"no") == 0){
		return 2;
	}
	else{
		fprintf(stderr,"Error: Incorrect Input.");
		return 0;
	}

}

//Free shared memory

void free_sharedMem(){

	shmctl(smid,IPC_RMID,NULL);
}

int main(void){


	printf("\n~~InterProcess Communications and Mandebrot Images\n~~");
	printf("~~By Bryan Mendoza (bmendo6)~~\n");

	pid_t reaped;
	int status, exec_check, pipe_check, rc, MAX_mem;
	size_t size = 0;

	int run = 1;

	char *new_problem = NULL, *filenames = NULL;	

	char *xmin = NULL, *xmax = NULL, *ymin = NULL, *ymax = NULL;
		
	double Xmin, Xmax, Ymin, Ymax;
	
	char *maxIter = NULL, *nCols = NULL, *nRows = NULL;

	int C, R, check_input = 0, stringLen = 0, MaxIter;

	char *newString;

	//Install signal handlers

	signal(SIGCHLD, handler);
	signal(SIGINT, handler);

	const int structSize = sizeof(MessageReceive)-sizeof(long int);	

	//executable names

	char * argv_1[4] = {"./mandelcalc"};
	char * argv_2[5] = {"./mandelDisplay"};


	//Create Pipes


	int fdp1[2];
	int fdp2[2];

	pipe_check = pipe(fdp1);
	if(pipe_check < 0){
		fprintf(stderr, "Error calling pipe.\n");
		exit(-1);
	}

	pipe_check = pipe(fdp2);
	if(pipe_check < 0){
		fprintf(stderr, "Error calling pipe.\n");
		close(fdp1[0]);
		close(fdp1[1]);
		exit(-1);
	}


	//Create message Q

	char mqID1[12];

	char mqID2[12];

	mqid1 = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);

	if(mqid1 < 0){
		fprintf(stderr,"Error with msgget.\n");
		exit(-1);
	}
	
	mqid2 = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);

	if(mqid2 < 0){
		fprintf(stderr,"Error with msgget.\n");
		msgctl(mqid1,IPC_RMID, NULL);
		exit(-1);
	}
	

	//Get message Q ids into string

	snprintf(mqID1,12,"%d",mqid1);

	snprintf(mqID2,12,"%d",mqid2);

	//Create Shared Mememory

	char smID[12];

	MAX_mem = 1024 * 1024 * sizeof(int);

	smid = shmget(IPC_PRIVATE,MAX_mem,0600|IPC_CREAT);	

	if(smid < 0){
		fprintf(stderr,"Error with shmget.\n");
		msgctl(mqid1,IPC_RMID, NULL);
		msgctl(mqid2,IPC_RMID, NULL);
		exit(-1);
	}

	snprintf(smID,12,"%d",smid);

	//Fill in children command line arguments
		
	argv_1[1] = mqID1;
	argv_2[1] = mqID1;

	argv_1[2] = smID;
	argv_2[2] = mqID2;

	argv_2[3] = NULL;
	argv_2[3] = smID;

	argv_2[4] = NULL;

	//Create new child process

	child1 = fork();

	if (child1 < 0){
		fprintf(stderr, "Error calling Fork.\n");
		release_messageQ(mqid1, mqid2);
		free_sharedMem();
		exit(-1);
	}
	
	//Child 1 does work here

	if (child1 == 0){

		close(fdp1[1]);
		close(fdp2[0]);
		dup2(fdp1[0],0);
		dup2(fdp2[1],1);
		close(fdp1[0]);
		close(fdp2[1]);

		execvp(argv_1[0],argv_1);

		//If problem with Execvp
		release_messageQ(mqid1, mqid2);
		free_sharedMem();	
		exit(-1);
	}

	//Parent continues work here

	else{
		
		child2 = fork();

		if (child2 < 0){
			fprintf(stderr, "Error calling Fork.\n");
			kill(child1, SIGUSR1);
			release_messageQ(mqid1, mqid2);
			free_sharedMem();
			exit(-1);
		}

		//Child 2 does work here
		if (child2 == 0){
			
			close(fdp1[0]);
			close(fdp1[1]);
			close(fdp2[1]);
			dup2(fdp2[0],0);
			close(fdp2[0]);

			exec_check = execvp(argv_2[0],argv_2);
			//If problem with Execvp
			kill(child1, SIGUSR1);
			release_messageQ(mqid1, mqid2);
			free_sharedMem();
			exit(-1);
		}
		//Parent resumes work
		else{	
			//Close parent pipes
			close(fdp1[0]);
			close(fdp2[0]);
			close(fdp2[1]);

			//While True
			while(run)
			{

				//Ask user to input new problem
				while(check_input == 0){
				
					printf("\nDo you want to enter a new Mandelbrot problem (yes/no)? ");
					getline(&new_problem, &size, stdin);
					new_problem[strlen(new_problem)-1] = '\0';
					check_input = check_command(new_problem);

					//User enters no
					if(check_input == 2){
						//Run = 0, quit loop
						run = 0;
					}
				}	

				//User enter yes, run = 1
				if(run){
	
					
					//Check user input for new problem

					printf("\nEnter Filename: ");

					getline(&filenames,&size,stdin);

					check_input = 0;
					
					while(check_input == 0){

						while(check_input == 0){
							printf("\nEnter XMin: ");
							getline(&xmin,&size,stdin);
							check_input = check_X(xmin,&Xmin);
						}

						check_input = 0;

						while(check_input == 0){
							printf("\nEnter XMax: ");
							getline(&xmax,&size,stdin);
							check_input = check_X(xmax,&Xmax);
						}
	
					//	if(Xmin >= Xmax){
					//		check_input = 0;
					//		fprintf(stderr,"\nXmin should be smaller than Xmax\n");
					//	}

					}

					check_input = 0;

					while(check_input == 0){

						while(check_input == 0){
							printf("\nEnter YMin: ");
							getline(&ymin,&size,stdin);
							check_input = check_Y(ymin,&Ymin);
						}

						check_input = 0;

						while(check_input == 0){
							printf("\nEnter YMax: ");
							getline(&ymax,&size,stdin);
							check_input = check_Y(ymax,&Ymax);
						}

					//	if(Ymin >= Ymax){
					//		check_input = 0;
					//		fprintf(stderr,"\nYmin should be smaller than Ymax\n");
					//	}
					}

					check_input = 0;

					//Enter number of Rows

					while(check_input == 0){
						printf("\nEnter number of Rows: ");
						getline(&nRows,&size,stdin);
						check_input = check_rows_cols(nRows,&R);
					}				

					check_input = 0;

					//Enter number of Columns

					while(check_input == 0){
						printf("\nEnter number of Columns: ");
						getline(&nCols,&size,stdin);
						check_input = check_rows_cols(nCols,&C);
					}				

					check_input = 0;

					//Enter Max Iterations

					while(check_input == 0){
						printf("\nEnter max Iterations: ");
						getline(&maxIter,&size,stdin);
						check_input = check_maxIter(maxIter,&MaxIter);
					}				

					check_input = 0;

					//After all input has been checked get the max length of all the input 
					// Strings
		
					stringLen = pipe_string_len(xmin,xmax,ymin,ymax,maxIter,nCols,nRows);

					//Create concatenated string of input to pass to pipe

					newString = new_pipe_string(xmin,xmax,ymin,ymax,maxIter,nCols,nRows,stringLen);

					
					//Prepare filename to be sent to Message Q

					int filelen = strlen(filenames);
					filenames[filelen-1] = '\0';

					//Create Message
					MessageStruct *message = malloc(sizeof(MessageStruct) + filelen-1);
					message->mtype = 1;
					strcpy(message->filename,filenames);

					//send file name
					rc = msgsnd(mqid2,message,filelen,0);

					if(rc == -1){
						fprintf(stderr,"Error sending message.\n");	
						handler(SIGINT);
					}

					//Next write input string to pipe

					int i;

					for(i=0; i < stringLen; i++){

						write(fdp1[1],&newString[i],1);

					}


					//We are done with pipe string
					free(newString);

					
					//Wait for message Q's messages from Children


					MessageReceive *msgres = malloc(sizeof(MessageReceive));

					int numBytes, numBytes2;

					numBytes = msgrcv(mqid1,msgres,structSize,0,0);

					if(numBytes < 0){
						fprintf(stderr,"Error: receiving message\n");
						handler(SIGINT);
					}
	
					numBytes2 = msgrcv(mqid1,msgres,structSize,0,0);	
		
					if(numBytes2 < 0){
						fprintf(stderr,"Error: receiving message\n");
						handler(SIGINT);	
					}	

					//Free message structs
					free(message);
					free(msgres);

				}
				
				//User is done
				else{

					//Disable SIGCHLD handler

					signal(SIGCHLD,SIG_DFL);

					//Send SIGUSR1 signals to terminate children processes
	
					kill(child1,SIGUSR1);
					kill(child2,SIGUSR1);

					//Wait for and collect children

					while ((reaped = wait(&status)) > 0){

						if(WIFEXITED(status)){
							printf("Child %d exit: %d.\n",reaped,WEXITSTATUS(status));
						}

						//Free resources

						release_messageQ(mqid1, mqid2);
						free_sharedMem();

					}

				}


			}

		}

	}

}

