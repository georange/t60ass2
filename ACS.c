/**
   Georgia Ma
   V00849447
   CSC 360
   Assignment 2
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#define SLEEP_TIME_CONVERSION 100000
#define TIME_CONVERSION 0.1
#define MAX_FILE 1024
#define MAX_INPUT 256

// customer struct to keep track of each customer in the queue
typedef struct customer {
	int id;
	float arrival_time;
	float service_time;
	int class;	// 0 for Economy, 1 for Business
	struct customer* next;
} customer;


/** Global Variables **/

// queue heads for customer lines (business is the higher priority) and amount of customers in each line
struct customer business_queue;
struct customer economy_queue;
struct customer* all_customers[MAX_INPUT];
int business_count = 0;
int economy_count = 0;

// lists for time waited and amount of customers in each line (index)
float business_time[MAX_INPUT];
int b_i = 0;
float economy_time[MAX_INPUT];
int e_i = 0;

// threads, mutex, and convar
//pthread_t clerks[4];
pthread_t customers[MAX_INPUT];
pthread_mutex_t mutex;
pthread_cond_t convar;

// status variables
int clerks[4] = {-1, -1, -1, -1};		// for telling which clerk is serving which customer

//int serving = -1; 	// for telling which clerk is serving a queue, -1 means no clerk is serving
//int busy = 0;		// for telling when a queue is busy being served by a clerk
//int served = 0;		// for telling when a customer has left a queue and is being served (to stop multiple customers leaving the queue)

int total = 0; 		// total customers


/** Queue Functions **/

// inserts a customer into a queue, returns size of queue
void enqueue(int id, float arrival_time, float service_time, int class) {
	struct customer* queue_head;
	int count = 0;

	if (class == 0) {
		queue_head = (struct customer*)&economy_queue;
		economy_count++;
		count = economy_count;
	} else if (class == 1) {
		queue_head = (struct customer*)&business_queue;
		business_count++;
		count = business_count;
	} else {
		fprintf(stderr, "Error: invalid class.\n");
		exit(1);
	}

	if (!queue_head) {
		//printf("RUN\n");
		queue_head = (struct customer*)malloc(sizeof(struct customer));

		queue_head->id = id;
		queue_head->arrival_time = arrival_time;
		queue_head->service_time = service_time;
		queue_head->class = class;
		queue_head->next = NULL;
		//printf("%d %d %f %f\n",queue_head->id, queue_head->class, queue_head->arrival_time, queue_head->service_time);
	} else {
		//printf("RUN\n");
		struct customer *curr = queue_head;
		while (curr->next != NULL) {
			curr = curr->next;
		}

		curr->next = (struct customer*)malloc(sizeof(struct customer));
		curr->next->id = id;
		curr->next->arrival_time = arrival_time;
		curr->next->service_time = service_time;
		curr->next->class = class;
		curr->next->next = NULL;
		//printf("%d %d %f %f\n",curr->next->id, curr->next->class, curr->next->arrival_time, curr->next->service_time);
	}
	
	printf("A customer enters a queue: the queue ID %1d, and length of the queue %2d. \n", class, count);
	return;
}

// deletes a customer from the start of a queue
void dequeue(int class) {
	struct customer* queue_head;
	int count;

	if (class == 0) {
		queue_head = (struct customer*)&economy_queue;
		economy_count--;
		count = economy_count;
	} else if (class == 1) {
		queue_head = (struct customer*)&business_queue;
		business_count--;
		count = business_count;
	} else {
		fprintf(stderr, "Error: invalid class.\n");
		exit(1);
	}

	if (queue_head) {
		queue_head = queue_head->next;
	}
	
	printf("A customer leaves a queue: the queue ID %1d, and length of the queue %2d. \n", class, count);
	return;
}


/** Input Parsing Function **/

