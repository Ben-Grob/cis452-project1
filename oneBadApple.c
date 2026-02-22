/*
cis452 Project1 - One Bad Apple

Copied SampleProgramTwo from lab three
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define READ 0
#define WRITE 1
int main()
{
    int fd[2];
    int k = 0;
    int pipeCreationResult;
    char myStringOutput[] = "This a test!";
    char myStringInput[50];
    int pid;    

    printf("Input the desired number of nodes for the network.");
    fgets(k, 10, stdin);
    int pipes [k][2]; // create k pipes

    // Change this to loop k times making a pipe each time - use % operator
    pipeCreationResult = pipe(fd);
    if(pipeCreationResult < 0){
        perror("Failed pipe creation\n");
        exit(1);
    }

    // Change this to fork k times
    pid = fork();
    if(pid < 0) // Fork failed
    {
        perror("Fork failed");
        exit(1);
    }

    int output = 3;
    int input;

    // not sure how we'll change this
    if(pid == 0)
    { // Child process
        write(fd[1], &myStringOutput, sizeof(myStringOutput));
        printf("Child wrote [%s]\n", myStringOutput);
    }
    else
    {
        read(fd[0], &myStringInput, sizeof(myStringInput));
        printf("Parent received [%s] from child process\n", myStringInput);
    }

    return 0;
}
