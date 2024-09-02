#include <stdio.h>
#include <stdint.h> 

#define f 16384
static int load_avg = 0;

int
thread_get_load_avg (void) {
	/* TODO: Your implementation goes here */
	int ready_threads = 1;

	// load_avg = ((59 * f) / 60 * load_avg) / f  + ready_threads * f / 60 ;
    load_avg = ((int64_t)(((int64_t)(59 * f)) * f / (60 * f))) * load_avg / f  
                + ((int64_t)(1 * f))*f / (60 * f) * ready_threads;

    // load_avg = (59/60) * load_avg + (1/60) * ready_threads;
    
    // load_avg = ((59 * load_avg) + ready_threads * f) / 60 ;
	// msg("%d", load_avg);

	return load_avg;
}

int main(void){
    
    // for (int i = 0; i < 420; i++){
    //     thread_get_load_avg();
    //     printf("%ld\n", (load_avg + f / 2)/f);
    //     // printf("%ld\n", ((int64_t)(59 * f))*f/(60 * f));
    // }
    // int nice = 1;
    int PRI_MAX = 63;
    int result = (PRI_MAX * f - (f*4 / 4) - (5 * 2) * f);
       
    // printf("%d", (load_avg + f/2) / f);
    printf("%d", result);
}