// reads input file and puts information into the enqueue function
void set_up_customers(char* to_read) {
	FILE* input = fopen(to_read, "r");
	if (!input)	{
		printf("Error: could not open input file. \n");
		exit(1);
	}
	char buffer[MAX_FILE];
	char contents[MAX_FILE][MAX_FILE];
	int i = 0;
	while (fgets(buffer, MAX_FILE-1, input)) {
		strncpy(contents[i], buffer, strlen(buffer)+1);
		i++;
	}
	fclose(input);

	// parse first line for total number of customers
	total = atoi(contents[0]);
	if (total < 0) {
		printf("Error: invalid value in input file.");
		exit(1);
	}

	// add content to all_customers
	int j = 0;
	for (i = 1; i < total+1; i++) {
		struct customer* temp = (struct customer*)malloc(sizeof(struct customer));

		char* token = strtok(contents[i], ":");
		temp->id = atoi(token);

		token = strtok(NULL, ",");
		temp->class = atoi(token);

		token = strtok(NULL, ",");
		temp->arrival_time = atoi(token);

		token = strtok(NULL, ",");
		temp->service_time = atoi(token);

		all_customers[j] = temp;
		j++;
		
		if (temp->id<0 || temp->class<0 || temp->arrival_time<0 || temp->service_time < 0) {
			printf("Error: invalid value in input file.");
			exit(1);
		}

		//printf("%d %d %f %f\n",temp->id, temp->class, temp->arrival_time, temp->service_time);

		//enqueue(temp->id, temp->arrival_time, temp->service_time, temp->class);
		//free(temp);
	}
}

// testing function for printing out all the current queues
void print_queues() {
	printf("All Customers: \n");
	int i;
	for (i = 0; i < total; i++) {
		printf("%d %d %f %f\n",all_customers[i]->id, all_customers[i]->class, all_customers[i]->arrival_time, all_customers[i]->service_time);
	}

	printf ("Business queue: \n");
	struct customer* curr = business_queue.next;
	while (curr != NULL) {
		printf("%d %d %f %f\n",curr->id, curr->class, curr->arrival_time, curr->service_time);
		curr = curr->next;
	}

	printf ("Economy queue: \n");
	curr = economy_queue.next;
	while (curr != NULL) {
		printf("%d %d %f %f\n",curr->id, curr->class, curr->arrival_time, curr->service_time);
		curr = curr->next;
	}
}


/** Thread Functions **/


// returns the time difference between time started and time now 
float get_time_from(struct timeval start) {
	struct timeval curr;
	gettimeofday(&curr, NULL);
	int time_conversion = 10*SLEEP_TIME_CONVERSION;
	
	long curr_microseconds = (curr.tv_sec * time_conversion) + curr.tv_usec;
	long start_microseconds = (start.tv_sec * time_conversion) + start.tv_usec;
	return (float)(curr_microseconds - start_microseconds) / (time_conversion);
}

// check if a clerk is open
int check_clerks() {
	if (clerks[0] == -1) {
		return 1;
	} 
	if (clerks[1] == -1) {
		return 1;
	}
	if (clerks[2] == -1) {
		return 1;
	}
	if (clerks[3] == -1) {
		return 1;
	}
	
	return 0;
}

// if a clerk is open, take the spot
int get_clerk (customer* c) {
	if (clerks[0] == -1) {
		clerks[0] = c->id;
		return 1;
	}
	if (clerks[1] == -1) {
		clerks[1] = c->id;
		return 1;
	}
	if (clerks[2] == -1) {
		clerks[2] = c->id;
		return 1;
	}
	if (clerks[3] == -1) {
		clerks[3] = c->id;
		return 1;
	}
	
	return 0;
}

struct timeval get_service(customer* c) {
	struct timeval start;
	pthread_mutex_lock(&mutex);	

	enqueue(c->id, c->arrival_time, c->service_time, c->class);
	// start keeping time once customer has entered the queue
	gettimeofday(&start, NULL);
	
	if (c->class == 0) {
		// if I'm at the front of economy queue and business queue is empty and there's a free clerk, take the position and leave the queue
		/**if (check_clerks() && !business_queue.id && economy_queue.id == c->id) {
			get_clerk(c);
			dequeue(c->class);
			pthread_mutex_unlock(&mutex); 
			return start;
		}**/
		// otherwise, I wait for the conditions to be right
		while (business_queue.next != NULL && economy_queue.next->id != c->id && !check_clerks()) {
			pthread_cond_wait(&convar, &mutex);
		}
	} else if (c->class == 1) {
		// if I'm at the front of business queue and there's a free clerk, take the position and leave the queue
		/**if (check_clerks() && business_queue.id == c->id) {
			get_clerk(c);
			dequeue(c->class);
			pthread_mutex_unlock(&mutex); 
			return start;
		}**/
		// otherwise I wait for the conditions to be right
		while (business_queue.next->id != c->id && !check_clerks()) {
			pthread_cond_wait(&convar, &mutex);
		}
	} else {
		fprintf(stderr, "Error: invalid class.\n");
		exit(1);
	}
	
	get_clerk(c);
	dequeue(c->class);
	pthread_mutex_unlock(&mutex); 
	
	return start;
}

void release_service(int clerk) {
	pthread_mutex_lock(&mutex);
	
	clerks[clerk-1] = -1;
	
	pthread_mutex_unlock(&mutex);
	pthread_cond_broadcast(&convar);
	
}

