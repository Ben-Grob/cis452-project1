#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#define READ 0
#define WRITE 1
#define MAX_MESSAGE_LENGTH 256
#define EMPTY_DESTINATION -1

typedef struct
{
    int isShutdown;
    int sourceNode;
    int destinationNode;
    // Shared token payload carried around the ring.
    char payload[MAX_MESSAGE_LENGTH];
} AppleMessage;

volatile sig_atomic_t shutdownRequested = 0;

void handleSigint(int signalNumber)
{
    (void)signalNumber;
    shutdownRequested = 1;
}

ssize_t readAll(int fileDescriptor, void *buffer, size_t bytesToRead)
{
    size_t totalRead = 0;
    char *bufferPointer = (char *)buffer;

    // Pipes are streams; loop until exactly one full message is read.
    while (totalRead < bytesToRead)
    {
        ssize_t bytesRead = read(fileDescriptor, bufferPointer + totalRead, bytesToRead - totalRead);
        if (bytesRead == 0)
        {
            return 0;
        }
        if (bytesRead < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        totalRead += (size_t)bytesRead;
    }

    return (ssize_t)totalRead;
}

ssize_t writeAll(int fileDescriptor, const void *buffer, size_t bytesToWrite)
{
    size_t totalWritten = 0;
    const char *bufferPointer = (const char *)buffer;

    // Pipes are streams; loop until exactly one full message is written.
    while (totalWritten < bytesToWrite)
    {
        ssize_t bytesWritten = write(fileDescriptor, bufferPointer + totalWritten, bytesToWrite - totalWritten);
        if (bytesWritten < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        totalWritten += (size_t)bytesWritten;
    }

    return (ssize_t)totalWritten;
}

void clearMessage(AppleMessage *message)
{
    message->isShutdown = 0;
    message->sourceNode = 0;
    message->destinationNode = EMPTY_DESTINATION;
    message->payload[0] = '\0';
}

void prepareShutdownMessage(AppleMessage *message, int sourceNode)
{
    clearMessage(message);
    message->isShutdown = 1;
    message->sourceNode = sourceNode;
}

int promptForMessage(AppleMessage *message, int nodeCount)
{
    char inputBuffer[128];
    char *endPointer;
    long destinationNode;

    while (1)
    {
        printf("\nEnter destination node (0-%d) or q to quit: ", nodeCount - 1);
        fflush(stdout);

        if (!fgets(inputBuffer, sizeof(inputBuffer), stdin) || shutdownRequested)
        {
            return 0;
        }
        if (inputBuffer[0] == 'q' || inputBuffer[0] == 'Q')
        {
            return 0;
        }

        errno = 0;
        destinationNode = strtol(inputBuffer, &endPointer, 10);
        if (errno == 0 && endPointer != inputBuffer && (*endPointer == '\n' || *endPointer == '\0') &&
            destinationNode >= 0 && destinationNode < nodeCount)
        {
            break;
        }

        printf("Invalid destination node. Try again.\n");
    }

    printf("Enter message: ");
    fflush(stdout);
    if (!fgets(message->payload, sizeof(message->payload), stdin) || shutdownRequested)
        return 0;

    message->payload[strcspn(message->payload, "\n")] = '\0';
    message->isShutdown = 0;
    message->sourceNode = 0;
    message->destinationNode = (int)destinationNode;
    return 1;
}

int main(void)
{
    int k = 0;

    printf("Input the desired number of nodes for the network: ");
    fflush(stdout);
    if (scanf("%d", &k) != 1 || k < 2)
    {
        fprintf(stderr, "Please enter an integer value >= 2.\n");
        return 1;
    }
    getchar();

    int pipes[k][2];
    pid_t pids[k];
    int nodeId = 0;

    for (int i = 0; i < k; i++)
    {
        pids[i] = -1;
        if (pipe(pipes[i]) < 0)
        {
            perror("Failed pipe creation");
            return 1;
        }
    }

    // Parent is node 0; create child processes for nodes 1..k-1.
    for (int i = 1; i < k; i++)
    {
        pids[i] = fork();
        if (pids[i] < 0)
        {
            perror("Fork failed");
            return 1;
        }
        if (pids[i] == 0)
        {
            nodeId = i;
            break;
        }
        printf("Parent created child process for node %d with pid %d\n", i, pids[i]);
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = (nodeId == 0) ? handleSigint : SIG_IGN;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);

    // Ring mapping: read from previous node, write to next node.
    int readPipeIndex = (nodeId - 1 + k) % k;
    int inputFileDescriptor = pipes[readPipeIndex][READ];
    int outputFileDescriptor = pipes[nodeId][WRITE];

    for (int i = 0; i < k; i++)
    {
        if (pipes[i][READ] != inputFileDescriptor)
        {
            close(pipes[i][READ]);
        }
        if (pipes[i][WRITE] != outputFileDescriptor)
        {
            close(pipes[i][WRITE]);
        }
    }

    printf("Node %d started (pid=%d), reads from node %d and writes to node %d\n",
           nodeId, getpid(), readPipeIndex, (nodeId + 1) % k);

    if (nodeId == 0)
    {
        AppleMessage message;

        // Parent injects first token message into the ring.
        if (!promptForMessage(&message, k))
        {
            prepareShutdownMessage(&message, 0);
            writeAll(outputFileDescriptor, &message, sizeof(message));
        }
        else
        {
            printf("Node 0 sending message to node %d: \"%s\"\n", message.destinationNode, message.payload);
            if (writeAll(outputFileDescriptor, &message, sizeof(message)) >= 0)
            {
                while (1)
                {
                    ssize_t bytesRead = readAll(inputFileDescriptor, &message, sizeof(message));
                    if (bytesRead <= 0)
                    {
                        break;
                    }

                    printf("Node 0 received apple from node %d\n", k - 1);

                    if (message.isShutdown)
                    {
                        break;
                    }
                    if (message.destinationNode == 0)
                    {
                        printf("Node 0 received message: \"%s\" (from node %d)\n", message.payload, message.sourceNode);
                        clearMessage(&message);
                    }
                    if (shutdownRequested)
                    {
                        prepareShutdownMessage(&message, 0);
                        writeAll(outputFileDescriptor, &message, sizeof(message));
                        break;
                    }
                    if (message.destinationNode == EMPTY_DESTINATION)
                    {
                        // Apple came back empty: prompt for the next user message.
                        if (!promptForMessage(&message, k))
                        {
                            prepareShutdownMessage(&message, 0);
                            writeAll(outputFileDescriptor, &message, sizeof(message));
                            break;
                        }
                        printf("Node 0 sending message to node %d: \"%s\"\n", message.destinationNode, message.payload);
                    }
                    if (writeAll(outputFileDescriptor, &message, sizeof(message)) < 0)
                    {
                        break;
                    }
                }
            }
        }

        close(inputFileDescriptor);
        close(outputFileDescriptor);
        for (int i = 1; i < k; i++)
        {
            if (pids[i] > 0)
            {
                waitpid(pids[i], NULL, 0);
            }
        }
        printf("Node 0 exiting cleanly.\n");
    }
    else
    {
        AppleMessage message;

        while (1)
        {
            ssize_t bytesRead = readAll(inputFileDescriptor, &message, sizeof(message));
            if (bytesRead <= 0)
            {
                break;
            }

            printf("Node %d received apple. Header destination=%d\n", nodeId, message.destinationNode);

            if (message.isShutdown)
            {
                printf("Node %d forwarding shutdown token.\n", nodeId);
                writeAll(outputFileDescriptor, &message, sizeof(message));
                break;
            }
            if (message.destinationNode == nodeId)
            {
                // This node is the destination, so consume and clear the header.
                printf("Node %d received message: \"%s\" (from node %d)\n", nodeId, message.payload, message.sourceNode);
                clearMessage(&message);
            }

            printf("Node %d forwarding apple to node %d\n", nodeId, (nodeId + 1) % k);
            if (writeAll(outputFileDescriptor, &message, sizeof(message)) < 0)
            {
                break;
            }
        }

        close(inputFileDescriptor);
        close(outputFileDescriptor);
        printf("Node %d exiting cleanly.\n", nodeId);
    }

    return 0;
}
