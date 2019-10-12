#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>



/**
 * file server.c
 * @author Miriam Gehbauer <e11708473@student.tuwien.ac.at>
 * @date 11.04.2019
 * @brief The server waits for connections from clients and transmits the requested files.
 *
 * @details The server waits for connections from clients and transmits the requested files.
 *Option -p can be used to specify the port on which the server shall listen for incoming connections.If this option is not used the port defaults to 8080 (port 80 requires root privileges).
 * Option -i is used to specify the index filename, i.e. the file which the server shall attempt to transmit if the request path is a directory. The default index filename is index.html.
 **/


#define BINARY_BUFFER_LEN 1024 * 1024
#define MAX_CHAR_LEN 2048

static char *program_name;
static int isListening = 1;

/**
* @brief checks if Directory exists
* @details checks if Directory exists with open the directory and close it
* @param dirName: Name of the Directory
* @return 1 if the Directory exists and else returns 0
**/
static int checkDirExists(char *dirName) {
    DIR *dir = opendir(dirName);
    if (dir) {
        /* Directory exists. */
        closedir(dir);
        return 1;
    } else {
        return 0;
    }
}

/**
* @brief checks if the Port is valid
* @details a vaild Port must be between 1 and 65535
* @param strPort: Port as a String
**/
static void checkValidPort(char *strPort) {
    char *ptr;
    long port;
    port = strtol(strPort, &ptr, 10);
    if (port < 1 || port > 65535) {
        fprintf(stderr, "Error in %s:Port must be between 1 and 65535 \n", program_name);
        exit(EXIT_FAILURE);
    }
}


/**
* @brief checks if options are correct
* @details checks if options are correct and if each option ist only one time given
* @param p: option p
* @param i: option i
**/
static void checkOptions(int p, int i) {
    if (p > 1) {
        fprintf(stderr, "Error in %s: Too many Ports\n", program_name);
        exit(EXIT_FAILURE);
    }

    if (i > 1) {
        fprintf(stderr, "Error in %s: Too many index \n", program_name);
        exit(EXIT_FAILURE);
    }
}


/**
* @brief set the current Date
* @details set the current Date and Time
* @param date: variable in which the Date will be saved
* @param errorMsg: errorMsg which will be send
**/
static void setCurrentDate(char *date) {
    time_t now = time(&now);
    struct tm *tm = gmtime(&now);
    strftime(date, MAX_CHAR_LEN, "%a, %d %b %y %H:%M:%S %Z", tm);
}

/**
* @brief sends a Http Response Error
* @details sends a Http Response Error with a specific Error message
* @param sockfile: file for the communication between server and client
* @param errorMsg: errorMsg which will be send
**/
static void sendHttpResponseError(FILE *sockfile, char *errorMsg) {
    fprintf(stderr, "%s", errorMsg);
    char responseError[100] = {0};
    sprintf(responseError, "HTTP/1.1 %s\r\n"
                           "Connection: close\r\n\r\n", errorMsg);

    fputs(responseError, sockfile);
    fflush(sockfile); // send all buffered data

}

/**
* @brief sends a Http Response Header
* @details sends a Http Response Header with a specific filesize
* @param sockfile: file for the communication between server and client
* @param fileSize: size from the response File
**/
static void sendHttpResponseHeader(FILE *sockfile, int fileSize) {
    fprintf(stderr, "200 OK");
    char date[MAX_CHAR_LEN];
    setCurrentDate(date);
    char responseHeader[1000] = {0};

    sprintf(responseHeader, "HTTP/1.1 200 OK\r\n"
                            "Date: %s\r\n"
                            "Content-Lenght: %d\r\n"
                            "Connection: close\r\n\r\n", date, fileSize);

    fputs(responseHeader, sockfile);
    fflush(sockfile); // send all buffered data

}

/**
* @brief checks the fist line of the Http Request Header
* @details checks the fist line of the Http Request Header if the request method and the protocol is correct
* @param sockfile: file for the communication between server and client
* @param header_line: first line of the header
* @param requestFilename: filename from the requested File
* @return 1 if the Header is correct  and 0 if the Header is not correct and a error message was send
**/
static int checkRequestHeaderAndGetFilename(FILE *sockfile, char *header_line, char *requestFilename) {
    char *requestMethod;
    requestMethod = strtok(header_line, " ");
    char *filename;
    filename = strtok(NULL, " ");

    strcat(requestFilename, filename);
	
    if (strcmp(requestMethod, "GET") != 0) {
        char *errorMsg = "501 Not implemented";
        sendHttpResponseError(sockfile, errorMsg);
        return 0;
    }

    char *protocol;
    protocol = strtok(NULL, " ");

    if (strcmp(protocol, "HTTP/1.1\r\n") != 0) {
        //getErrorMessage(400, errorMsg);
        char *errorMsg = "400 Bad Request";
        sendHttpResponseError(sockfile, errorMsg);
        return 0;
    }

    return 1;
}

