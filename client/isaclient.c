#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>
#include <signal.h>
#include <pwd.h>

#include "isaclient.h"
//#include "api.h"
#include "../api.h"

// function checks and gets arguments
void getArgs(int argc, char* argv[], int* portNumber, char** hostname, char** login);

void prepareHttpHeader(char* command, char* hostname, char* header);

int countChar(char* haystack, const char needle);

void parseCommandForName(char* command, char* name);

void parseCommandForNameAndId(char* command, char* name, char* id);

void concatNameAndId(char* name, char* id);

void putMethodAndUrlAndHostToHeader(char* header, char* method, char* url, char* hostname);

struct Api* newApi(char* command, char* apiEquivalent, int numberOfCommands);

void deleteApi(struct Api* api);

// function finds server and creates socket
void findServer(char **hostname, const int *portNumber, struct hostent* server, struct sockaddr_in *serverAddress, int *clientSocket);

// function connects to server, sends requests and prints responses
void initiateCommunication(const int *clientSocket, struct sockaddr_in serverAddress, char** apiCommand, char** hostname);

// function checks format of messages send through sockets
int checkProtocol(char* message);

int main(int argc, char* argv[]) {
    int clientSocket = 0;
    int portNumber = 0;
    char* hostname = malloc(sizeof(strlen("") + 1));
    hostname[0] = '\0';
    char* command = malloc(sizeof(strlen("") + 1));
    command[0] = '\0';
    char* httpHeader = malloc(sizeof(strlen("") + 1));
    httpHeader[0] = '\0';
    struct hostent server;
    struct sockaddr_in serverAddress;

    // get arguments
    getArgs(argc, argv, &portNumber, &hostname, &command);

    prepareHttpHeader(command, hostname, httpHeader);

    // find server
    findServer(&hostname, &portNumber, &server, &serverAddress, &clientSocket);

    // send request and get response
    initiateCommunication(&clientSocket, serverAddress, &command, &hostname);

    exit(EXIT_CODE_0);
}

/**
 * Function for getting arguments.
 *
 * @param argc argument count
 * @param argv pointer to array of chars
 * @param portNumber port number to be assigned
 * @param hostname host name to be assigned
 * @param n will be set to 1 if program is run with -n argument
 * @param f will be set to 1 if program is run with -f argument
 * @param l will be set to 1 if program is run with -l argument
 * @param login login to be assigned
 */
void getArgs(int argc, char* argv[], int* portNumber, char** hostname, char** login) {
    int arguments;
    opterr = 0;
    char* commandOption = malloc(sizeof(strlen("") + 1));
    commandOption[0] = '\0';
    char* tmp = malloc(sizeof(strlen("") + 1));
    tmp[0] = '\0';

    while ((arguments = getopt(argc, argv, "hH:p:")) != -1 ) {
        switch (arguments) {
            case 'h':
                fprintf(stdout, "Client part for ISA project; try to run with \"-H <host> -p <portnumber> <command>\" arguments\n"
                                "<command> can be any of:\nboards - GET /boards\nboards add <name> - POST /boards/<name>\n"
                                "board delete <name> - DELETE /boards/<name>\nboards list <name> - GET /board/<name>\n"
                                "item add <name> <content> - POST /board/<name>\nitem delete <name> <id> - DELETE /board/<name>/<id>\n"
                                "item update <name> <id> <content> - PUT /board/<name>/<id>\nbe.g. ./isaclient -H localhost -p 1025 boards // this will send GET request to the server to obtain /boards");
                exit(EXIT_CODE_1);
            case 'H':
                *hostname = realloc(*hostname, sizeof(strlen(optarg) + 1));
                strcpy(*hostname,optarg);
                break;
            case 'p' :
                *portNumber = (int) strtol(optarg,&optarg,10);
                if (*portNumber < 1024 || *portNumber > 65535) {
                    fprintf(stderr, "Option -p requires an argument in interval <1024,65535>; provided: %d.\n", *portNumber);
                    exit(EXIT_CODE_1);
                }
                break;
            case '?' :
                if (optopt == 'p') {
                    fprintf(stderr, "Option -p requires an argument in interval <1024,65535>.\n");
                }
                else if (isprint(optopt)) {
                    fprintf(stderr, "Unknown option -%c.\n", optopt);
                }
                exit(EXIT_CODE_1);
            default:

                break;
        }
    }

    // check if number of arguments is correct (minimum is 6)
    if (argc < 6) {
        fprintf(stderr, "Wrong number of parameters, try to run with \"-h\" argument for more details.\n");
        exit(EXIT_CODE_1);
    }

    // find API commands
    // copy first command
    commandOption = realloc(commandOption, sizeof(strlen(argv[optind]) + 1));
    strcpy(commandOption, argv[optind]);
    // each addition command needs to be added with ' '
    int i = optind + 1;
    while (i < argc) {
        tmp = realloc(tmp, sizeof(strlen(commandOption) + 1));
        strcpy(tmp, commandOption);
        commandOption = realloc(commandOption, sizeof(strlen(tmp) + strlen(" ") + strlen(argv[i]) + 1));
        strcpy(commandOption, tmp);
        strcat(commandOption, " ");
        strcat(commandOption, argv[i]);
        i++;
    }
    *login = realloc(*login, sizeof(strlen(commandOption) + 1));
    strcpy(*login, commandOption);

    // free allocated helper variables
    free(tmp);
    free(commandOption);
}

