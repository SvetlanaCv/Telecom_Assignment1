/*
 Shawn Zamechek 
 Web Proxy
 */
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include "mysocket.h"
#include <time.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define MAX_REQUEST 8192
#define MAX_URL 2048
#define WEBPORT 80
#define MAX_MSG 200
#define BUFFLEN 500
#define MAX_STR_LEN 100

char* internet= "C:\\Program Files\\internet explorer\\iexplore.exe";

/*
 This struct contains the info that is passed to a thread.
 */
struct args 
{
    int id;	//thread id
    int connfd;	//client address
    char buffer[MAX_REQUEST];	//client reuqest
};

struct cacheObject{	//cache object for linked list
	char request[MAX_MSG]; //client request
	char message[MAX_MSG]; //message to be returned to client
	char site[MAX_MSG];	//browser site
	struct cacheObject * next; //next cache object
};

void makeCache();
struct cacheObject* inCache(char request[MAX_MSG]);
void addCache(struct cacheObject* obj);
void printCache();

struct cacheObject* start;
/*
 This struct is filled from data from the remote http request. It contains the whole remote request, the method, path, 
 version, the host and page or file.
 */
struct req
{
    char request[MAX_REQUEST];
    char method[16];
    char path[MAX_URL];
    char version[16];
    char host[MAX_URL];
    char page[MAX_URL];
};

void *handleConnection(void *args);
void getReqInfo(char *request, struct req *myreq);
//void returnFile(int socket, char* filepath);
void sendRemoteReq(char filename[MAX_URL], char host[MAX_URL], int socket, char path[MAX_URL], char buffer[MAX_REQUEST]);
FILE *fp_log; //log
void checkForBlock();
FILE *blocklist; //list of blocked URLs

//typedef struct node //node for blocking linked list
//{
//    char val[MAX_STR_LEN];
//    struct node * next;
//} Node_t;


/*
 Main setup the socket for listening and enters an infinate while loop. 
 When a connection arrives it spawns a thread and passes the request to the handleConnection function
 */
int main(int argc, char** argv) {
	    if(strcmp(argv[1], "block") == 0){	//if argv is "block" then initiate blocking function
			checkForBlock();
			return 0;
		}
    else {	//else function normally
		time_t result;
		makeCache();	//initiate cache
		result = time(NULL);
		struct tm* brokentime = localtime(&result);
		int port = atoi(argv[1]);
		int srvfd  = makeListener(port); //listen from given port number
		int threadID = 0;	//count threads
		fp_log = fopen("server.log", "a");	//open log
		fprintf(fp_log, "Server started at %s \nListening on Port %d\n",asctime(brokentime), port);	
		printf("Server started at %s \nListening on Port %d\n",asctime(brokentime), port);	
		fclose(fp_log);
		struct args* myargs;	//prepare client info
		myargs = (struct args*)malloc(sizeof(struct args));
		while (1)
		{
			myargs->connfd = listenFor(srvfd);	//listen for client
			fp_log = fopen("server.log", "a");
			pthread_t thread;
			threadID++;
			myargs->id = threadID;
			pthread_create(&thread, NULL, handleConnection, myargs); //once thread is made, handle request
			fprintf(fp_log, "Thread %d created\n", myargs->id); 
			printf("Thread %d created\n", myargs->id);
		}
		free(myargs);
		printf("Connection closed");
		close(srvfd);
	}	
    return 0;
}
/*
 Handels the connection by passing the request to the getReqInfo function for parsing 
 and then retuns the request http page via the sendRemoteReq() function.
 */
