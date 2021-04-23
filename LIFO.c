// gcc LIFO.c -lpthread

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

#define SIZE 25  // buffer size (fixed at 25) 
#define TRUE 1
#define FALSE 0

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
    int threadNum;
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
 
void insertbuffer(void *thread_n) {
    int thread_numb = *(int *)thread_n;

    if (buffer_index < SIZE) {
        // new PrintRequest with random size (100-1000 bytes)
        struct printRequest newPrintReq = { (rand() % 1000 + 100), thread_numb };

        // create new node for printRequest
        struct node *newNode = (struct node *) malloc(sizeof(struct node));
        newNode->payload = newPrintReq;
        newNode->next = 0;

        // append newNode to end of list
        tail->next = newNode;
        tail = newNode;
        
        printf("Producer %d added %d to buffer\n", thread_numb, newNode->payload.size );
        buffer_index++;
    } else {
        printf("Buffer overflow\n");
    }
}
 
struct printRequest dequeuebuffer(void *thread_n) {
    int thread_numb = *(int *)thread_n;
    struct printRequest printJob; 
    if (buffer_index > 0) {
        printf("buffer_index = %d\n", buffer_index);

        struct node *temp = head;
        struct node *prev;

        if (temp == NULL) {
            printf("empty queue\n");
            exit(0);
        }
        if (head == tail) { // Only one node in List
            free(head);
            head=NULL;
        }
        while (temp->next != NULL) { // iterate list until temp = last element
            prev = temp;
            temp = temp->next;
        }

        /* process and remove printJob from queue */
        printJob = temp->payload;
        free(prev->next);
        prev->next = NULL;
        tail = prev;

        printf("Consumer [%d] dequeue process [%d] (%d bytes) from buffer\n", thread_numb, printJob.threadNum, printJob.size);
        buffer_index--;

    } else {
        printf("Buffer underflow\n");
        exit(0);
    }
    return printJob;
}
 
int isempty() {
    if (buffer_index == 0)
        return TRUE;
    return FALSE;
}
  
int isfull() {
    if (buffer_index == SIZE)
        return TRUE;
    return FALSE;
}

/* PRODUCER */
void *producer(void *thread_n) {
    int thread_numb = *(int *)thread_n;
    int numPrintJobs = rand() % 25;
    int i=0;
    while (i++ < numPrintJobs) { 
        my_wait(&full_sem); 
        pthread_mutex_lock(&buffer_mutex);

        do {
            pthread_mutex_unlock(&buffer_mutex);
            my_wait(&full_sem); // sem=0: wait. sem>0: go and decrement it
            pthread_mutex_lock(&buffer_mutex);
        } while (isfull()); // check for spurios wake-ups
        
        insertbuffer(thread_n);

        pthread_mutex_unlock(&buffer_mutex);
        my_post(&empty_sem); 

        // delay between succesive print jobs
        double interval = (double)rand() / (double)RAND_MAX;
        printf("sleeping %f sec...\n\n", interval);
        sleep(interval);
    }

    // Now all printRequests have been made, we can flag to consumers production is complete
    flag = true;
    pthread_exit(0);
}
 
/* CONSUMER */
void *consumer(void *thread_n) {
    int thread_numb = *(int *)thread_n;

    while (!flag) {    // loop until all requests are processed. then flag = true
        sleep(1);

        my_wait(&empty_sem);
        my_wait(&shared_sem);
        pthread_mutex_lock(&buffer_mutex);

        dequeuebuffer(thread_n);

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
    
    // initialize first node. Set head and tail pointing to it
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

    // Create producer and consumer threads
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
        pthread_create(&cons_thread[i], NULL, consumer, &cons_thread_numb[i]);  

    }
 
    // join producer threads
    for (i = 0; i < NUM_PRODUCERS; i++)
        pthread_join(prod_thread[i], NULL);
    
    flag = true;  // signal producers are finished

    // join consumer threads
    for (i = 0; i < NUM_CONSUMERS; i++)
        pthread_join(cons_thread[i], NULL);
 

    // deallocate semaphores and mutexbuffer
    pthread_mutex_destroy(&buffer_mutex);
    my_destroy(&full_sem);
    my_destroy(&empty_sem);
    my_destroy(&shared_sem);
 
    return 0;
}