/**
 * Function tries to find appropriate API service for command arguments or it results in error if no API matches the commands.
 *
 * @param command
 *
 * @return appropriate API
 */
void prepareHttpHeader(char* command, char* hostname, char* header) {
    char space = ' ';
    char* name = malloc(sizeof(strlen("") + 1));
    name[0] = '\0';

    if (strstr(command, "item update") != NULL) {
        if (countChar(command, space) == 3) {
            char* id = malloc(sizeof(strlen("") + 1));
            id[0] = '\0';
            parseCommandForNameAndId(command, name, id);
            concatNameAndId(name, id);
            putMethodAndUrlAndHostToHeader(header, PUT_BOARD, name, hostname);
            free(name);
            return;
        }
    } else if (strstr(command, "item delete") != NULL) {
        if (countChar(command, space) == 3) {
            char* id = malloc(sizeof(strlen("") + 1));
            id[0] = '\0';
            parseCommandForNameAndId(command, name, id);
            concatNameAndId(name, id);
            putMethodAndUrlAndHostToHeader(header, POST_BOARD, name, hostname);
            free(name);
            return;
        }
    } else if (strstr(command, "item add") != NULL) {
        if (countChar(command, space) == 3) {
            char* id = malloc(sizeof(strlen("") + 1));
            id[0] = '\0';
            parseCommandForNameAndId(command, name, id);
            concatNameAndId(name, id);
            putMethodAndUrlAndHostToHeader(header, POST_BOARD, name, hostname);
            free(name);
            return;
        }
    } else if (strstr(command, "boards list") != NULL) {
        if (countChar(command, space) == 2) {
            parseCommandForName(command, name);
            putMethodAndUrlAndHostToHeader(header, GET_BOARD, name, hostname);
            free(name);
            return;
        }
    } else if (strstr(command, "board delete") != NULL) {
        if (countChar(command, space) == 2) {
            parseCommandForName(command, name);
            putMethodAndUrlAndHostToHeader(header, DELETE_BOARDS, name, hostname);
            free(name);
            return;
        }
    } else if (strstr(command, "board add") != NULL) {
        if (countChar(command, space) == 2) {
            parseCommandForName(command, name);
            putMethodAndUrlAndHostToHeader(header, POST_BOARDS, name, hostname);
            free(name);
            return;
        }
    } else if (strstr(command, "boards") != NULL) {
        if (countChar(command, space) == 0) {
            // todo
            free(name);
            return;
        }
    }

    fprintf(stderr, "Bad <command> argument; try running with \"-h\" argument for more info.\n");
    exit(EXIT_CODE_1);
}

/**
 * Function counts the occurrences of certain character (needle) in a string (haystack).
 *
 * @param haystack char whose occurrence is counted
 * @param needle string in which char is counted
 *
 * @return number of occurrences of needle in haystack
 */
int countChar(char* haystack, const char needle) {
    int occurrences = 0;

    for (int i = 0; i < strlen(haystack); i++) {
        if (haystack[i] == needle) {
            occurrences++;
        }
    }

    return occurrences;
}

void parseCommandForName(char* command, char* name) {
    int numberOfSpaces = 0;
    char* tmpName = malloc(sizeof(strlen("") + 1));
    tmpName[0] = '\0';

    for (int i = 0; i < strlen(command); i++) {
        if (numberOfSpaces >= 2) {
            // <name> part of command is located behind second space
            if (command[i] != '\0') {
                tmpName = realloc(tmpName, sizeof(strlen(name) + 2));
                strcpy(tmpName, name);
                tmpName[strlen(name)] = command[i];
                tmpName[strlen(name)+1] = '\0';
                name = realloc(name, sizeof(strlen(tmpName) + 1));
                strcpy(name, tmpName);
            }
        } else{
            // look for space characters
            if (command[i] == ' ') {
                numberOfSpaces++;
            }
        }
    }

    free(tmpName);
}

