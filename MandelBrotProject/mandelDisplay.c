#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<sys/msg.h>
#include<sys/ipc.h>
#include<sys/types.h>
#include<signal.h>
#include<sys/shm.h>

/*

MandelDisplay.c by Bryan Mendoza
  (bmendo6)

*/



int problems = 0;

//Linked list node
typedef struct node{

	char c;
	struct node* next;	

}Node;

//Linked list struct
typedef struct vector{

	Node* head;
	Node* tail;
	int size;

}Vector;	

//Create new vector
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


//Message Q structs

typedef struct{

	long int mtype;
	char message[60];


}MessageReceive;


typedef struct{

	long int mtype;
	char filename[1];

}MessageStruct;


void handler(int sig){

	exit(problems);

}


void print_vector(Vector * vector){

	Node* temp;

	temp = vector->head;

	while(temp != NULL){
		printf("%c", temp->c);
		temp = temp->next;
	}
}


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

	char *arr = malloc(sizeof(char)*(vector->size+1));
	Node * temp;
	int index = 0;
	temp = vector->head;
	while(temp != NULL){
		arr[index] = temp->c;
		index++;
		temp = temp->next;
	}
	arr[index] = '\0';

	return arr;
}

//parse pipe string
void parse_input(char* pipe_string, double* xmin, double* xmax, double* ymin,
		 double* ymax, int* cols, int* rows){

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

//Display MandelBrot image, retrieving values from shared memory

void display_mandelbrot(FILE* file, int* data, double xmin, double xmax, 
			double ymin, double ymax, int rows, int cols){


	printf("Y_Min: %lf, Y_Max: %lf\n",ymin,ymax);
	
	char colors[] = ".-~:+*%O8&?$@#X";
	int ncolors = 15;
	int r,c,n;
	for(r=rows-1; r >= 0; r--){
		for(c=0; c < cols; c++){
			n = *(data + r * cols + c);
			if(n < 0){
				printf(" ");
				if(file != NULL){
					fprintf(file,"%c%c",' ',' ' );
				}
			}
			else{
				printf("%c",colors[n % ncolors]);
				if(file != NULL){
					fprintf(file,"%c%c",colors[n % ncolors],' ');
				}
			}
		}
		printf("\n");
		if(file != NULL){
			fprintf(file,"%c",'\n');
		}
	}
		
	printf("X_Min: %lf, X_Max: %lf\n",xmin,xmax); 
		
}


int main(int arc, char * argv[]){

	signal(SIGUSR1, handler);

	const int structSize = sizeof(MessageReceive)-sizeof(long int);	

	int msgID1, msgID2, smID;

	FILE *file;

	char *line = NULL;
	size_t size = 0;
	
	Vector *vector;

	vector = new_Vector();

	char* array = NULL;

	//Parse command line arguments

	sscanf(argv[1],"%d", &msgID1);
	sscanf(argv[2],"%d", &msgID2);
	sscanf(argv[3],"%d", &smID);

	int rc, numBytes, r, i;

	char* msg = "done";

	char c;

	double xmin, xmax, ymin, ymax;
	int rows, cols, *data, r1;

	//While true

	while(1){

		//Read from pipe

		while(r = (read(0,&c,1)) != 0){	
			//If we read a new line then break loop		
			if(c == '\n'){
				break;
			}
			//Add character to vector
			add(vector,c);
		}

		//Check messgae q for filename
		MessageReceive *msgres = malloc(sizeof(MessageReceive));

		int numBytes;

		numBytes = msgrcv(msgID2,msgres,structSize,1,0);

		if(numBytes == -1){
			fprintf(stderr,"Error sending message.\n");
			exit(-1);
		}

			
		//Open file for writting

		file = fopen(msgres->message,"r+");

		if(file == NULL){

			fprintf(stderr,"Error: File not opened\n");

		}

		free(msgres);

		array = Vector_To_Array(vector);

		printf("\n\n");

		//Prepare shared memory for reading

		data = shmat(smID,NULL,0);

		if(data == (int*)-1){
			fprintf(stderr,"Error calling shmat.\n");
			exit(-2);
		}

		//Parse Array to get arguments

		parse_input(array,&xmin,&xmax,&ymin,&ymax,&cols,&rows);

		//Read from shared memory and print MandelBrot Image to screen and
		//write to file if it exists

		display_mandelbrot(file,data, xmin, xmax, ymin, ymax, rows,cols);

		//Close file
		if(file != NULL){
			fclose(file);
		}

		printf("\n");

		r1 = shmdt(data);

		if(r1 == -1){
			fprintf(stderr,"Error calling shmdt.\n");
			exit(-3);
		}
	
		free_vector(vector);
		
		free(array);

		//New problem done

		problems++;

		//Send done message

		MessageStruct *message = malloc(sizeof(MessageStruct) + strlen(msg));
		message->mtype = 1;
		strcpy(message->filename,msg);
	
		rc = msgsnd(msgID1,message,(strlen(msg)+1),0);

		if(rc == -1){
			fprintf(stderr,"Error sending message.\n");
			exit(-4);
		}

		free(message);


	}
}