void *handleConnection(void *args)
{
    clock_t start = clock();
    struct args* myarg = (struct args*) args;
    struct req* myreq;
    myreq = (struct req*) malloc(sizeof(struct req));
    int bytesRead;
    printCache();	
    bzero(myarg->buffer, MAX_REQUEST);
    bytesRead = read(myarg->connfd, myarg->buffer, sizeof(myarg->buffer));	//read client request
    char request[MAX_MSG];
    strncpy(request, myarg->buffer, MAX_MSG-1);	//parse request
    //printCache();
    struct cacheObject* obj;
    obj = inCache(request);	//check if request exists in cache
    if(obj!=NULL){	//if so
	printf("FOUND CACHED DATA. SENDING ON TO CLIENT.\n");
	write(myarg->connfd, obj->message, MAX_MSG);	//send previous info to cache
	printf("DATA SENT. OPENING BROWSER.\n");
	char* prog[3];	//and open web browser
    	prog[0] = internet;
    	prog[1] = obj->site;
    	prog[2] = '\0';
        //execvp(prog[0], prog);
    }
    else{	//otherwise, continue as normal
    	getReqInfo(myarg->buffer, myreq);	//parses all information from request
	blocklist = fopen("server.blocklist", "r");	//open blocked URL file
	if(blocklist == NULL) {printf("ERROR: CANNOT OPEN FILE");}
	char buff[BUFFLEN];				//the following code parses file info into data
	char *input = calloc(BUFFLEN, sizeof(char));
	while(fgets(buff, sizeof(buff), blocklist))
	{
		input = realloc(input, strlen(input)+1+strlen(buff));
		strcat(input,buff); 
	} 
	free(input);
	char delim[] = "\n";
	char *data[sizeof(input)+1];
	char *ptr = strtok(input, delim);
	int i = 0;
	while(ptr != NULL){				//check if any value in data matches, if so, cancel thread
		data[i] = ptr;
		if(data[i]!=NULL){
			printf("The data is: %s\n", data[i]);
			ptr = strtok(NULL, delim);
			if(strcmp(data[i], myreq->host)==0){
				printf("THE REQUEST CLIENT ENTERED IS ON THE BLOCKED LIST.\n ABORT THREAD...\n");
				free(myreq);
				write(myarg->connfd, "Requested URL is blocked.", MAX_MSG);
				fprintf(fp_log, "Thread %d exits\n", myarg->id);
    				printf("Thread %d exits\n", myarg->id);
				pthread_exit(NULL);
			}
		}
		i++;
	}  
    	sendRemoteReq(myreq->page, myreq->host, myarg->connfd, myreq->path, request);	//sends the request to a web browser
    }
    int diff = clock() - start;	//measures timing
    printf("Function took: %dms\n", diff);
    printCache();
    fprintf(fp_log, "Thread %d exits\n", myarg->id); 
    printf("Thread %d exits\n", myarg->id);
    free(myreq);	//frees up malloc'd space
    pthread_exit(NULL);	//exits thread
}

void getReqInfo(char *request, struct req *myreq)	//parses the HTTP request and reports back
{
    char host[MAX_URL], page[MAX_URL];
    strncpy(myreq->request, request, MAX_REQUEST-1);
    strncpy(myreq->method, strtok(request, " "), 16-1);
    strncpy(myreq->path, strtok(NULL, " "), MAX_URL-1);
    strncpy(myreq->version, strtok(NULL, "\r\n"), 16-1);
    if(strstr(myreq->request, "https")!=NULL){printf("FOUND HTTPS");sscanf(myreq->path, "https://%99[^/]%99[^\n]", host, page);}
    else {sscanf(myreq->path, "http://%99[^/]%99[^\n]", host, page);}
    strncpy(myreq->host, host, MAX_URL-1);
    strncpy(myreq->page, page, MAX_URL-1);
    fprintf (fp_log, "method: %s\nversion: %s\npath: %s\nhost: %s\npage: %s\n", myreq->method, myreq->version, myreq->path, myreq->host, myreq->page);
    printf("method: %s\nversion: %s\npath: %s\nhost: %s\npage: %s\n", myreq->method, myreq->version, myreq->path, myreq->host, myreq->page);
}
/*
 Sends the HTTP request to the remote site and retuns the HTTP payload to the connected client.
 */