void parseCommandForNameAndId(char* command, char* name, char* id) {
    int numberOfSpaces = 0;
    char* tmpName = malloc(sizeof(strlen("") + 1));
    tmpName[0] = '\0';
    char* tmpId = malloc(sizeof(strlen("") + 1));
    tmpId[0] = '\0';

    for (int i = 0; i < strlen(command); i++) {
        if (command[i] == ' ') {
            numberOfSpaces++;
            continue;
        }
        if (numberOfSpaces >= 3) {
            // <id> part of command is located behind third space
            if (command[i] != '\0') {
                tmpId = realloc(tmpId, sizeof(strlen(id) + 2));
                strcpy(tmpId, id);
                tmpId[strlen(id)] = command[i];
                tmpId[strlen(id) + 1] = '\0';
                id = realloc(id, sizeof(strlen(tmpId) + 1));
                strcpy(id, tmpId);
            }
        } else if (numberOfSpaces >= 2) {
            // <name> part of command is located behind second space
            if (command[i] != '\0') {
                tmpName = realloc(tmpName, sizeof(strlen(name) + 2));
                strcpy(tmpName, name);
                tmpName[strlen(name)] = command[i];
                tmpName[strlen(name) + 1] = '\0';
                name = realloc(name, sizeof(strlen(tmpName) + 1));
                strcpy(name, tmpName);
            }
        }
    }

    free(tmpName);
}

void concatNameAndId(char* name, char* id) {
    char* tmp = malloc(sizeof(strlen(name) + strlen(id) + 2));
    if (tmp == NULL) {
        exit(EXIT_CODE_1);
    }

    strcpy(tmp, name);
    tmp[strlen(name)] = '/';
    strcat(tmp, id);

    name = realloc(name, sizeof(strlen(tmp) + 1));
    strcpy(name, tmp);
    free(tmp);
}

void putMethodAndUrlAndHostToHeader(char* header, char* method, char* url, char* hostname) {
    int methodLen = strlen(method);
    int urlLen = strlen(url);
    int hostnameLen = strlen(hostname);
    char* host = "Host: ";
    int hostLen = strlen(host);
    int headerSize = methodLen + urlLen + strlen(PROTOCOL_VERSION) + hostnameLen + hostLen + 6; // space, \r, \n, \r, \n, \0

    header = realloc(header, sizeof(headerSize));
    bzero(header, headerSize);
    if (header == NULL) {
        exit(EXIT_CODE_1);
    }

    strcpy(header, method);
    strcat(header, url);
    header[methodLen + urlLen] = ' ';
    strcat(header, PROTOCOL_VERSION);
    strcat(header, "\r\n");
    strcat(header, host);
    strcat(header, hostname);
    strcat(header, "\r\n");
    header[headerSize] = '\0';

    free(host);
}

struct Api* newApi(char* command, char* apiEquivalent, int numberOfCommands) {
    struct Api* newApi = malloc(sizeof(struct Api));
    if (newApi == NULL) {
        return NULL;
    }

    newApi->commandArgument = malloc(sizeof(strlen(command) + 1));
    if (newApi->commandArgument == NULL) {
        return NULL;
    }
    strcpy(newApi->commandArgument, command);

    newApi->apiEquivalent = malloc(sizeof(strlen(apiEquivalent) + 1));
    if (newApi->apiEquivalent == NULL) {
        return NULL;
    }
    strcpy(newApi->apiEquivalent, apiEquivalent);

    newApi->numberOfCommands = numberOfCommands;
    return newApi;
}

void deleteApi(struct Api* api) {
    if (api != NULL) {
        free(api->apiEquivalent);
        free(api->commandArgument);
        free(api);
    }
}

/**
 * Function finds server according to hostname argument (either name or IPv4). Function also creates socket.
 *
 * @param hostname host name of server
 * @param portNumber port number of server
 * @param server structure with info about host
 * @param serverAddress structure with socket elements
 * @param clientSocket descriptor of client socket
 */
