#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>


/* Queue Structures */

typedef struct queue_node_s {
  struct queue_node_s *next;
  struct queue_node_s *prev;
  char c;
} queue_node_t;

typedef struct {
  struct queue_node_s *front;
  struct queue_node_s *back;
  pthread_mutex_t lock;
} queue_t;


/* Thread Function Prototypes */
void *producer_routine(void *arg);
void *consumer_routine(void *arg);


/* Global Data */
long g_num_prod; /* number of producer threads */
pthread_mutex_t g_num_prod_lock;
//Bug3. Fix. Made count a global variable and created a mutex for it.
long count = 0;
pthread_mutex_t count_lock;


/* Main - entry point */
int main(int argc, char **argv) {
  queue_t queue;
  pthread_t producer_thread, consumer_thread;
  void *thread_return = NULL;
  int result = 0;

  /* Initialization */

  printf("Main thread started with thread id %x\n", (unsigned int)pthread_self());

  memset(&queue, 0, sizeof(queue));
  pthread_mutex_init(&queue.lock, NULL);

  g_num_prod = 1; /* there will be 1 producer thread */

  /* Create producer and consumer threads */

  result = pthread_create(&producer_thread, NULL, producer_routine, &queue);
  if (0 != result) {
    fprintf(stderr, "Failed to create producer thread: %s\n", strerror(result));
    exit(1);
  }

  printf("Producer thread started with thread id %x\n", (unsigned int) producer_thread);
/* Bug1 - a detached process cannot be joined, which the code attempts later. Commenting this detach takes care of the "failed to join" error.*/
 /* result = pthread_detach(producer_thread);
  if (0 != result)
    fprintf(stderr, "Failed to detach producer thread: %s\n", strerror(result));
*/
  result = pthread_create(&consumer_thread, NULL, consumer_routine, &queue);
  if (0 != result) {
    fprintf(stderr, "Failed to create consumer thread: %s\n", strerror(result));
    exit(1);
  }

  //Own code to test something.
  //printf("Consumer thread %x started from %x.\n",(unsigned int) consumer_thread, (unsigned int) pthread_self());

  /* Join threads, handle return values where appropriate */

  result = pthread_join(producer_thread, NULL);
  if (0 != result) {
    fprintf(stderr, "Failed to join producer thread: %s\n", strerror(result));
    pthread_exit(NULL);
  }

  result = pthread_join(consumer_thread, &thread_return); 
  if (0 != result) {
    fprintf(stderr, "Failed to join consumer thread: %s\n", strerror(result));
    pthread_exit(NULL);
  }
  printf("\nPrinted %lu characters.\n", (long unsigned) thread_return); //Bug2. Unnecessary cast to pointer of the thread_return. Removing the "*" results in the correct referencing.
  //Bug2. Not sure if this is a bug but malloc was never used to allocate memory to thread_return so using free() is resulting in SIGSEV.
  //free(thread_return);

  pthread_mutex_destroy(&queue.lock);
  pthread_mutex_destroy(&g_num_prod_lock);
  return 0;
}


/* Function Definitions */

/* producer_routine - thread that adds the letters 'a'-'z' to the queue */
void *producer_routine(void *arg) {
  queue_t *queue_p = arg;
  queue_node_t *new_node_p = NULL;
  pthread_t consumer_thread;
  int result = 0;
  char c;

  result = pthread_create(&consumer_thread, NULL, consumer_routine, queue_p);
  if (0 != result) {
    fprintf(stderr, "Failed to create consumer thread: %s\n", strerror(result));
    exit(1);
  }

  //Own code to make output more informative.
  printf("Consumer thread %x started from producer thread %x.\n", (unsigned int) consumer_thread, (unsigned int) pthread_self());

  result = pthread_detach(consumer_thread);
  if (0 != result)
    fprintf(stderr, "Failed to detach consumer thread: %s\n", strerror(result));

  for (c = 'a'; c <= 'z'; ++c) {

    /* Create a new node with the prev letter */
    new_node_p = malloc(sizeof(queue_node_t));
    new_node_p->c = c;
    new_node_p->next = NULL;

    /* Add the node to the queue */
    pthread_mutex_lock(&queue_p->lock);
    if (queue_p->back == NULL) {
      assert(queue_p->front == NULL);
      new_node_p->prev = NULL;

      queue_p->front = new_node_p;
      queue_p->back = new_node_p;
    }
    else {
      assert(queue_p->front != NULL);
      new_node_p->prev = queue_p->back;
      queue_p->back->next = new_node_p;
      queue_p->back = new_node_p;
    }
    pthread_mutex_unlock(&queue_p->lock);

    //Own
    //printf("Pushing producer to end of queue.\n");
    sched_yield();
  }

  /* Decrement the number of producer threads running, then return */
  //Bug4. Did not lock mutex for g_num_prod before changing it. Fixed by locking and unlocking the mutex.
  pthread_mutex_lock(&g_num_prod_lock);
  --g_num_prod;
  pthread_mutex_unlock(&g_num_prod_lock);
  return (void*) 0;
}


/* consumer_routine - thread that prints characters off the queue */
void *consumer_routine(void *arg) {
  queue_t *queue_p = arg;
  queue_node_t *prev_node_p = NULL;
  //Bug3. Fix. Made count global variable to print the correct number of character count.
  //long count = 0; /* number of nodes this thread printed */

  printf("Consumer thread started with thread id %x\n", (unsigned int)pthread_self());

  /* terminate the loop only when there are no more items in the queue
   * AND the producer threads are all done */

  pthread_mutex_lock(&queue_p->lock);
  pthread_mutex_lock(&g_num_prod_lock);
  //Bug 5. Mutex for queue_p is locked when testing while condition only on the first loop run.
  while(queue_p->front != NULL || g_num_prod > 0) {
    pthread_mutex_unlock(&g_num_prod_lock);
    if (queue_p->front != NULL) {

      /* Remove the prev item from the queue */
      prev_node_p = queue_p->front;

      if (queue_p->front->next == NULL)
        queue_p->back = NULL;
      else
        queue_p->front->next->prev = NULL;

      queue_p->front = queue_p->front->next;
      //Bug 5. Fix. Must unlock mutex AFTER we print the value.
      //pthread_mutex_unlock(&queue_p->lock);

      /* Print the character, and increment the character count */
      printf("%c from %x\n", prev_node_p->c, (unsigned int) pthread_self());
      free(prev_node_p);
      
      //Bug 5. Fix. Must unlock mutex AFTER we print the value.
      pthread_mutex_unlock(&queue_p->lock);

      /* Bug3. Fix. Use mutex for lock when changing its value. */
      pthread_mutex_lock(&count_lock);
      ++count;
      pthread_mutex_unlock(&count_lock);
    }
    else { /* Queue is empty, so let some other thread run */
      pthread_mutex_unlock(&queue_p->lock);
      sched_yield();
    }
    //Bug 5. Fix. Must lock queue_p before the while tests condition. 
    pthread_mutex_lock(&queue_p->lock);
  }
  pthread_mutex_unlock(&g_num_prod_lock);
  pthread_mutex_unlock(&queue_p->lock);

  //Own code to test something.
  //printf("Exiting thread %x with count value as %d.\n", (unsigned int) pthread_self(), (int)count);
  return (void*) count;

  //Own code to test something.
  //pthread_exit((void *) count);
}
