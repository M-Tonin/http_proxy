/****************************************
 * 489 PA2
 * Harrison Chandler (hchandl)
 * Li Huang (lisepher)
 *
 * proxy.c
 * usage: ./proxy <port>
 *
 *****************************************/

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <signal.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

//Buffer size
const int BUFFER_SIZE = 200000;

//Maximum number concurrent connections (queue length)
const int MAX_CONCURRENT = 50;

/* Function to parse URL, return error code(unspecified yet)
 * url: pointer to URL string
 * hostname: pointer to a string storing server's host name
 * port: pointer to an integer storing server's port number
 */
int parse_url(char *url, char *hostname, int *port)
{
    char abs_path[5000];
    char *temp_url = url;
    int i = 0;
    
    //Check if there is protocol prefix like "http://", "ftp://"
    if(url[0] == 'h'|| url[0] == 'f')
    {
        char c[10];
        memcpy(c, url, 7);
        if(!strcmp(c, "http://"))
            url = &url[7];
        else
        {
            memcpy(c, url, 6);
            if(!strcmp(c, "ftp://"))
            {
                url = &url[6];
                printf("Bad request: ftp not supported!\n");
                return -1;
            }
        }
    }
    
    //Find hostname in URL
    for(i = 0; url[i] != '/' && url[i] != ':' && url[i] != '\0'; i++)
        hostname[i] = url[i];
    hostname[i] = '\0';
    url = &url[i];
    
    //If contains port number
    if(url[0] == ':')
    {
        int j;
        char num[5];
        for(i = 0; url[i] < 48 || url[i] > 57; i++)
        {
            if(url[i] == '\n' || url[i] == '\0')
            {
                printf("Bad URL!\n");
                return -1;
            }
        }
        for(j = i; url[j] > 47 && url[j] < 58; j++)
            num[j - i] = url[j];
        *port = atoi(num);
        
        while(url[j] !='/')
        {
            if(url[j] == '\n' || url[j] == '\0')
            {
                printf("Bad URL: no absolute path\n");
                return -1;
            }
            j++;
        }
        url = &url[j];
    }else *port = 80;
    
    if(url[0] != '/')
    {
        printf("Bad URL: no absolute path\n");
        return -1;
    }
    
    strcpy(abs_path, url);
    strcpy(temp_url, abs_path);
    return 0;
}


/* Function to parse request line, delimiter could be either spaces or horizontal tabs(9 in ASCII)
 * Return error code(unspecified yet)
 * rqst_line: pointer to string of request line
 * hostname: pointer to the string storing server's host name
 * port: pointer to the integer storing server's port number
 */
int parse_rqst_line(char *rqst_line, char *hostname, int *port)
{
    int i = 0, j = 0, isGET;
    char method[10], url[5000], abs_path[5000], version[10]; //Temporarily save method, url and version
    
    //Parse method
    while(rqst_line[i] == ' ' || rqst_line[i] == 9) i++;
    j = i;
    while(rqst_line[i] != ' ' && rqst_line[i] != 9 && rqst_line[i] != '\0')
    {
        if(rqst_line[i] == '\r' || rqst_line[i] == '\n') break;
        if(i - j > 9) break;
        method[i-j] = rqst_line[i];
        i++;
    }
    method[i-j] = '\0';
    isGET = 1;
    if(strcmp(method, "GET"))
    {
        isGET = 0;
        //   printf("Not \"GET\" method!\n");
        return -1;
    }
    
    //Parse URL
    while(rqst_line[i] == ' '|| rqst_line[i] == 9)
        i++;
    j = i;
    while(rqst_line[i] != ' ' && rqst_line[i] != 9 && rqst_line[i] != '\0')
    {
        if(rqst_line[i] == '\r' || rqst_line[i] == '\n')
        {
            //    printf("URL error: Bad request\n");
            return -1;
        }
        url[i-j] = rqst_line[i];
        i++;
    }
    url[i-j] = '\0';
    strcpy(abs_path, url);
    if(parse_url(abs_path, hostname, port))
    {
        //  printf("URL error: Bad request\n");
        return -1;
    }
    
    while(rqst_line[i] == ' ' || rqst_line[i] == 9)
        i++;
    j = i;
    
    //Parse version
    while(rqst_line[i] != ' ' && rqst_line[i] != 9 && rqst_line[i] != '\0')
    {
        if(rqst_line[i] == '\r' || rqst_line[i] == '\n')
        {
            //  printf("Version error: Bad request\n");
            return -1;
        }
        version[i-j] = rqst_line[i];
        i++;
    }
    version[i-j] = '\0';
    if(strcmp(version, "HTTP/1.0"))
    {
        // printf("Not Version 1.0 !\n");
        // strcpy(version, "HTTP/1.0");
    }
    
    //Concatenate three parts to a standard formatted request line, save in rqst_line
    rqst_line[0] = '\0';
    strcat(rqst_line, method);
    strcat(rqst_line, " ");
    if(isGET)
        strcat(rqst_line, abs_path);
    else
        strcat(rqst_line, url);
    strcat(rqst_line, " ");
    strcat(rqst_line, version);
    
    return 0;
}


