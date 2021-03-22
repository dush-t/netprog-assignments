#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <string.h>
#include <pthread.h>

#include "message.h"

#define MAX 1024
#define MAX_GROUPS 32



typedef struct Clients {
    int cid;
    char name[MAX];
    char queue_path[MAX];
    int queue;
    int num_groups;
    int groups[MAX_GROUPS][2];
} Client;

typedef struct Groups {
    int gid;
    char name[MAX];
    int num_clients;
    Client* clients[MAX];
} Group;


Client* clients[MAX];
Group* groups[MAX];
int next_group_id = 0;
int total_clients = 0;
int total_groups = 0;
int server_queue;


Client* 
new_client(int cid, char name[], char qpath[]) {
    Client* client = (Client*)malloc(sizeof(Client));
    client->cid = cid;
    sprintf(client->name, "%s", name);
    sprintf(client->queue_path, "%s", name);

    return client;
}

Group*
new_group(int gid, char name[]) {
    Group* group = (Group*)malloc(sizeof(Group));
    group->gid = gid;
    sprintf(group->name, "%s", name);

    return group;
}

Group* 
find_group_by_id(int id) {
    if (id < total_groups && groups[id]->gid == id) {
        return groups[id];
    }
    for (int i = 0; i < total_groups; i++) {
        if (groups[i]->gid == id) return groups[i];
    }

    return NULL;
}

Group*
find_group_by_name(char name[]) {
    for (int i = 0; i < total_groups; i++) {
        if (strcmp(groups[i]->name, name) == 0) {
            return groups[i];
        }
    }

    return NULL;
}

Client*
find_client_by_id(int id) {
    for (int i = 0; i < total_clients; i++) {
        if (clients[i]->cid == id) return clients[i];
    }

    return NULL;
}

Client*
find_client_by_name(char name[]) {
    for (int i = 0; i < total_clients; i++) {
        if (strcmp(clients[i]->name, name) == 0) {
            return clients[i];
        }
    }

    return NULL;
}


int
get_client_queue_for_group(Client* clt, Group* grp) {
    int num_grps = clt->num_groups;
    for (int i = 0; i < num_grps; i++) {
        if (clt->groups[i][0] == grp->gid) {
            return clt->groups[i][1];
        }
    }

    return -1;
}


int
send_group_message(Group* grp, Message msg_buf) {
    int num_clients = grp->num_clients;
    
    msg_buf.mtype = GROUP_MESSAGE;
    msg_buf.protocol = GROUP_MESSAGE;

    for (int i = 0; i < num_clients; i++) {
        // int queue = grp->clients[i]->queue;
        int queue = get_client_queue_for_group(grp->clients[i], grp);
        if (queue == -1) {
            printf("Error: user not registered in group\n");
            return -1;
        }
        int size = sizeof(msg_buf) - sizeof(msg_buf.mtype);
        int status = msgsnd(queue, &msg_buf, size, 0);
        if (status < 0) {
            perror("msgsnd");
            return -1;
        }
    }

    return 0;
}

int
send_client_message(Client* clt, Message msg_buf) {
    msg_buf.mtype = CLIENT_MESSAGE;
    msg_buf.protocol = CLIENT_MESSAGE;

    int size = sizeof(msg_buf) - sizeof(msg_buf.mtype);
    int status = msgsnd(clt->queue, &msg_buf, size, 0);
    if (status < 0) {
        perror("msgsnd");
        return -1;
    }

    return 0;
}

int
send_query_response(Client* clt, QueryResponse qres) {
    int size = sizeof(qres) - sizeof(qres.mtype);
    int status = msgsnd(clt->queue, &qres, size, 0);
    if (status < 0) {
        perror("msgsnd");
        return -1;
    }

    return 0;
}

int
send_control_response(Client* clt, ControlResponse cres) {
    int size = sizeof(cres) - sizeof(long);
    int status = msgsnd(clt->queue, &cres, size, 0);
    if (status < 0) {
        perror("msgsnd");
        return -1;
    }

    return 0;
}

Client*
register_client(int uid, char name[], char queuepath[]) {
    printf("Inside register_client\n");
    if (find_client_by_id(uid) != NULL) {
        printf("Client already exists\n");
        Client* clt = find_client_by_id(uid);
        sprintf(clt->name, "%s", name);
        sprintf(clt->queue_path, "%s", queuepath);
        return clt;
    }

    printf("Creating new client\n");
    key_t key = ftok(queuepath, 0);
    printf("ckey=%d\n", key);
    int queue = msgget(key, 0777 | IPC_CREAT);
    if (queue < 0) {
        perror("msgget");
        return NULL;
    }

    printf("Connected to the client's main queue\n");

    Client* client = new_client(uid, name, queuepath);
    client->queue = queue;
    client->num_groups = 0;

    printf("Client created\n");

    clients[total_clients++] = client;

    printf("Client added to db\n");

    return client;
}


int
join_group(Group* grp, Client* clt) {
    key_t key = ftok(clt->queue_path, grp->gid+1);
    int grp_queue = msgget(key, 0664 | IPC_CREAT);
    if (grp_queue < 0) {
        perror("msgget");
        return -1;
    }

    // Add group to client data structure
    clt->groups[clt->num_groups][0] = grp->gid;
    clt->groups[clt->num_groups][1] = grp_queue;
    clt->num_groups = clt->num_groups + 1;

    // Add client to group data structure
    grp->clients[grp->num_clients] = clt;
    grp->num_clients += 1;

    return grp->gid;
}


Group*
create_group(char name[], Client* creator) {
    // Create Group struct and update relevant data structures
    Group* group = new_group(next_group_id++, name);
    total_groups++;

    int status = join_group(group, creator);
    if (status < 0) {
        printf("Failed to join group");
        return NULL;
    }
    
    return group;
}