// customer threads
void* customer_thread_function(void* temp) {
	customer* c = (customer*)temp;
	struct timeval start;
    float time_taken;
	int clerk = -1;
	
	// wait for customer to arrive on time
	usleep(c->arrival_time * SLEEP_TIME_CONVERSION);
	printf ("A customer arrives: customer ID %2d. \n", c->id);
	
	// get served by a clerk
	start = get_service(c);
	
	// find out which clerk is serving me
	int i;
	for (i = 0; i < 4; i++) {
		if (clerks[i] == c->id) {
			clerk = i+1;
		}
	}
	
	time_taken = get_time_from(start);
	// print at start of service 
	printf("A clerk starts serving a customer: start time %.2f, the customer ID %2d, the clerk ID %1d. \n", time_taken, c->id, clerk);
    
	time_taken = get_time_from(start);
	if (c->class == 1) {
		business_time[b_i] = time_taken;
		b_i++;
	} else {
		economy_time[e_i] = time_taken;
		e_i++;
	}
	
	// wait for customer to finish being served
	usleep(c->service_time * SLEEP_TIME_CONVERSION);
	
	time_taken = get_time_from(start);
	// print at end of service 
	printf("A clerk finishes serving a customer: end time %.2f, the customer ID %2d, the clerk ID %1d. \n", time_taken, c->id, clerk);
	
	// finish getting served by a clerk
	release_service(clerk);
	
	pthread_exit(NULL);
}
/*
// clerk threads
void* clerk_thread_function(void* i) {
	int clerk_id = *((int *) i);
	clerk_id++;
    free(i);
	
	
	pthread_exit(NULL);
} */

int main(int argc, char* argv[]) {

	if (argc != 2) {
		fprintf(stderr, "Error: please include an input file name.\n");
		exit(1);
	}

	set_up_customers(argv[1]);
	//print_queues();					

	// initialization of mutex, convar, attr, and detachstate
	if (pthread_mutex_init(&mutex, NULL) != 0) {
		printf("Error: mutex initialization fail.\n");
		exit(1);
	}
	if (pthread_cond_init(&convar, NULL) != 0) {
		printf("Error: convar initialization fail.\n");
		exit(1);
	}

	pthread_attr_t attr;
	if (pthread_attr_init(&attr) != 0) {
		printf("Error: attr initialization fail.\n");
		exit(1);
	}
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0) {
		printf("Error: setdetachstate fail.\n");
		exit(1);
	}

	// set up threads for each customer and clerk
	int i;
	for (i = 0; i < total; i++) {
		if (pthread_create(&customers[i], &attr, customer_thread_function, (void*)all_customers[i]) != 0){
			printf("Error: failed to create customer pthread.\n");
			exit(1);
		}
	}
	/*
	for (i = 0; i < 4; i++) {
		int *arg = malloc(sizeof(*arg));
        if ( arg == NULL ) {
            printf("Error: failed to allocate memory for clerk_id\n");
            exit(1);
        }

        *arg = i;
		if (pthread_create(&clerks[i], &attr, clerk_thread_function, arg) != 0){
			printf("Error: failed to create clerk pthread.\n");
			exit(1);
		}
	} */

	// Wait for all threads to terminate
	for (i = 0; i < total; i++) {
		if (pthread_join(customers[i], NULL) != 0) {
			printf("Error: failed to join pthread.\n");
			exit(1);
		}
	}
	
	// print time averages
	double all_time = 0;
	double b_time = 0;
	double e_time = 0;
	for (i = 0; i < b_i; i++) {
		b_time = b_time + business_time[i];
	}
	for (i = 0; i < e_i; i++) {
		e_time = e_time + economy_time[i];
	}
	all_time = b_time + e_time;
	
	printf("The average waiting time for all customers in the system is: %.2f seconds. \n", all_time/total);
	printf("The average waiting time for all business-class customers is: %.2f seconds. \n", b_time/b_i);
	printf("The average waiting time for all economy-class customers is: %.2f seconds. \n", e_time/e_i);

	// clean up
	if (pthread_mutex_destroy(&mutex) != 0) {
		printf("Error: mutex destroy failed.\n");
		exit(1);
	}
	if (pthread_cond_destroy(&convar) != 0) {
		printf("Error: convar destroy failed.\n");
		exit(1);
	}
	if (pthread_attr_destroy(&attr) != 0) {
		printf("Error: attr destroy failed.\n");
		exit(1);
	}

	exit (0);
}
