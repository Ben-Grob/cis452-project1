#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>


void* baker_function(void* arg) {
    // Loop through each recipe
        // Acquire ingredients
        // Acquire bowl, spoon, mixer
        // Mix
        // Release bowl, spoon, mixer
        // Acquire oven
        // Bake
        // Release oven
    
    // Announce finished
    return NULL;
}

int main() {
    // 1. Initialize all structs and their semaphores
    // we can create structs for each resource type
    
    // 2. Ask user for number of bakers
    int num_bakers = 0;
    
    // 3. Loop to create baker threads 
    for (int i = 0; i < num_bakers; i++) {
        pthread_create(&threads[i], NULL, baker_function, &baker_args[i]);
    }
    
    // 4. Wait for all threads to finish
    for (int i = 0; i < num_bakers; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 5. Destroy all semaphores and cleanup
}