/**
* @brief reads the Header from the request
* @details reads the Header from the request and get the Filename from the response File
* @param sockfile: file for the communication between server and client
* @param requestFilename: filename from the requested File
* @return 1 if the Header is correct  and 0 if the Header is not correct and a error message was send
*/
static int readRequestHeaderAndGetFilename(FILE *sockfile, char *requestFilename) {
    char header_line[MAX_CHAR_LEN];
    char first_header_line[MAX_CHAR_LEN] = "";

    //read html status line
    fgets(header_line, sizeof(header_line), sockfile);
    strcat(first_header_line, header_line);

    //read header line by line until last header line "\r\n"
    while (strcmp(header_line, "\r\n") != 0) {
        fgets(header_line, sizeof(header_line), sockfile);
    }

    return checkRequestHeaderAndGetFilename(sockfile, first_header_line, requestFilename);
}


/**
* @brief This function returns the size of a file.
* @details This function returns the size of a file.
* @param file
* @return returns the size of a file.
*/
static unsigned long fsize(char *file) {
    FILE *f = fopen(file, "r");
    fseek(f, 0, SEEK_END);
    unsigned long len = (unsigned long) ftell(f);
    fclose(f);
    return len;
}


/**
* @brief get the path of the requested File
* @details get the path of the requested File in the root directory
* @param requestedFile: File which will be send
* @param DocRoot: directory of the server
* @param index: name of the index file
* @param requestedFilepath: path of the requested File in the root directory
*/
static void getRequestedFilepath(char *requestedFilepath, char *DocRoot, char *requestedFilename, char *index) {
    strcat(requestedFilepath, DocRoot);
    strcat(requestedFilepath, "/");
    if (strcmp(requestedFilename, "/") == 0) { //send index file
        strcat(requestedFilepath, index);
    } else {
        strcat(requestedFilepath, requestedFilename);
    }
}

/**
* @brief send requested File
* @details send requested File to the sockedfile
* @param requestedFile: File which will be send
* @param sockfile: file for the communication between server and client
*/
static void sendFile(FILE *sockfile, FILE *requestedFile) {
    uint8_t binary_buffer[BINARY_BUFFER_LEN];
    while (!feof(requestedFile)) {
        ssize_t n = fread(binary_buffer, sizeof(uint8_t), BINARY_BUFFER_LEN, requestedFile);
        fwrite(binary_buffer, sizeof(uint8_t), n, sockfile);
    }
}

/**
* @brief  starts the server
* @details  starts the server and build a connection
* @param port: port from the Server
*/
static int getConnection(char * port) {
    struct addrinfo hints, *ai;
    memset(&hints,0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int s = getaddrinfo(NULL, port, &hints, &ai);
    if (s != 0) {
        freeaddrinfo(ai);
        fprintf(stderr,
                "ERROR in %s: getaddrinfo failed: %s", program_name,
                gai_strerror(s)
        );
        exit(EXIT_FAILURE);
    }

    int sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sockfd < 0) {
        freeaddrinfo(ai);
        fprintf(stderr,
                "ERROR in %s: Create Socket Failed", program_name);
        exit(EXIT_FAILURE);
    }

//avoid EADDRINUSE
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

    if (bind(sockfd, ai->ai_addr, ai->ai_addrlen) < 0) {
        freeaddrinfo(ai);
        fprintf(stderr,"ERROR in %s: Bind Failed", program_name);
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd,10) < 0) {
        freeaddrinfo(ai);
        fprintf(stderr,"ERROR in %s: listen Failed", program_name);
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(ai);
    return sockfd;
}