void sendRemoteReq(char filename[MAX_URL], char host[MAX_URL], int socket, char path[MAX_URL], char buffer[MAX_REQUEST])
{
    time_t result;
    result = time(NULL);
    struct tm* brokentime = localtime(&result);
    int chunkRead;
    int chunkWritten;
	size_t fd;
    char data[512];
	char reqBuffer[512];
    bzero(reqBuffer, 512);
    fd = connectTo(host, WEBPORT);
    strcat(reqBuffer, "GET ");
    strcat(reqBuffer, filename);
    strcat(reqBuffer, " HTTP/1.1\n");
    strcat(reqBuffer, "host: ");
    strcat(reqBuffer, host);
    strcat(reqBuffer, "\n\n");		//the above code constructs the request to be sent on to the web browser
    fprintf(fp_log, "request sent to %s :\n%s\nSent at: %s\n", host, reqBuffer, asctime(brokentime));
    printf("request sent to %s :\n%s\nSent at: %s\n", host, reqBuffer, asctime(brokentime));	//report back
    chunkRead = write(fd, reqBuffer, strlen(reqBuffer));	//sends the request to the browser
    int totalBytesRead = 0;
    int totalBytesWritten = 0;
    chunkRead = read(fd, data, sizeof(data));	//reads the information it's given back
    chunkWritten = write(socket, data, chunkRead);	//sends that on to the client
    fprintf (fp_log, "completed sending %s at %d bytes at %s\n------------------------------------------------------------------------------------\n", filename, totalBytesRead, asctime(brokentime));
    printf("completed sending %s at %d bytes at %s\n------------------------------------------------------------------------------------\n", filename, totalBytesRead, asctime(brokentime));
    char *prog[3];
    prog[0] = internet;
    prog[1] = path;
    prog[2] = '\0';
    //execvp(prog[0], prog);	//open web browser
    struct cacheObject* obj;	//construct cache object
    obj = (struct cacheObject*) malloc(sizeof(struct cacheObject*));
    strcpy(obj->request, buffer);
    strncpy(obj->message, data, MAX_MSG-1);
    strncpy(obj->site, path, MAX_MSG-1);
    addCache(obj);	//add object to cache
    fclose(fp_log);	//close log
    close(fd);		//close broswer socket
    close(socket); 	//close client socket
}

void printCache(){	//prints cache
	struct cacheObject* current;
	current = start;
	int i = 0;
	while(i<5){
		printf("Number %d\nRequest: %s\nMessage: %s\nSite: %s\n\n", i, current->request, current->message, current->site);
		i++;
		current = current->next;
	}

}

void addCache(struct cacheObject* obj){	//add object to head of cache in FIFO method
	free(start->next->next->next->next);
	start->next->next->next->next = NULL;
	obj->next = start;
	start = obj;
}

struct cacheObject* inCache(char request[MAX_MSG]){	//checks if request is already in cache, returns respective cache object if true
	int i = 0;
	struct cacheObject* obj;
	obj = start;
	while(i<5){
		if(strcmp(obj->request, request)==0) {return obj;}
		else {obj = obj->next;}
		i++;
	}
	return obj;
}

void makeCache(){	//initiates cache
        start = (struct cacheObject*) malloc(sizeof(struct cacheObject));
	start->next = (struct cacheObject*) malloc(sizeof(struct cacheObject));
	start->next->next = (struct cacheObject*) malloc(sizeof(struct cacheObject));
	start->next->next->next = (struct cacheObject*) malloc(sizeof(struct cacheObject));
	start->next->next->next->next = (struct cacheObject*) malloc(sizeof(struct cacheObject));
	start->next->next->next->next->next = NULL;
}

void checkForBlock(){	//asks user for URLS to lbock, parses them and enters them into a block URL file
	printf("Enter URLs to block. iPress space to exit\n");
        char buff[BUFFLEN];
        char *input = calloc(BUFFLEN, sizeof(char));
        while(*(fgets(buff, sizeof(buff), stdin)) != '\n')
        {
            input = realloc(input, strlen(input)+1+strlen(buff));
            strcat(input,buff); 
        }
	blocklist = fopen("server.blocklist", "a");
        input[strcspn(input, "\n")] = 0;
        char delim[] = " ";
        char *data[sizeof(input)+1];
        char *ptr = strtok(input, delim);
        int i = 0;
	while(ptr != NULL)
	{
            data[i] = ptr;
            if(data[i]!=NULL)
            {
		fprintf(blocklist, "%s\n", data[i]);
		ptr = strtok(NULL, delim);
            }
        i++;
	}      
        fclose(blocklist);
}
