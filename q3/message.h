#define MAX_LEN 128

// Protocols
#define CLIENT_MESSAGE 1
#define GROUP_MESSAGE 2
#define REGISTER 3
#define GROUP_JOIN 4
#define QUERY_REQUEST 5
#define QUERY_RESPONSE 6
#define CONTROL 7
#define CONTROL_RESPONSE 8

// Query Types
#define QUERY_CLIENT 1
#define QUERY_GROUP 2

// Control Types
#define REGISTER_CLIENT 1
#define JOIN_GROUP 2
#define LEAVE_GROUP 3
#define CREATE_GROUP 4

// Control Statuses
#define STATUS_OK 200
#define STATUS_ERROR 500

typedef struct Messages {
    long  mtype;
    int  protocol;
    int  src;
    int  dst;
    long auto_delete;
    long timestamp;
    int  status;
    char src_name[MAX_LEN];
    char content[2 * MAX_LEN];
} Message;

typedef struct QueryRequests {
    long  mtype;
    int  protocol;
    int  src;
    int  query_type;
    char content[MAX_LEN];
} QueryRequest;

typedef struct QueryResponses {
    long  mtype;
    int status;
    char content[MAX_LEN * 2];
} QueryResponse;

typedef struct ControlMessages {
    long  mtype;
    char queuepath[MAX_LEN]; // For registering new clients
    char name[MAX_LEN];      // For registering new clients
    int  action;
    int  src;
    int  gid;                // For joining or leaving groups
} ControlMessage;

typedef struct ControlResponse {
    long mtype;
    long action;
    int status;
    int gid;
} ControlResponse;