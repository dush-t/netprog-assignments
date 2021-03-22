#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>

#include "message.h"

char server_queue_path[1024];
char client_queue_path[1024];
char client_name[1024];

int server_queue;
int client_queue;



int
init_connection() {
    char* squeue_env = getenv("SERVER_QUEUE_PATH");
    char* cqueue_env = getenv("CLIENT_QUEUE_PATH");
    if (squeue_env == NULL) {
        printf("SERVER_QUEUE_PATH env var not set.\nEnter server queue path: ");
        scanf(" %s", server_queue_path);
    } else {
        strcpy(server_queue_path, squeue_env);
    }
    if (cqueue_env == NULL) {
        printf("CLIENT_QUEUE_PATH env var not set. Creating a queue file.\n");
        sprintf(client_queue_path, "./%s.%lu.queue", client_name, (unsigned long) time(NULL));
        if (creat(client_queue_path, 0777) == -1) {
            printf("Error while creating client queue. Exiting.\n");
            return -1;
        } else {
            printf("Client queue created at %s\n", client_queue_path);
        }
    } else {
        strcpy(client_queue_path, cqueue_env);
    }

    printf("%s\n", client_queue_path);

    key_t skey = ftok(server_queue_path, 1);
    if (skey < 0) {
        perror("ftok: while getting server queue key");
        return -1;
    }
    server_queue = msgget(skey, 0777 | IPC_CREAT);
    if (server_queue < 0) {
        perror("msgget");
        return -1;
    }

    key_t ckey = ftok(client_queue_path, 0);
    if (ckey < 0) {
        perror("ftok: while getting client queue key");
        return -1;
    }
    printf("ckey=%d\n", ckey);

    client_queue = msgget(ckey, 0777 | IPC_CREAT);
    if (client_queue < 0) {
        perror("msgget");
        return -1;
    }

    return 0;
}



int
register_self(char name[], char queuepath[]) {
    printf("Registering client as %s\n", name);

    ControlMessage cmsg;
    cmsg.mtype = CONTROL;
    cmsg.action = REGISTER_CLIENT;
    strcpy(cmsg.name, name);
    strcpy(cmsg.queuepath, queuepath);
    cmsg.src = getuid();


    int size = sizeof(cmsg) - sizeof(long);
    // printf("\n\nsize = %d | size2 = %d | size3 = %d\n\n", size, size2, size3);
    int status = msgsnd(server_queue, &cmsg, size, 0);
    if (status < 0) {
        perror("msgsnd");
        return -1;
    }

    printf("Sent register control message\n");

    ControlResponse cres;
    int cres_size = sizeof(cres) - sizeof(long);

    status = msgrcv(client_queue, (void*)&cres, cres_size, CONTROL_RESPONSE, 0);
    if (status < 0) {
        perror("msgrcv");
        return -1;
    }

    if (cres.status != STATUS_OK) {
        printf("Error in registration. Aborting.\n");
        return -1;
    }

    printf("Registration successful!\n");
    return 0;
}



int get_gid(char group_name[]) {
    printf("Finding group with name: %s\n", group_name);

    QueryRequest query;
    query.mtype = QUERY_REQUEST;
    query.query_type = QUERY_GROUP;
    query.src = getuid();
    sprintf(query.content, "%s", group_name);
    int qsize = sizeof(query) - sizeof(long);
    int qstat = msgsnd(server_queue, &query, qsize, 0);
    if (qstat < 0) {
        perror("msgsnd");
        printf("Unable to send group query\n");
        return -1;
    }

    QueryResponse qres;
    int qres_size = sizeof(qres) - sizeof(long);
    int gstat = msgrcv(client_queue, &qres, qres_size, QUERY_RESPONSE, 0);
    if (gstat < 0) {
        perror("msgrcv");
        printf("Unable to receive group query response\n");
        return -1;
    }

    if (qres.status == STATUS_ERROR) {
        printf("Error resolving query: %s", qres.content);
        return -1;
    }

    int gid = atoi(qres.content);
    printf("Found group. Gid = %d", gid);
    return gid;
}



int 
join_group(char group_name[]) {
    printf("Joining group %s...\n", group_name);

    int gid = get_gid(group_name);
    if (gid < 0) {
        printf("Unable to find group\n");
        return -1;
    }

    ControlMessage cmsg;
    cmsg.mtype = CONTROL;
    cmsg.action = JOIN_GROUP;
    cmsg.gid = gid;
    cmsg.src = getuid();

    int size = sizeof(cmsg) - sizeof(long);
    int status = msgsnd(server_queue, &cmsg, size, 0);
    if (status < 0) {
        perror("msgsnd");
        printf("Unable to send group join request\n");
        return -1;
    }

    ControlResponse cres;
    size = sizeof(cres) - sizeof(long);
    status = msgrcv(client_queue, &cres, size, CONTROL, 0);
    if (status < 0) {
        perror("msgrcv");
        printf("Unable to receive response for group join request\n");
        return -1;
    }

    printf("Joined group %s with id %d\n", group_name, gid);
    return gid;
}



int
create_group(char group_name[]) {
    printf("Creating group %s...\n", group_name);

    ControlMessage cmsg;
    cmsg.mtype = CONTROL;
    cmsg.action = CREATE_GROUP;
    sprintf(cmsg.name, "%s", group_name);
    cmsg.src = getuid();
    
    int size = sizeof(cmsg) - sizeof(long);
    int status = msgsnd(server_queue, &cmsg, size, 0);
    if (status < 0) {
        perror("msgsnd");
        printf("Unable to send group create request\n");
        return -1;
    }

    ControlResponse cres;
    size = sizeof(cres) - sizeof(long);
    status = msgrcv(client_queue, (void*)&cres, size, CONTROL_RESPONSE, 0);
    if (status < 0) {
        perror("msgrcv");
        printf("Unable to receive response for group create request\n");
        return -1;
    }

    if (cres.status != STATUS_OK) {
        printf("Error creating new group\n");
        return -1;
    }

    int gid = cres.gid;
    printf("Successfully created new group %s with gid %d", group_name, gid);
    return gid;
}



int
main(int argc, char** argv) {
    char *client_name_env = getenv("CLIENT_NAME");
    if (client_name_env == NULL) {
        printf("CLIENT_NAME env var not set.\nEnter client name: ");
        scanf(" %s", client_name);
    } else {
        strcpy(client_name, getenv("CLIENT_NAME"));
    }
    
    int status = init_connection();
    if (status < 0) {
        printf("Error initialising connection.\n");
        return -1;
    }
    status = register_self(client_name, client_queue_path);
    if (status < 0) {
        printf("Error registering client.\n");
        return -1;
    }

    if (argc > 1 && strcmp(argv[1], "join-group") == 0) {
        char group_name[1024];
        strcpy(group_name, argv[2]);
        int gid = join_group(group_name);
        if (gid < 0) {
            printf("Unable to join group.\n");
            return -1;
        }
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "create-group") == 0) {
        char group_name[1024];
        strcpy(group_name, argv[2]);
        int gid = create_group(group_name);
        if (gid < 0) {
            printf("Unable to create new group\n");
            return -1;
        }
        return 0;
    }
}