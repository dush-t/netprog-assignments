#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <pwd.h>

#include "message.h"

#define MESSAGE_TYPE_GROUP 1
#define MESSAGE_TYPE_CLIENT 2

char server_queue_path[1024];
char client_queue_path[1024];
char client_name[1024];

int server_queue;
int client_queue, group_queue, private_queue;

int recvtype;

char*
get_username() {
    struct passwd *pws;
    pws = getpwuid(getuid());
    return pws->pw_name;
}

int
get_queue(key_t key) {
    int queue = msgget(key, 0777 | IPC_CREAT);
    if (queue < 0) {
        queue = msgget(key, 0644);
        printf("queue = %d\n", queue);
    }

    return queue;
}

int try_init_dir(char dir[]) {
    strcat(dir, "/.simplemsg");
    int status = mkdir(dir, 0777);
    if (status == -1) {
        return 0;
    }

    char control[1024], private[1024], group[1024];
    strcpy(control, dir); strcat(control, "/control.queue");
    int fd = open(control, 0777); close(fd);

    return 0;
}

int
init_connection() {
    char target_dir[1024];
    strcpy(target_dir, getenv("HOME"));
    try_init_dir(target_dir);
    char* homedir = getenv("HOME");

    char control[1024], private[1024], group[1024];
    sprintf(client_queue_path, "%s/.simplemsg/control.queue", homedir);

    // printf("%s\n", client_queue_path);

    key_t skey = ftok("/tmp/simplemsg/server.queue", 1);
    server_queue = msgget(skey, 0777 | IPC_CREAT);
    if (server_queue < 0) {
        perror("msgget");
        return -1;
    }

    key_t ckey = ftok(client_queue_path, 0);
    
    // if (ckey == -1) {
    //     perror("ftok");
    //     return -1;
    // }
    // printf("ckey=%d\n", ckey);

    client_queue = get_queue(ckey);
    // printf("Client queue = %d\n", client_queue);
    if (client_queue < 0 || group_queue < 0 || private_queue < 0) {
        perror("msgget");
        return -1;
    }

    return 0;
}



int
register_self(char name[], char queuepath[]) {
    // printf("Registering client as %s\n", name);

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

    // printf("Sent register control message\n");

    ControlResponse cres;
    int cres_size = sizeof(cres) - sizeof(long);

    status = msgrcv(client_queue, (void*)&cres, cres_size, CONTROL_RESPONSE, 0);
    if (status < 0) {
        perror("msgrcv");
        return -1;
    }

    if (cres.status != STATUS_OK) {
        printf("Error in initialization. Aborting.\n");
        return -1;
    }

    // printf("Registration successful!\n");
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
    printf("Found group. Gid = %d\n", gid);
    return gid;
}

