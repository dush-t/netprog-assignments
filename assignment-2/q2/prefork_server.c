#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/inet.h>


#define PORT 9002
#define REQ_SIZE 2048
#define MAX_WORKERS 1024

#define GOT_REQUEST 1
#define COMPLETED_REQUEST 2
#define READ_ALLOWED 3

typedef struct ControlMsgs {
    int type;
    int pid;
    int reqs_handled;
} ControlMsg;

typedef struct Workers {
    pid_t pid;
    int reqs_handled;
    short is_idle;
    short to_harvest;
    int socket;
} Worker;


int minSpareServers, maxSpareServers, maxRequestsPerChild;
int sock, lsock; // sock is the TCP listening socket, lsock is the local socket for child-parent communication
int num_idle_workers = 0;
int num_workers = 0;
int is_worker = 0;

Worker* workers[MAX_WORKERS];
struct sockaddr_un server_addr;


struct sockaddr_un
get_worker_sock_addr(pid_t pid) {
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;

    bzero(addr.sun_path, 108);
    char path[64];
    sprintf(path, "childsock_%d", pid);
    strcpy(&(addr.sun_path[1]), path);

    return addr;
}

Worker* 
get_worker_by_id(pid_t pid) {
    for (int i = 0; i < num_workers; i++) {
        if (workers[i]->pid == pid) {
            return workers[i];
        }
    }

    return NULL;
}

Worker* 
remove_worker(pid_t pid) {
    for (int i = 0; i < num_workers; i++) {
        Worker* w = workers[i];
        if (w->pid == pid) {
            workers[i] = NULL;
            num_workers--;
            num_idle_workers--;
            kill(w->pid, SIGKILL);
            waitpid(w->pid, NULL, WUNTRACED);

            return w;
        }
    }

    return NULL;
}

Worker*
add_worker(pid_t pid) {
    Worker* wk = (Worker*)malloc(sizeof(Worker));
    wk->is_idle = 1;
    wk->pid = pid;
    wk->reqs_handled = 0;
    wk->to_harvest = 0;

    wk->socket = socket(AF_UNIX, SOCK_DGRAM, 0);

    int inserted = 0;
    for (int i = 0; i < num_workers; i++) {
        if (workers[i] == NULL) {
            workers[i] = wk;
            inserted = 1;
            break;
        }
    }

    if (!inserted) {
        workers[num_workers] = wk;
    }

    num_workers++;
    num_idle_workers++;

    return wk;
}

void
print_worker_stats() {
    printf("\n%d worker processes are running, %d are idle\n", num_workers, num_idle_workers);
    for (int i = 0; i < num_workers; i++) {
        Worker* wk = workers[i];
        printf("Worker pid = %d | clients_handled = %d | active = %d\n", wk->pid, wk->reqs_handled, !wk->is_idle);
    }
}

void
sigquit_handler(int signum) {
    if (!is_worker) {
        printf("Got SIGQUIT\nKilling all children\n");
        for (int i = 0; i < num_workers; i++) {
            Worker* wk = workers[i];
            kill(wk->pid, SIGKILL);
            waitpid(wk->pid, NULL, WUNTRACED);
            printf("Worker %d: killed\n", wk->pid);
        }
        printf("Children killed. Exiting now.\n");
        exit(0);
    }
}

void
sigint_handler(int signum) {
    if (!is_worker) {
        print_worker_stats();
    }
}


