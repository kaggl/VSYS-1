#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h> //for exit
#include <dirent.h>
#include <errno.h>
/**
 * file client.c
 * @author Miriam Gehbauer <e11708473@student.tuwien.ac.at>
 * @date 25.03.2019
 * @brief The client takes an URL as input, connects to the corresponding server and requests the file specified in the URL.
 *
 * @details The client takes an URL as input, connects to the corresponding server and requests the file specified in the URL.
 * Option -p can be used to specify the port on which the client shall attempt to connect to the
 * server. If this option is not used the port defaults to 80, which is the standard port for HTTP.".
 * Either option -o or -d can be used to write the requested content to a file. Option -o is used
 * to specify a filename to which the transmitted content is written. Option -d is used to specify
 * a directory in which a file of the same name as the requested file is created and filled with the
 * transmitted content. If none of these options is given, the transmitted content is written to stdout.
 **/

#define BINARY_BUFFER_LEN 1024 * 1024
#define MAX_CHAR_LEN 2048

static char *program_name;

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
* @brief checks if the String is a Number
* @details checks if the String only contains Digits
* @param str: String which should only contains Numbers
* @return 1 if the String only contains Digits and else returns 0
**/
static int isDigitsOnly(char *str) {
    int counter = 0;
    while (str[counter] != '\0') {
        if (str[counter] < '0' || str[counter] > '9') {
            return 0;
        }
        counter++;
    }
    return 1;
}

/**
* @brief checks if the String ends with a Slash
* @details checks if the String ends with a Slash
* @param dirName: Name of the Directory
* @return 1 if the String ends with a Slash and else returns 0
**/
static int checkLastCharIsSlash(char *str) {
    if (str[(strlen(str) - 1)] == '/') {
        return 1;
    }
    return 0;
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
        fprintf(stderr, "Error in %s:Port must be between 1 nad 65535 \n", program_name);
        exit(EXIT_FAILURE);
    }
}

/**
* @brief checks if options are correct
* @details checks if options are correct and if each option ist only one time given
* @param p: option p
* @param o: option o
* @param d: option d
**/
static void checkOptions(int p, int o, int d) {
    if (o > 0 && d > 0) {
        fprintf(stderr, "Error in %s:Output File and Output Directory: Use only one of them \n", program_name);
        exit(EXIT_FAILURE);
    }

    if (p > 1) {
        fprintf(stderr, "Error in %s: Too many Ports\n", program_name);
        exit(EXIT_FAILURE);
    }


    if (o > 1) {
        fprintf(stderr, "Error in %s: Too many Output Files\n", program_name);
        exit(EXIT_FAILURE);
    }

    if (d > 1) {
        fprintf(stderr, "Error in %s: Too many Output Directory\n", program_name);
        exit(EXIT_FAILURE);
    }

    if (o == 1) {
        if (stdout == NULL) // Files not open
        {
            fprintf(stderr, "Error in %s: Output File not open\n", program_name);
            exit(EXIT_FAILURE);
        }
    }
}

/**
* @brief send a request header
* @details send a request header to the given Host for the a specific file
* @param sockfile: file for the communication between server and client
* @param host: host to sent the requested file
* @param file: file which the client will have
**/
static void sendRequestHeader(FILE *sockfile, char *host, char *file) {
    char requestheader[100] = {0};
    sprintf(requestheader, "GET /%s HTTP/1.1\r\nHost: "
                           "%s\r\n"
                           "Connection: close\r\n\r\n", file, host);
    fputs(requestheader, sockfile);
    fflush(sockfile); // send all buffered data
}

/**
@brief check the Header from the response
@details check the Header from the response if the Protocol and the Status code are correct
@param header_line: first line of the Header
*/
static void checkHeader(char *header_line) {
    char *protocol;
    protocol = strtok(header_line, " ");
    char *statusNr;
    statusNr = strtok(NULL, " ");

    if (strcmp(protocol, "HTTP/1.1") != 0 || isDigitsOnly(statusNr) != 1) {
        fprintf(stderr, "Error in %s: Protocol error! \n", program_name);
        exit(EXIT_FAILURE);
    }

    //print error if Status != 200
    char *ptr;
    long statusNumber;
    statusNumber = strtol(statusNr, &ptr, 10);
    fprintf(stderr, "Get Response with Status %s\n", statusNr);
    if (statusNumber != 200) {
        fprintf(stderr, "Error in %s: %s ", program_name, statusNr);
        char *status;
        status = strtok(NULL, " ");
        while (status != NULL) {
            fprintf(stderr, "%s ", status);
            status = strtok(NULL, " ");
        }
        fprintf(stderr, "\n");
        exit(3);
    }
}

/**
* @brief reads the Header from the response
* @details reads and check the Header from the response
* @param sockfile: file for the communication between server and client
*/
static void readResponseHeader(FILE *sockfile) {
    char header_line[2048];

    //read html status line and check
    fgets(header_line, sizeof(header_line), sockfile);
    checkHeader(header_line);

    //read header line by line until last header line "\r\n"
    while (strcmp(header_line, "\r\n") != 0) {
        fgets(header_line, sizeof(header_line), sockfile);
    }
}

/**
* @brief create the directory
* @details create the directory in which the response file will be saved
* @param path: path where the response file will be saved
* @param lastCharIsSlash: the value is 1 if the last char from the url is a Slash else the value ist 0
* @param filename:  name of the response file
*/
static void createDir(char *path, int lastCharIsSlash, char *filename) {
    if (lastCharIsSlash) {
        strcat(path, "/index.html");
    } else {
        int slashPos = -1;
        for (int i = 0; i < strlen(filename); ++i) {
            if (filename[i] == '/') {
                slashPos = i;
            }
        }
        char *filenameForDir = filename;
        if (slashPos == -1) {
            strcat(path, "/");
            strcat(path, filenameForDir);
        } else {
            filenameForDir += slashPos;
            strcat(path, filenameForDir);
        }
    }
}