void findServer(char **hostname, const int *portNumber, struct hostent* server, struct sockaddr_in *serverAddress, int* clientSocket) {
    // "
    if ( (server=gethostbyname(*hostname)) == NULL) {
        fprintf(stderr, "There appears to be no such host.\n");
    }

    bzero(serverAddress, sizeof(&serverAddress));
    serverAddress->sin_family = AF_INET;
    bcopy(server->h_addr, (char *)&serverAddress->sin_addr.s_addr, (size_t)server->h_length);
    serverAddress->sin_port = htons(*portNumber);
    // " inspired by: Rysavy Ondrej - DemoTcp/client.c

    // create socket
    if ( (*clientSocket=socket(AF_INET,SOCK_STREAM,0)) < 0) {
        fprintf(stderr, "Creating socket ended with error.\n");
    }
}

/**
 * Function connects to server, sends request to get specific data and receives response.
 *
 * @param clientSocket socket descriptor
 * @param serverAddress socket address structure of server
 * @param n -n argument of program
 * @param f -f argument of program
 * @param l -l argument of program
 * @param login login of sought user
 */
void initiateCommunication(const int *clientSocket, struct sockaddr_in serverAddress, char** apiCommand, char** hostname){
    ssize_t r = 0;
    char request[BUFSIZ];
    char response[BUFSIZ];
    char message [BUFSIZ];
    bzero(message,BUFSIZ);
    bzero(request,BUFSIZ);
    bzero(response,BUFSIZ);

    // "
    // connect to server
    if ( connect(*clientSocket,(struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        fprintf(stderr, "Connecting to server resulted in error.\n");
        close(*clientSocket);
        exit(1);
    }
    // " inspired by: Rysavy Ondrej - DemoTcp/client.c

    // prepare request for sending

    /*strcpy(request,"**Protocol xvarga14**");
    if ( *n != 0) {
        strcat(request, "n");
        strcat(request, *login);
    } else if ( *f != 0 ) {
        strcat(request, "f");
        strcat(request, *login);
    } else if ( *l != 0 ) {
        strcat(request, "l");
        if ( *login != NULL ) {
            strcat(request, *login);
        }
    }
    strcat(request, "***");*/

    // send request to server
    if ( write(*clientSocket, request, strlen(request)) < 0) {
        fprintf(stderr, "Writing to socket resulted in error.\n");
        close(*clientSocket);
        exit(1);
    }

    // if client was run with -n or -f argument
    /*if ( *l == 0) {
        // get response from server
        if (read(*clientSocket, response, BUFSIZ) < 0) {
            fprintf(stderr, "Reading from socket resulted in error.\n");
            close(*clientSocket);
            exit(1);
        }

        // check if communication is in set protocol
        if ( checkProtocol(response) != 0) {
            fprintf(stderr, "Communication in unknown protocol.\n");
            close(*clientSocket);
            exit(1);
        }

        // print response
        memcpy(message, &response[21], strstr(response, "***")-response-21);
        fprintf(stdout, "%s\n", message);

    } else {
        // if client was run with -l argument
        while ( (r = read(*clientSocket,response, BUFSIZ)) >= 0 && (strcmp(response, "**Protocol xvarga14**No more entries.***")) != 0) {
            // check if communication is in set protocol
            if ( checkProtocol(response) != 0) {
                fprintf(stderr, "Communication in unknown protocol.\n");
                close(*clientSocket);
                exit(1);
            }

            // print response
            memcpy(message, &response[21], strstr(response, "***")-response-21);
            fprintf(stdout, "%s\n", message);

            bzero(response, BUFSIZ);
            bzero(message, BUFSIZ);

            // tell server the message was delivered
            if ( write(*clientSocket, "**Protocol xvarga14**Message delivered.***", 256) < 0) {
                fprintf(stderr, "Writing to socket resulted in error.\n");
                close(*clientSocket);
                exit(1);
            }
        }

        // print error if reading from socket was unsuccessful
        if ( r < 0 ) {
            fprintf(stderr, "Reading from socket resulted in error.\n");
            close(*clientSocket);
            exit(1);
        }

    }*/

    close(*clientSocket);
}

/**
 * Function checks if message format meets the protocol criteria.
 *
 * @param message message to be checked
 * @return 0 if message is OK otherwise return 1
 */
int checkProtocol(char* message) {

    char protocolEnding[4];
    bzero(protocolEnding, 4);
    memcpy(&protocolEnding,strstr(message,"***"),strlen("***")+1);
    char protocolOpening[21];
    bzero(protocolOpening, 21);
    memmove(&protocolOpening,message,21);

    if ( strcmp(protocolOpening,"**Protocol xvarga14**") != 0 && strcmp(protocolEnding, "***") != 0 ) {
        return 1;
    }

    return 0;
}