int get_cid(char client_name[]) {
    printf("Finding user with name: %s\n", client_name);

    QueryRequest query;
    query.mtype = QUERY_REQUEST;
    query.query_type = QUERY_CLIENT;
    query.src = getuid();
    strcpy(query.content, client_name);
    int qsize = sizeof(query) - sizeof(long);
    int qstat = msgsnd(server_queue, &query, qsize, 0);
    if (qstat < 0) {
        perror("msgsnd");
        printf("Unable to send client query\n");
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

    int cid = atoi(qres.content);
    printf("Found client. Cid = %d\n", cid);
    return cid;
}


int 
join_group(char group_name[]) {
    printf("Joining group %s...\n", group_name);

    int gid = get_gid(group_name);
    if (gid < 0) {
        printf("Unable to find group\n");
        return -1;
    }

    printf("Found group with gid = %d\n", gid);
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

    printf("Sent group join request\n");

    ControlResponse cres;
    size = sizeof(cres) - sizeof(long);
    status = msgrcv(client_queue, &cres, size, CONTROL_RESPONSE, 0);
    if (status < 0) {
        perror("msgrcv");
        printf("Unable to receive response for group join request\n");
        return -1;
    }

    printf("Successfully joined group %s with id %d\n", group_name, gid);
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
    printf("Successfully created new group %s with gid %d\n", group_name, gid);
    return gid;
}

void*
handle_messages() {
    int mtype = getuid() + 5000;
    while (1) {
        Message msg;
        int size = sizeof(msg) - sizeof(long); 
        int status = msgrcv(server_queue, (void*)&msg, size, mtype, 0);
        if (status < 0) {
            perror("handle_messages > msgrcv");
            return NULL;
        }

        if (msg.protocol == GROUP_MESSAGE) {
            msg.mtype = 5000 + msg.dst;
            int status = msgsnd(client_queue, &msg, size, 0);
            if (status < 0) {
                perror("handle_messages > GROUP_MESSAGE msgsnd");
                return NULL;
            }
        }
        else if (msg.protocol == CLIENT_MESSAGE) {
            msg.mtype = 10000 + msg.src;
            int status = msgsnd(client_queue, &msg, size, 0);
            if (status < 0) {
                perror("handle_messages > CLIENT_MESSAGE msgsnd");
                return NULL;
            }
        }
    }
}

int
start_message_send_loop(char name[], int type) {
    int mtype, dst;
    if (type == MESSAGE_TYPE_CLIENT) {
        mtype = CLIENT_MESSAGE;
        dst = get_cid(name);
    }
    else if (type == MESSAGE_TYPE_GROUP) {
        mtype = GROUP_MESSAGE;
        dst = get_gid(name);
    }

    long auto_delete = -1;

    while (1) {
        Message msg;
        msg.protocol = (int)mtype;
        msg.src = getuid();
        msg.dst = dst;
        msg.auto_delete = -1;
        msg.timestamp = (long)time(NULL);
        strcpy(msg.src_name, client_name);
        fgets(msg.content, sizeof(msg.content), stdin);

        int msgsize = sizeof(msg) - sizeof(long);
        if (type == MESSAGE_TYPE_CLIENT) {
            msg.mtype = dst + 10000;
            int status = msgsnd(client_queue, (void*)&msg, msgsize, 0);
            if (status < 0) {
                perror("msgsnd");
                printf("Failed to send message to self-queue\n");
                return -1;
            }
        }

        msg.mtype = mtype;
        int status = msgsnd(server_queue, (void*)(&msg), msgsize, 0);
        if (status < 0) {
            perror("msgsnd");
            printf("Unable to send message. Exiting\n");
            return -1;
        }
        printf("Message sent\n");
    }

    return 0;
}

int
start_message_rcv_loop(char name[], int type) {
    int mtype;
    char queue_path[1024];
    
    recvtype = type;
    pthread_t router_thread;

    if (type == MESSAGE_TYPE_CLIENT) {
        mtype = get_cid(name) + 10000;
    }
    else if (type == MESSAGE_TYPE_GROUP) {
        mtype = get_gid(name) + 5000;
    }

    pthread_create(&router_thread, NULL, handle_messages, NULL);

    printf("Listening for messages, press Ctrl+C to stop\n");

    while (1) {
        Message msg;
        int size = sizeof(msg) - sizeof(long);
        int status = msgrcv(client_queue, (void*)&msg, size, mtype, 0);
        if (status < 0) {
            perror("msgrcv");
            return -1;
        }
        time_t tstamp = (time_t)msg.timestamp;
        struct tm tm = *localtime(&tstamp);
        printf("[%s\t at %02d:%02d] %s", msg.src_name, tm.tm_hour, tm.tm_min, msg.content);
    }

    return 0;
}

int
list_query(int type) {
    QueryRequest query;
    query.mtype = QUERY_REQUEST;
    query.query_type = type;
    query.src = getuid();
    strcpy(query.content, "__ALL__");
    int qsize = sizeof(query) - sizeof(long);
    int qstat = msgsnd(server_queue, &query, qsize, 0);
    if (qstat < 0) {
        perror("msgsnd");
        printf("Unable to send group query\n");
        return -1;
    }

    QueryResponse qres;
    int qres_size = sizeof(qres) - sizeof(long);
    while (1) {
        int gstat = msgrcv(client_queue, &qres, qres_size, QUERY_RESPONSE, 0);
        if (gstat < 0) {
            perror("list_query > msgrcv");
            return -1;
        }
        if (strcmp(qres.content, "__END__") == 0) {
            break;
        }
        printf("%s\n", qres.content);
    }

    return 0;
}


int
main(int argc, char** argv) {
    strcpy(client_name, get_username());
    init_connection();
    char target_dir[1024];
    sprintf(target_dir, "%s/.simplemsg", getenv("HOME"));
    int status = register_self(client_name, target_dir);
    if (status < 0) {
        printf("Error initializing.\n");
        return -1;
    }

    if (strcmp(argv[1], "join-group") == 0) {
        char group_name[1024];
        strcpy(group_name, argv[2]);
        int gid = join_group(group_name);
        if (gid < 0) {
            printf("Unable to join group.\n");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[1], "create-group") == 0) {
        char group_name[1024];
        strcpy(group_name, argv[2]);
        int gid = create_group(group_name);
        if (gid < 0) {
            printf("Unable to create new group\n");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[1], "read") == 0) {
        if (argc < 4) {
            printf("Usage: ./client read ['group' | 'client'] [name]\n");
            return -1;
        }

        char* name = argv[3];
        if (strcmp(argv[2], "group") == 0) {
            return start_message_rcv_loop(name, MESSAGE_TYPE_GROUP);
        }
        else {
            return start_message_rcv_loop(name, MESSAGE_TYPE_CLIENT);
        }
    }

    if (strcmp(argv[1], "send-to") == 0) {
        if (argc < 4) {
            printf("Usage: ./client send-to ['group' | 'client'] [name]\n");
            return -1;
        }

        char* name = argv[3];
        if (strcmp(argv[2], "group") == 0) {
            return start_message_send_loop(name, MESSAGE_TYPE_GROUP);
        }
        else {
            return start_message_send_loop(name, MESSAGE_TYPE_CLIENT);
        }
    }

    if (strcmp(argv[1], "list") == 0) {
        if (argc < 3) {
            printf("Usage: ./client list ['group' | 'client']\n");
            return -1;
        }

        if (strcmp(argv[2], "groups") == 0 || strcmp(argv[2], "group") == 0) {
            list_query(QUERY_GROUP);
        }
        else {
            list_query(QUERY_CLIENT);
        }
        
    }
}