/* Function to parse header line, return error code
 * hline: pointer to header line
 * hostname: pointer to the string storing server's host name
 */
int parse_hline(char *hline, char *hostname)
{
    int i = 0;
    char h[10] = "host";
    for(; i < 4; i++)
    {
        if(hline[i] != h[i] && hline[i] != h[i] - 32)
            break;
    }
    if(i == 4)
    {
        while(hline[i] == ' ') i++;
        while(hline[i] == ':') i++;
        while(hline[i] == ' ') i++;
        strcpy(hostname, &hline[i]);
    }
    return 0;
}

/* Function to parse request message: divide HTTP request message to lines and parse it line by line,
 * change the format to standar format and save the new request message to new_msg.
 * Return error code.
 * msg_buf: pointer to the message string
 * rqst_length: pointer to the integer storing the lengthe of request message
 * new_msg: pointer to a string used to store the processed and standardly formatted message
 * serv_hostname: pointer to the string storing server's host name
 * serv_port: pointer to the integer storing server's port number
 */
int parse_msg(const char *msg_buf, int *rqst_length, char *new_msg, char *serv_hostname, int *serv_port)
{
    int i = 0, j = 0, n;
    char line[5000];
    
    //Get request line
    while(msg_buf[i] != '\n' && msg_buf[i] != '\0')
    {
        if(msg_buf[i] != '\r')
            line[j++] = msg_buf[i];
        i++;
    }
    line[j] = '\0';
    if(msg_buf[i] != '\0') i++;
    j = 0;
    
    //Parse request line
    new_msg[0] = '\0';
    serv_hostname[0] = '\0';
    n = parse_rqst_line(line, serv_hostname, serv_port);
    if(n)
        return -1;
    strcat(new_msg, line);
    strcat(new_msg, "\r\n");
    
    //Get header lines until reaching empty line(CR LF)
    while(1)
    {
        while(msg_buf[i] != '\n' && msg_buf[i] != '\0')
        {
            if(msg_buf[i] != '\r')
                line[j++] = msg_buf[i];
            i++;
        }
        if(msg_buf[i] != '\0') i++;
        if(j == 0) break; //Empty line, break loop
        line[j] = '\0';
        j = 0;
        
        //Parse every header line
        parse_hline(line, serv_hostname);
        strcat(new_msg, line);
        strcat(new_msg, "\r\n");
    }
    
    strcat(new_msg, "\r\n");
    n = strlen(new_msg);
    for(; i < *rqst_length; i++, n++)
        new_msg[n] = msg_buf[i];
    *rqst_length = n;
    
    return 0;
}

/* Thread to process the request from client: accept request message, connect to server,
 * receive data from server and send data back to client.
 * proxy_listen_skt_fd: file descriptor of the listening socket initialized in main thread
 */
