#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void r1(int int1);
void r2(void *main_id);

void *main() {
	pthread_t t1, t2;
	int ret;
	int x = 1000;
	ret = pthread_create(&t1, NULL, r1, x);
	if(ret != 0) 
		printf("F this shit %d.\n", ret);
	
	ret = pthread_create(&t2, NULL, r2, (void *) pthread_self());
	if(ret !=0)
		printf("WTF %d.\n", ret);

	pthread_join(r1, NULL);
	pthread_detach(t2);
	
	pthread_exit((void *) x);
}

void r1(int int1) {
	printf("r1 thread id %x.\n", pthread_self());
}

void r2(void *main_id) {
	void *main_retval = NULL;

	printf("r2 thread id %x.\n", (long unsigned) pthread_self());
	pthread_join(main_id, &main_retval);
	
	printf("Main returned %d.\n", (int) main_retval);
	//free(main_retval);

}