void* 
handle_group_send_msgs() {
    while (1) {
        Message msg;
        int size = sizeof(msg) - sizeof(msg.mtype);
        int status = msgrcv(server_queue, &msg, size, GROUP_MESSAGE, 0);
        if (status < 0) {
            perror("msgrcv");
            return (void*)-1;
        }

        Group* grp = find_group_by_id(msg.dst);
        send_group_message(grp, msg);
    }
}

void* 
handle_client_send_msgs() {
    while (1) {
        Message msg;
        int size = sizeof(msg) - sizeof(msg.mtype);
        int status = msgrcv(server_queue, &msg, size, CLIENT_MESSAGE, 0);
        if (status < 0) {
            perror("msgrcv");
            return (void*)-1;
        }

        Client* clt = find_client_by_id(msg.dst);
        send_client_message(clt, msg);
    }
}

char*
get_all_clients_msg() {
    char* result = (char*)malloc(sizeof(char) * MAX_LEN * 10);
    for (int i = 0; i < total_clients; i++) {
        char cltString[512];
        sprintf(cltString, "Name = %s | ID = %d\n", clients[i]->name, clients[i]->cid);
        strcat(result, cltString);
    }

    return result;
}

char* 
get_all_groups_msg() {
    char* result = (char*)malloc(sizeof(char) * MAX_LEN * 10);
    for (int i = 0; i < total_groups; i++) {
        char grpString[512];
        sprintf(grpString, "Name = %s | ID = %d\n", groups[i]->name, groups[i]->gid);
    }

    return result;
}

void*
handle_queries() {
    while (1) {
        QueryRequest query;
        int size = sizeof(query) - sizeof(query.mtype);
        int status = msgrcv(server_queue, &query, size, QUERY_REQUEST, 0);
        if (status < 0) {
            perror("msgrcv");
            return (void*)-1;
        }

        Client* src_client = find_client_by_id(query.src);
        QueryResponse res;
        res.mtype = QUERY_RESPONSE;

        switch (query.query_type) {
        case QUERY_CLIENT:
            if (strcmp(query.content, "__ALL__") != 0) {
                Client* clt = find_client_by_name(query.content);
                if (clt == NULL) {
                    res.status = STATUS_ERROR;
                    break;
                }
                sprintf(res.content, "%d", clt->cid);
                res.status = STATUS_OK;
            } 
            else {
                char* result = get_all_clients_msg();
                sprintf(res.content, "%s", result);
                res.status = STATUS_OK;
            }
            break;

        case QUERY_GROUP:
            if (strcmp(query.content, "__ALL__") != 0) {
                Group* grp = find_group_by_name(query.content);
                if (grp == NULL) {
                    res.status = STATUS_ERROR;
                    break;
                }
                sprintf(res.content, "%d", grp->gid);
                res.status = STATUS_OK;
            }
            else {
                char* result = get_all_groups_msg();
                sprintf(res.content, "%s", result);
                res.status = STATUS_OK;
            }
            break;
        
        default:
            sprintf(res.content, "Invalid request!");
            res.status = STATUS_ERROR;
            break;
        }
        
        send_query_response(src_client, res);
    }
}

void*
handle_control_msgs() {
    printf("Started control handler\n");
    while (1) {
        ControlMessage cmsg;
        int size = sizeof(cmsg) - sizeof(cmsg.mtype);
        printf("Expecting size=%d\n", size);
        int status = msgrcv(server_queue, (void*)&cmsg, size, CONTROL, 0);
        if (status < 0) {
            perror("msgrcv");
            return (void*)-1;
        }

        ControlResponse cres;
        cres.mtype = CONTROL_RESPONSE;
        cres.action = cmsg.action;

        switch (cmsg.action) {
        case REGISTER_CLIENT: {
            printf("Register client message received\n");
            Client* clt = register_client(cmsg.src, cmsg.name, cmsg.queuepath);
            printf("Client registered\n");
            cres.status = STATUS_OK;
            break;
        }
        
        case JOIN_GROUP: {
            Client* clt = find_client_by_id(cmsg.src);
            Group* grp = find_group_by_id(cmsg.gid);
            join_group(grp, clt);
            cres.status = STATUS_OK;
            break;
        }

        case CREATE_GROUP:{
            printf("Finding client\n");
            Client* clt = find_client_by_id(cmsg.src);
            printf("Found client\n");
            printf("Creating group\n");
            Group* grp = create_group(cmsg.name, clt);
            printf("Created group\n");
            cres.status = STATUS_OK;
            cres.gid = grp->gid;
            printf("Everything good\n");
            break;
        }

        default:
            cres.status = STATUS_ERROR;
            break;
        }

        Client* src_clt = find_client_by_id(cmsg.src);
        printf("Sending response to %s\n", src_clt->name);
        send_control_response(src_clt, cres);
        printf("Sent response\n");
    }
}

int
main() {
    key_t key = ftok("server.queue", 1);
    server_queue = msgget(key, 0777 | IPC_CREAT);
    if (server_queue < 0) {
        printf("Failed to create server queue!\n");
        perror("msgget");
        exit(-1);
    }

    printf("Connected to queue.\nStarting handlers...\n");
    pthread_t grp_send, clt_send, control, query;

    pthread_create(&grp_send, NULL, handle_group_send_msgs, NULL);
    pthread_create(&clt_send, NULL, handle_client_send_msgs, NULL);
    pthread_create(&control, NULL, handle_control_msgs, NULL);
    pthread_create(&query, NULL, handle_queries, NULL);
    
    pthread_join(grp_send, NULL);
    pthread_join(clt_send, NULL);
    pthread_join(control, NULL);
    pthread_join(query, NULL);

    return 0;
}