void *proxy_thread(void * arg)
{
    int server_port_num, client_skt_fd, rqst_length, n;
    
    struct sockaddr_in server_addr;
    struct hostent *server;
    char *buffer;
    char *new_msg;
    char *server_host;
    
    buffer = (char *) malloc(sizeof(char) * BUFFER_SIZE);
    new_msg = (char *) malloc(sizeof(char) * BUFFER_SIZE);
    server_host = (char *) malloc(sizeof(char) * BUFFER_SIZE);
    
    int connect_skt_fd = *((int *) arg);
    
    memset(buffer, 0, sizeof(buffer));
    
    rqst_length = read(connect_skt_fd, buffer, BUFFER_SIZE);
    if(rqst_length < 0)
    {
        //    printf("Error receiving request from client\n");
        write(connect_skt_fd, "HTTP/1.0 400 Bad Request\r\n\r\n", 28);
        close(connect_skt_fd);
        
        free(buffer);
        free(new_msg);
        free(server_host);
        
        pthread_exit(NULL);
    }
    
    
    //Process request message saved in buffer, save the processed message in new_msg, server's hostname in server_host,
    //and port number in server_port_num
    new_msg[0] = '\0';
    server_host[0] = '\0';
    n = parse_msg(buffer, &rqst_length, new_msg, server_host, &server_port_num);
    if(n < 0)
    {
        write(connect_skt_fd, "HTTP/1.0 400 Bad Request\r\n\r\n", 28);
        close(connect_skt_fd);
        
        free(buffer);
        free(new_msg);
        free(server_host);
        
        pthread_exit(NULL);
    }
    
    //Look for server ip address
    server = gethostbyname(server_host);
    if (server == NULL)
    {
        //  printf("Cannot find server!\n");
        close(connect_skt_fd);
        
        free(buffer);
        free(new_msg);
        free(server_host);
        
        pthread_exit(NULL);
    }
    memset((char*) &server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port_num);
    bcopy((char*) server->h_addr, (char*) &server_addr.sin_addr.s_addr, server->h_length);
    
    //Connect to server
    client_skt_fd = socket(PF_INET, SOCK_STREAM, 0);
    if(client_skt_fd < 0)
    {
        //  printf("Error opening proxy client socket!\n");
        pthread_exit(NULL);
    }
    n = connect(client_skt_fd, (struct sockaddr*) &server_addr, sizeof(server_addr));
    if(n < 0)
    {
        printf("Cannot connect to server!\n");
        close(client_skt_fd);
        close(connect_skt_fd);
        
        free(buffer);
        free(new_msg);
        free(server_host);
        
        pthread_exit(NULL);
    }
    
    n = write(client_skt_fd, new_msg, rqst_length);
    if(n < 0)
    {
        //  printf("Error sending to server!\n");
        close(client_skt_fd);
        close(connect_skt_fd);
        
        free(buffer);
        free(new_msg);
        free(server_host);
        
        pthread_exit(NULL);
    }
    
    memset(buffer, 0, sizeof(buffer));
    int isReading = read(client_skt_fd, buffer, BUFFER_SIZE);
    while (isReading > 0)
    {
        int isWriting = write(connect_skt_fd, buffer, isReading);
        if(isWriting < 0)
        {
            //	    printf("Error sending to client!\n");
            close(client_skt_fd);
            close(connect_skt_fd);
            
            free(buffer);
            free(new_msg);
            free(server_host);
            
            pthread_exit(NULL);
        }
        memset(buffer, 0, sizeof(buffer));
        isReading = read(client_skt_fd, buffer, BUFFER_SIZE);
    }
    if (isReading < 0)
    {
        //    printf("ERROR reading from socket\n");
        close(client_skt_fd);
        close(connect_skt_fd);
        
        free(buffer);
        free(new_msg);
        free(server_host);
        
        pthread_exit(NULL);
    }
    
    close(client_skt_fd);
    close(connect_skt_fd);
    
    free(buffer);
    free(new_msg);
    free(server_host);
    
    pthread_exit(NULL);
}



int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }
    
    // Port number, listening socket file descriptor, connection socket file discriptor and proxy address
    int proxy_port_num, n;
    
    long proxy_listen_skt_fd;
    struct sockaddr_in proxy_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    
    signal(SIGPIPE, SIG_IGN);
    
    proxy_listen_skt_fd = socket(AF_INET, SOCK_STREAM, 0); // Create listening socket
    if(proxy_listen_skt_fd < 0)
    {
        printf("Error opening proxy listening socket!\n");
        exit(1);
    }
    
    //Proxy address
    proxy_port_num = atoi(argv[1]);
    bzero((char*) &proxy_addr, sizeof(proxy_addr));
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_addr.s_addr = INADDR_ANY;
    proxy_addr.sin_port = htons(proxy_port_num);
    
    n = bind(proxy_listen_skt_fd, (struct sockaddr*) &proxy_addr, sizeof(proxy_addr));
    if(n < 0)
    {
        printf("Binding error!\n");
        exit(1);
    }
    
    listen(proxy_listen_skt_fd, MAX_CONCURRENT);
    
    while (1)
    {
        client_len = sizeof(client_addr);
        pthread_t thread;
        
        int connect_skt_fd = accept(proxy_listen_skt_fd, (struct sockaddr*) &client_addr, &client_len); //Accept request, create a connection socket
        n = pthread_create(&thread, NULL, proxy_thread, &connect_skt_fd);
        if(n)
        {
            printf("Error creating thread!\n");
        }
    }
}