/**
* @brief  communication with the client
* @details  communication with the client and send teh requested File
* @param fd_client: value of the client
* @param docRoot: directory of the server
* @param index: name of the index file
*/
static void communicateWithClient(int fd_client, char *docRoot, char *index) {
    FILE *sockfile = fdopen(fd_client, "r+");
    char requestFilename[MAX_CHAR_LEN] = "";

    int headerValid = readRequestHeaderAndGetFilename(sockfile, requestFilename);

    if (headerValid) {

        char requestedFilepath[MAX_CHAR_LEN] = "";
        getRequestedFilepath(requestedFilepath, docRoot, requestFilename, index);

        FILE *requestedFile = fopen(requestedFilepath, "r");

        if (requestedFile == NULL) {
            char *errorMsg = "404 Not Found";
            sendHttpResponseError(sockfile, errorMsg);
        } else {
            int fileSize = fsize(requestedFilepath);
            sendHttpResponseHeader(sockfile, fileSize);
            sendFile(sockfile, requestedFile);
        }
    }
    fclose(sockfile);
}

/**
 * @brief  handle all signals
 * @param signal: sinal which will be handled
 */
static void handle_signal(int signal) {
    isListening = 0;
}

/**
 * @brief  setup the signal handler
 **/
static void setup_signal_handlers(void) {
    //initial signal
    struct sigaction sa_sigint, sa_sigterm;
    memset(&sa_sigint, 0, sizeof sa_sigint);
    memset(&sa_sigterm, 0, sizeof sa_sigterm);

    //function for signal
    sa_sigint.sa_handler = handle_signal;
    sa_sigterm.sa_handler = handle_signal;

    //signal error
    if (sigaction(SIGINT, &sa_sigint, NULL) != 0 || sigaction(SIGTERM, &sa_sigterm, NULL) != 0) {
        fprintf(stderr, "Error in %s : signal error\n", program_name);
        exit(EXIT_FAILURE);
    }
}

/**
 * Program entry point.
 * @brief The program starts here. This function represent the Server. 
 * @details he server waits for connections from clients and transmits the requested files.
 *Option -p can be used to specify the port on which the server shall listen for incoming connections.If this option is not used the port defaults to 8080 (port 80 requires root privileges).
 * Option -i is used to specify the index filename, i.e. the file which the server shall attempt to transmit if the request path is a directory. The default index filename is index.html.
 * @param argc The argument counter.
 * @param argv The argument vector.
 * @return Returns <code>EXIT_SUCCESS</code> on success, <code>EXIT_FAILURE</code> otherwise.
 */
int main(int argc, char *argv[]) {
    program_name = argv[0];
    setup_signal_handlers();
    char *port = "8080";
    char *index = "index.html";

    // --------------------------------getOpt---Begin----------------------------------
    int opt_p = 0;
    int opt_i = 0;
    int opt;

    while ((opt = getopt(argc, argv, "p:i:")) != -1) {
        switch (opt) {
            case 'p': //option p is given
                opt_p += 1;
                port = optarg;
                break;
            case 'i': //option o is given
                opt_i += 1;
                index = optarg;
                break;
            default: /* '?' */ //somiting wrong ist given
                fprintf(stderr, "Usage: %s [-p PORT] [-i INDEX] DOC_ROOT\n", program_name);
                return EXIT_FAILURE;
        }
    }

    if (optind + 1 != argc) { //unspecified options or too many arguments
        fprintf(stderr,
                "Error %s: unspecified options or too many arguments \nUsage: %s [-p PORT] [-i INDEX] DOC_ROOT\n",
                program_name, program_name);
        return EXIT_FAILURE;
    }

    checkOptions(opt_p, opt_i);
    checkValidPort(port);


    //------------------check dir----------------
    char *docRoot = argv[optind];
    if (!checkDirExists(docRoot)) {
        fprintf(stderr, "Error in %s: Directory does not exist\n", program_name);
        exit(EXIT_FAILURE);
    }

    //------------connect to client---------------------

    int sockfd = getConnection(port);

    fprintf(stderr, "Listening on http://localhost:%s ...\n", port);

    while (isListening) {
        int fd_client = accept(sockfd, NULL, NULL);
        if (fd_client < 0) {
            close(fd_client);
            break;
        }
        fprintf(stderr, "Get Request from Client - Send Response with Status ");
        communicateWithClient(fd_client, docRoot, index);
        fprintf(stderr, " - Closed Connection to Client\n");
        close(fd_client);
    }


    //cleanup
    close(sockfd);
    fprintf(stderr, "\nShutdown Server\n");
    exit(EXIT_SUCCESS);
}