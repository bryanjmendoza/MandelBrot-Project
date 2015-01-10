#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<signal.h>
#include<stdio.h>
#include<sys/wait.h>
#include<string.h>
#include<sys/msg.h>
#include<sys/ipc.h>
#include<sys/shm.h>

/* mandelcalc.c by Bryan Mendoza (bmendo6) */


int problems = 0;

//Node of linked list
typedef struct node{

	char c;
	struct node* next;	

}Node;


//Linked List Vector
typedef struct vector{

	Node* head;
	Node* tail;
	int size;

}Vector;	


//Create vector
Vector* new_Vector(){

	Vector* vector = malloc(sizeof(Vector));
	vector->head = NULL;
	vector->tail = NULL;
	vector->size = 0;

	return vector;

}

//Add element to vector
void add(Vector* vector, char c){

	Node* node = malloc(sizeof(Node));
	node->c = c;
	node->next = NULL;

	if(vector->head == NULL){
		vector->head = node;
		vector->tail = node;
		vector->size++;
	}
	else{
		vector->tail->next = node;
		vector->tail = node;
		vector->size++;
	}
}

//Free vector elements
void free_vector(Vector * vector){

	Node* temp;
	Node* temp2;

	temp = vector->head;

	while(temp != NULL){
		temp2 = temp->next;
		free(temp);
		temp = temp2;
	}

	vector->head = NULL;
	vector->tail = NULL;
	vector->size = 0;

}


//Convert Vector to Array
char * Vector_To_Array(Vector* vector){

	char *arr = malloc(sizeof(char)*(vector->size));
	Node * temp;
	int index = 0;
	temp = vector->head;
	while(temp != NULL){
		arr[index] = temp->c;
		index++;
		temp = temp->next;
	}

	return arr;
}

//Parse pipe string and update pointers
void parse_input(char* pipe_string, double* xmin, double* xmax, double* ymin, 
		double* ymax, int* maxIter, int* cols, int* rows){


	char* tok;

	int index = 1;
	
	tok = strtok(pipe_string," ");
	
	while(tok != NULL){
		
		if(index == 1){
			sscanf(tok,"%lf",xmin);
		}
		else if(index == 2){
			sscanf(tok,"%lf",xmax);
		}
		else if(index == 3){
			sscanf(tok,"%lf",ymin);
		}
		else if(index == 4){
			sscanf(tok,"%lf",ymax);
		}
		else if(index == 5){
			sscanf(tok,"%d",maxIter);
		}
		else if(index == 6){
			sscanf(tok,"%d",cols);
		}
		else if(index == 7){
			sscanf(tok,"%d",rows);
		}

		index++;
		tok = strtok(NULL," ");
	}



}


//Message Q structs
typedef struct{

	long int mtype;
	char filename[1];

}MessageStruct;



void handler(int sig){

	exit(problems);

}

//Calculate mandelbrot

void calculate_Mandelbrot(int* data,double xmin, double xmax, double ymin, 
			double ymax, int maxIter, int cols, int rows){

	double delta_X = (xmax-xmin)/(cols-1);
	double delta_Y = (ymax-ymin)/(rows-1);
	
	int r,c,n;
	double Cy, Cx, Zx, Zy, Zx_next, Zy_next;

	for(r=0; r < rows; r++){
		Cy = ymin + r * delta_Y;
		for(c=0; c < cols; c++){
			Cx = xmin + c * delta_X;
			Zx = 0.0;
			Zy = 0.0;
			for(n=0; n < maxIter; n++){
				if(Zx * Zx + Zy * Zy >= 4.00)
					break;
				Zx_next = Zx * Zx - Zy * Zy + Cx;
				Zy_next = 2.0 * Zx * Zy + Cy;
				Zx = Zx_next;
				Zy = Zy_next;
			}
			if(n >= maxIter){
				//Add to shared memory
				*(data + r * cols + c) = -1;
			}
			else{
				//Add to shared memory
				*(data + r * cols + c) = n;
			}
		}	
	}  
}


int main(int argc, char* argv[]){


	size_t size = 0;
	char* line = NULL;

	Vector* vector;

	vector = new_Vector();

	char c;

	int r, msgID1,smID,index = 0,i = 0,rc;

	//Parse command line arguments

	sscanf(argv[1],"%d", &msgID1);

	sscanf(argv[2],"%d", &smID);

	signal(SIGUSR1, handler);

	char *msg = "done", *array = NULL, *array2 = NULL;

	double xmin,xmax,ymin,ymax;
	int rows, cols, maxIter, *data, r1;
	
	while(1){

		//Read string from pipe one character at a time

		while(r = (read(0,&c,1)) != 0){
	
			//If string reaches new line character exit loop
			if(c == '\n'){
				add(vector,c);
				break;
			}
			//Add charcter to vector
			add(vector,c);						
		}

		//Get new address space for shared memory

		data = shmat(smID,NULL,0);

		if(data == (int *)-1){
			fprintf(stderr,"Error calling shmat.\n");
			exit(-1);
		}

		//Create new character array
		array = Vector_To_Array(vector);

		array[vector->size-1] = '\0';

		//Parse array and update pointer

		parse_input(array,&xmin,&xmax,&ymin,&ymax,&maxIter,&cols,&rows);

		//Calculate mandelbrot and add to shared memory.

		calculate_Mandelbrot(data,xmin,xmax,ymin,ymax,maxIter,cols,rows);

		//Create new string to pass to second child
	
		array2 = Vector_To_Array(vector);

		//Write string to pipe

		for(i=0; i < vector->size; i++){
			write(1,&array2[i],1);
		}

		//release address space
				
		r1 = shmdt(data);

		if(r1 == -1){
			fprintf(stderr,"Error calling shmdt.\n");
			exit(-2);
		}

		//free vector and array resources

		free_vector(vector);

		free(array);

		free(array2);

		//New problem finished
		problems++;

		//Send done message
		MessageStruct *message = malloc(sizeof(MessageStruct) + strlen(msg));
		message->mtype = 1;
		strcpy(message->filename,msg);
	
		rc = msgsnd(msgID1,message,(strlen(msg)+1),0);


		if(rc == -1){
			fprintf(stderr,"Error sending message.\n");
			exit(-3);
		}

		free(message);

	}

}