int
start_worker() {
    pid_t pid = getpid();
    struct sockaddr_un localaddr = get_worker_sock_addr(pid);
    int localsock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (bind(localsock, (struct sockaddr*)&localaddr, sizeof(localaddr)) < 0) {
        perror("worker local bind");
        exit(-1);
    }
    listen(sock, 5);

    struct sockaddr_un serveraddr;
    socklen_t len = sizeof(serveraddr);
    int reqs_handled = 0;
    int init = 0;

    while (1) {
        // Wait for ACK from server
        if (init) {
            char buf[sizeof(ControlMsg)];
            int ackstat = recvfrom(localsock, &buf, sizeof(buf), 0, (struct sockaddr*)&serveraddr, &len);
            if (ackstat < 0) {
                perror("worker ack recvfrom");
                continue;
            }
        }
        init = 1;

        printf("worker %d ready to accept requests\n", pid);
        // Wait for a request to arrive
        struct sockaddr_in caddr;
        int addr_len = sizeof(caddr);
        int csock = accept(sock, (struct sockaddr*)&caddr, &addr_len);
        if (csock < 0) {
            perror("accept");
            return -1;
        }

        // Send GOT_REQUEST message, and then serve the request
        ControlMsg cmsg;
        cmsg.pid = pid;
        cmsg.reqs_handled = reqs_handled;
        cmsg.type = GOT_REQUEST;
        // printf("server_addr = %s\n", &(server_addr.sun_path[1]));
        if (sendto(localsock, (char*)&cmsg, sizeof(cmsg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("worker got request sendto");
            return -1;
        }

        printf("Child [PID = %d]: Accepted connection from %s:%d\n",
                pid, inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));

        char reply[32];
        sprintf(reply, "Worker %d says OK", pid);
        if (send(csock, reply, sizeof(reply), 0) < 0) {
            perror("worker tcp send");
            return -1;
        }
        close(csock);

        // Send COMPLETED_REQUEST message
        reqs_handled++;
        cmsg.reqs_handled = reqs_handled;
        cmsg.type = COMPLETED_REQUEST;
        if (sendto(localsock, (char*)&cmsg, sizeof(cmsg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("worker got request sendto");
            return -1;
        }
    }
}


void 
create_new_worker() {
    pid_t pid = fork();
    if (pid == 0) {
        printf("Started new worker with pid %d\n", getpid());
        is_worker = 1;
        int status = start_worker();
        exit(status);
    }
    else {
        add_worker(pid);
    }
}



void
exponential_spawn(int n) {
    if (n > 32) n = 32;
    for (int i = 0; i < n; i++) {
        create_new_worker();    
    }

    if (num_idle_workers > minSpareServers) {
        return;
    }

    exponential_spawn(n * 2);
}

void
handle_control_msg(ControlMsg* cmsg) {
    switch (cmsg->type) {
        case GOT_REQUEST: {
            Worker* wk = get_worker_by_id(cmsg->pid);
            wk->is_idle = 0;
            num_idle_workers--;
            if (num_idle_workers < minSpareServers) {
                create_new_worker();
            }
            break;
        }
        case COMPLETED_REQUEST: {
            Worker* wk = get_worker_by_id(cmsg->pid);
            wk->is_idle = 1;
            wk->reqs_handled = cmsg->reqs_handled;
            num_idle_workers++;
            if (num_idle_workers > maxSpareServers) {
                remove_worker(wk->pid);
            }
            else if (cmsg->reqs_handled == maxRequestsPerChild) {
                remove_worker(wk->pid);
                create_new_worker();
            }
            else {
                ControlMsg ack;
                ack.type = READ_ALLOWED;
                struct sockaddr_un workeraddr = get_worker_sock_addr(wk->pid);
                if (sendto(lsock, (char*)&ack, sizeof(ack), 0, (struct sockaddr*)&workeraddr, sizeof(workeraddr)) < 0) {
                    perror("ack sendto");
                    exit(-1);
                }
            }
            break;
        }
        default: {
            printf("Received unknown message type: %d\n", cmsg->type);
            return;
        }
    }
}



int
main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("Usage: pserver.out [MinSpareServers] [MaxSpareServers] [MaxRequestsPerChild]\n");
        exit(-1);
    }

    signal(SIGQUIT, sigquit_handler);
    signal(SIGINT, sigint_handler);

    minSpareServers = atoi(argv[1]);
    maxSpareServers = atoi(argv[2]);
    maxRequestsPerChild = atoi(argv[3]);

    // Create the TCP listening socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    saddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("bind");
        exit(-1);
    };
    if (listen(sock, 10) < 0) {
        perror("listen");
        exit(-1);
    };

    // Create abstract UNIX domain socket
    lsock = socket(AF_UNIX, SOCK_DGRAM, 0);
    server_addr.sun_family = AF_UNIX;
    bzero(server_addr.sun_path, 108);
    strcpy(&(server_addr.sun_path[1]), "preforkserver");
    if (bind(lsock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("lbind");
        exit(-1);
    }
    
    exponential_spawn(1);

    while (1) {
        char buf[sizeof(ControlMsg)];

        struct sockaddr_un caddr;
        socklen_t len = sizeof(caddr);

        int num_bytes = recvfrom(lsock, &buf, sizeof(buf), 0, (struct sockaddr*)&caddr, &len);
        if (num_bytes < 0) {
            perror("local recvfrom");
            exit(-1);
        }

        ControlMsg* cmsg = (ControlMsg*)buf;
        handle_control_msg(cmsg);
    }

}