/**
* @brief  connect to Server
* @details connect to Server with a specific hostname and Port
* @param hostname: hostname from the Server
* @param port: port from the Server
* @return the value of the socket
*/
static int connectToServer(char *hostname, char *port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sockfd;


    /* Obtain address(es) matching host/port */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */


    int res = getaddrinfo(hostname, port, &hints, &result);
    if (res != 0) {
        fprintf(stderr, "Error in %s: getaddrinfo: %s\n", program_name, gai_strerror(res));
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype,
                        rp->ai_protocol);
        if (sockfd == -1)
            continue;

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */
    }

    if (rp == NULL) {               /* No address succeeded */

        fprintf(stderr, "Error in %s: Could not connect\n", program_name);
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(result);
    return sockfd;


}


/**
* @brief get response File
* @details get response File and save it to stdout
* @param hostname: hostname from the Server
* @param sockfile: file for the communication between server and client
*/
static void getResponseFile(FILE *sockfile) {
    uint8_t binary_buffer[BINARY_BUFFER_LEN];
    while (!feof(sockfile)) {
        ssize_t n = fread(binary_buffer, sizeof(uint8_t), BINARY_BUFFER_LEN, sockfile);
        fwrite(binary_buffer, sizeof(uint8_t), n, stdout);
    }
}

/***
 * Program entry point.
 * @brief The program starts here. This function represent the Client . 
 * @details The client takes an URL as input, connects to the corresponding server and requests the file specified in the URL.
 *Option -p can be used to specify the port on which the client shall attempt to connect to the
 * server. If this option is not used the port defaults to 80, which is the standard port for HTTP.".
 * Either option -o or -d can be used to write the requested content to a file. Option -o is used
 * to specify a filename to which the transmitted content is written. Option -d is used to specify
 * a directory in which a file of the same name as the requested file is created and filled with the
 * transmitted content. If none of these options is given, the transmitted content is written to stdout. 
 *@param argc The argument counter.
 * @param argv The argument vector.
 * @return Returns <code>EXIT_SUCCESS</code> on success, <code>EXIT_FAILURE</code> otherwise.
 */

int main(int argc, char *argv[]) {
    program_name = argv[0];
    char *port = "80";

    int opt_p = 0;
    int opt_o = 0;
    int opt_d = 0;
    char *dir;

    int opt;

    // --------------------------------getOpt-------------------------------------
    while ((opt = getopt(argc, argv, "p:o:d:")) != -1) {
        switch (opt) {
            case 'p': //option p is given
                opt_p += 1;
                port = optarg;
                break;
            case 'o': //option o is given 
                opt_o += 1;
                stdout = fopen(optarg, "w");
                break;
            case 'd': //option d is given
                opt_d += 1;
                dir = optarg;
                break;
            default: /* '?' */ //somiting wrong ist given 
                fprintf(stderr, "Usage: %s [-p PORT] [ -o FILE | -d DIR ] URL\n", program_name);
                return EXIT_FAILURE;
        }
    }

    if (optind + 1 != argc) { //unspecified options or too many arguments
        fprintf(stderr,
                "Error %s: unspecified options or too many arguments \nUsage: %s [-p PORT] [ -o FILE | -d DIR ] URL\n",
                program_name, program_name);
        return EXIT_FAILURE;
    }

    checkOptions(opt_p, opt_o, opt_d);
    checkValidPort(port);

    // ------------------------get Hostname and Filename----------------------------------

    char *url = argv[optind];
    fprintf(stderr, "Send Request for %s - ", url);
    char *hostname;
    char *filename;
    int lastCharIsSlash = checkLastCharIsSlash(url);
    char *http = "http://";

    if (strncmp(url, http, strlen(http)) != 0) {
        fprintf(stderr, "Error in %s: Invalid Url\n", program_name);
        exit(EXIT_FAILURE);
    }

    url += strlen(http);


    int delimiterPos = -1;
    for (int i = 0; i < strlen(url); ++i) {
        if (url[i] == '/' ||
            url[i] == ';' ||
            url[i] == '?' ||
            url[i] == ':' ||
            url[i] == '@' ||
            url[i] == '=' ||
            url[i] == '&'
                ) {
            delimiterPos = i;
            break;
        }
    }

    if (delimiterPos == -1) {
        fprintf(stderr, "Error in %s: Invalid Url\n", program_name);
        exit(EXIT_FAILURE);
    }

    filename = url;
    filename += (delimiterPos + 1);
    hostname = url;
    hostname[delimiterPos] = '\0';


//------------create Dir---------------

    if (opt_d) {
        if (checkDirExists(dir)) {
            char *pathWithFilename = strdup(dir);
            createDir(pathWithFilename, lastCharIsSlash, filename);
            stdout = fopen(pathWithFilename, "w");
        } else {
            fprintf(stderr, "Error in %s: Directory does not exist\n", program_name);
            exit(EXIT_FAILURE);
        }
    }

//---------------------connect to server--------------

    int sockfd = connectToServer(hostname, port);

    FILE *sockfile = fdopen(sockfd, "r+");

    sendRequestHeader(sockfile, hostname, filename);
    readResponseHeader(sockfile);
    getResponseFile(sockfile);

    close(sockfd);
    fclose(stdout);
    return EXIT_SUCCESS;
}

