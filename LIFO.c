// gcc LIFO.c -lpthread

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

#define SIZE 25  // buffer size (fixed at 25) 

int NUM_CONSUMERS;
int NUM_PRODUCERS;
bool flag; // true when producers are done

//TESTING STUFF
typedef int buffer_t;  
buffer_t buffer[SIZE]; 
int buffer_index;     
pthread_mutex_t buffer_mutex;

struct CSem{
    int val; // value of Csem
    sem_t B1; // gate
    sem_t B2; // mutex
};

struct CSem full_sem;  /* when 0, buffer is full */
struct CSem empty_sem; /* when 0, buffer is empty. Kind of like an index for the buffer */
struct CSem shared_sem; /* prevents reading/writing the same element  */

struct printRequest{
    int size; // in bytes
};

struct node {
    struct printRequest payload; 
    struct node *next;
};

struct node *head, *tail; // point to first and last node in Linked List (global buffer)



/**********************
    HELPER FUNCTIONS
**********************/

/* min() helper function -> finds smaller of 2 ints*/
int min(int num1, int num2) 
{
    return (num1 > num2 ) ? num2 : num1;
}

// initialize semaphore. input pointer and key value
void my_init(struct CSem *cs, int key ){
    cs->val = key; 
    int m = min(1,key);
    sem_init(&cs->B1, 0, m); // 1 if (val > 0); 0 if (val = 0)
    sem_init(&cs->B2, 0, 1);
}

// Pc()
void my_wait(struct CSem *cs){
    sem_wait(&cs->B1); // P(gate)
    sem_wait(&cs->B2); // P(mutex)
    cs->val = cs->val - 1;
    if(cs->val > 0){
        sem_post(&cs->B1); // V(gate)
    }
    sem_post(&cs->B2); // V(mutex)
}

// Vc()
void my_post(struct CSem *cs){
    sem_wait(&cs->B2); // P(mutex)
    cs->val = cs->val + 1;
    if(cs->val = 1){
        sem_post(&cs->B1); // V(gate)
    }
    sem_post(&cs->B2); // V(mutex)
}

void my_destroy(struct CSem *cs){
    sem_destroy(&cs->B1);
    sem_destroy(&cs->B2);
}
 
void insertbuffer(buffer_t value) {
    if (buffer_index < SIZE) {
        buffer[buffer_index++] = value;
    } else {
        printf("Buffer overflow\n");
    }
}
 
buffer_t dequeuebuffer() {
    if (buffer_index > 0) {
        return buffer[--buffer_index]; // buffer_index-- would be error!
    } else {
        printf("Buffer underflow\n");
    }
    return 0;
}
 


// user process create print jobs.
//     -need struct for printRequest
//     -add pR to global queue
void *producer(void *thread_n) {
    buffer_t value;//testing
    int thread_numb = *(int *)thread_n;
    int numPrintJobs = rand() % 25;


    int i=0;
    while (i++ < numPrintJobs) { 

        my_wait(&full_sem); 
        pthread_mutex_lock(&buffer_mutex);
  


        // new PrintRequest with random byte size
        int randomByteSize = ( rand() % 1000 + 100 );
        struct printRequest newPrintReq = {randomByteSize};

        // create new node for printRequest
        struct node *newNode = (struct node *) malloc(sizeof(struct node));
        newNode->payload = newPrintReq;
        newNode->next = 0;

        // append newNode to end of list
        tail->next = newNode;
        tail = newNode;
        


        pthread_mutex_unlock(&buffer_mutex);
        my_post(&empty_sem); 

        printf("Producer %d added %d to buffer\n", thread_numb, newNode->payload.size );

        // delay between succesive print jobs
        double interval = (double)rand() / (double)RAND_MAX;
        sleep(interval);
    }

    // Now all printRequests have been made, we can flag to consumers production is complete
    flag = true;
    pthread_exit(0);
}
 
void *consumer(void *thread_n) {
    int thread_numb = *(int *)thread_n;
    int i=0;

    while (!flag) {    // loop until all requests are processed. then flag = true

        sleep(1);

        my_wait(&empty_sem);
        my_wait(&shared_sem);
        pthread_mutex_lock(&buffer_mutex);

        struct node *temp = head;
        struct node *prev;

        if (temp == NULL) {
            printf("empty queue\n");
            exit(0);
        }
        // Only one node in List
        if (head == tail) {
            free(head);
            head=NULL;
        }
        // iterate list to get last element
        while (temp->next != NULL) {
            prev = temp;
            temp = temp->next;
        }

        // process and remove printJob from queue
        struct printRequest printJob = temp->payload;
        free(prev->next);
        prev->next = NULL;
        tail = prev;

        printf("Consumer %d dequeue (%d bytes) from buffer\n", thread_numb, printJob.size);




        pthread_mutex_unlock(&buffer_mutex);
        my_post(&shared_sem);
        my_post(&full_sem);
    }
    pthread_exit(0);
}
 
int main(int argc, int *argv[]) {
    srand(time(NULL)); // call once for RNG

    // verify arguments exist
    if (argc != 3) {
        printf("not enough command line args");
        exit(0);
    }
    
    // true when producers are finished
    flag = false;

    // recieve number of producers/consumers from command line args
    NUM_PRODUCERS = atoi(argv[1]);
    NUM_CONSUMERS = atoi(argv[2]);
    printf("num_producers: %d\n",NUM_PRODUCERS);
    printf("num_consumers: %d\n",NUM_CONSUMERS);
    
    // Initialize first node. Set head and tail pointing to it
    struct node *root = (struct node *) malloc(sizeof(struct node));
    root->next = 0;
    head = root;
    tail = root;
    
    // initialize mutex buffer
    buffer_index = 0;
    pthread_mutex_init(&buffer_mutex, NULL);

    // custom implementations of sem_init
    my_init(&full_sem, SIZE);
    my_init(&empty_sem, 0);
    my_init(&shared_sem, NUM_CONSUMERS);

    // Setup producer processes and consumer threads
    pthread_t prod_thread[NUM_PRODUCERS];
    int prod_thread_numb[NUM_PRODUCERS];
    int i;
    for(i=0; i < NUM_PRODUCERS; i++) {
        prod_thread_numb[i] = i;
        pthread_create(prod_thread + i,  NULL,  producer,  prod_thread_numb + i);  
    }

    pthread_t cons_thread[NUM_CONSUMERS];
    int cons_thread_numb[NUM_CONSUMERS];
    for(i=0; i<NUM_CONSUMERS; i++) {
        cons_thread_numb[i] = i;
        // playing a bit with thread and thread_numb pointers...
        pthread_create(&cons_thread[i], NULL, consumer, &cons_thread_numb[i]);  

    }
 

    for (i = 0; i < NUM_PRODUCERS; i++)
        pthread_join(prod_thread[i], NULL);

    flag = true; // producers are done

    for (i = 0; i < NUM_CONSUMERS; i++)
        pthread_join(cons_thread[i], NULL);
 

    pthread_mutex_destroy(&buffer_mutex);
    my_destroy(&full_sem);
    my_destroy(&empty_sem);
    my_destroy(&shared_sem);
 
    return 0;
}
