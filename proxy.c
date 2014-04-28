/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS: (put your names here)
 *     Alex Henry, adhe223@g.uky.edu 
 *     Rob Burrus, robert.burrus@uky.edu 
 * 
 * Description:
	Main loads the DisallowedWords file, begins listening for connections to the server, and 
	then goes into an infinite loop that looks like this:
	
	while (1) {
	Accept(): wait for a connection request 
	echo(): read input from browser, send get request to server, parse for disallowed words, send to browser 
	Close(): close the connection
	
	Echo is where most of the work in our program happens. Echo reads a GET request from the browser,
	parses it, then formats the request we will send to the webserver, sends it, then receives what the
	webserver responds with and outputs it to the browser. It also calls the functions to log and block.
	After echo is finished we are back in the while loop in main where we close the connection with the
	browser and then loop around and begin listening again.
 
 IMPORTANT: In DisallowedWords file, make sure each "word" is on its own line and is followed by a single new line character. So even the last "word" should have a newline after it. Also the file must be in the
 same path and named "DisallowedWords".
 */ 

#include "csapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAXRESPONSE  1000000

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size, int blocked, int found);
void echo(int connfd, char disallowed[][MAXLINE], int numDisallowed, struct sockaddr_in *sockaddr);
void trimQuery (char * str);
int isBlocked(char * response, char disallowed[][MAXLINE], int numDisallowed);
void removeNewlines (char * str);

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
    /* Check arguments */
    if (argc != 2) {
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);
    }
	
	//Disallowed Word Processing**********
	//Load DisallowedWords
	//Find the number of lines so we can create an appropriate sized array
	FILE *file;
	int numLines = 0;
	char filename[16] = "DisallowedWords";
	char c;
	int fileExists = 1;
	
	file = fopen(filename, "r");
	if (file == NULL) {
		printf("DisallowedWords	failed to open!\n");
		fileExists = 0;
	}
	if (fileExists != 0) {
		//c = getc(file);
		//while (c != EOF)
		while((c=fgetc(file)) != EOF)
		{
			//Newline signals another line in file
			if (c == '\n')
			{
				numLines = numLines + 1;
			}

			//c = getc(file);
		}
		fclose(file); //close file.
		//printf("NumLines: %d", numLines);
	}
	//Now allocate an array of char arrays large enough to hold the words
	char disallowed[numLines][MAXLINE];
	if (fileExists) {
		file = fopen(filename, "r");
		int i;
		for (i = 0; i < numLines; i++) {
			fgets(disallowed[i], MAXLINE, file);
			
			//Remove the trailing newline character if it exists
			char * pos;
			if ((pos=strchr(disallowed[i], '\n')) != NULL) {
				*pos = '\0';
			}
		}
		fclose(file);
	}
	//Disallowed Word Finished**********
	
	int listenfd, connfd, port, clientlen; 
	struct sockaddr_in clientaddr; 
	struct hostent *hp; 
	char *haddrp; 
	unsigned short client_port; 
	
	port = atoi(argv[1]); /* the server listens on a port passed on the command line */ 
	
	//Listening for connections from browsers
	listenfd = open_listenfd(port); 
	//while (1) {
	/* Accept(): wait for a connection request */ 
	/* echo(): read input from browser, send get request to server, parse for disallowed words, send to browser 
	/* Close(): close the connection */
	//}	
	while (1) { 	
		printf("\n---------------------------------------------------------------------------------\n");	
		//Accept
		//Blocks waiting for a connection request
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 	//Connection to browers, I/O ready
		
		//Identifying the browser
		hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr),						AF_INET); 
		haddrp = inet_ntoa(clientaddr.sin_addr); 
		client_port = ntohs(clientaddr.sin_port); 
		//printf("server connected to %s (%s), port %u\n", hp->h_name, haddrp, client_port);

		//Write output to the browser
		echo(connfd, disallowed, numLines, (SA *)&clientaddr); 
		
		//Close connection with browser
		Close(connfd); 
	} 
	
    exit(0);
}

/*
	This is where most of the work in the program is done. Echo reads a GET request from the browser,
	parses it, then formats the request we will send to the webserver, sens it, then receives what the
	webserver responds with and outputs it to the browser. It also calls the functions to log and block.
*/
void echo(int connfd, char disallowed[][MAXLINE], int numDisallowed, struct sockaddr_in *sockaddr) 
{ 
	size_t n;

	//Instantiate buffers to use and make sure they are empty
	char buf[MAXLINE];
	strcpy(buf, "");
	char buf2[MAXLINE];
	strcpy(buf2, "");
	char hostname[MAXLINE];
	strcpy(hostname, "");
	char pathname[MAXLINE];
	strcpy(pathname, "");
	char urlpath[MAXLINE];
	strcpy(urlpath, "");
	char request[MAXLINE];
	strcpy(request, "");
	char response[MAXRESPONSE];
	strcpy(response, "");
	rio_t rio_browser;
	rio_t rio_webserv; 
	int count = 0;
	int failed = 0;
	int *port = (int *)malloc(sizeof(int));
	
	//Webserver connection initializers
	int clientfd;
	
	Rio_readinitb(&rio_browser, connfd); 
	
	n = Rio_readlineb(&rio_browser, buf, MAXLINE);
	//printf("proxy received %d bytes\n", n); 			//Print to server the status
	//printf("The proxy received: %s\n", buf);
	
	//Now we have to take buf (the browsers requests) and pass that to the web server. Our proxy is now
	//the client of the outside web server. We need to parse the first request then leave the rest
	
	//Trim
	char * trimmedRequest[MAXLINE];
		
	//Trim
	strcpy(trimmedRequest, buf);
	trimQuery(trimmedRequest);
	
	//This will remove any prepended text. Need to do this because of funky behavior from trimQuery
	if (strstr(trimmedRequest, "http://") != NULL) {
		strcpy(trimmedRequest, strstr(trimmedRequest, "http://"));
	} else {
		//Incorrectly formatted request
	}	
	//end trim
	
	//Parse
	//Now pass the trimmed query to parse_uri to get our hostname, pathname, and port
	int parseRet = parse_uri(trimmedRequest, hostname, pathname, port);
	printf("Parsed with hostname: %s    pathname: %s     port: %d\n", hostname, pathname, *port);
	
	if (parseRet != -1) {
		//sprintf(buf2,  "GET %s HTTP/1.1\r\n", pathname, hostname);
		//sprintf(buf2,  "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", pathname, hostname);
		sprintf(buf2,  "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", pathname, hostname);
		
		strcpy(buf, buf2);
	}
	//end parse
	
	//Now we move the request over to our request string
	strcat(request, buf);
	
	int size = 0;
	int found = 1;
	
	//Open connection with webserver, we are the client now
	clientfd = open_clientfd(hostname, *port);				//Connect to the webserver
	if (clientfd > -1) {
		Rio_readinitb(&rio_webserv, clientfd); 		
		
		printf("Sending request to webserver: %s", request);
		int e = rio_writen(clientfd, request, strlen(request));	
		printf("Waiting on response from server...\n");
		if (e != -1) {
			int b = Rio_readnb(&rio_webserv, response, MAXRESPONSE);			//Read the response from the server
			//add bytes to size
			size = size + b;
			//printf("returned %d bytes\n", b);
			
			//Write the response from the webserver to the browser if not blocked
			int blocked = isBlocked(response, disallowed, numDisallowed);
			
			if (blocked != 1) {
				printf("The server replied: %s\n", response);
				Rio_writen(connfd, response, strlen(response));
			} else {
				//Output a blocked page
				char blockedPage[] = "<html><head><meta charset=\"UTF-8\"><title>Blocked Page</title></head><body><h1>Blocked Page</h1></body></html>\n";
				Rio_writen(connfd, blockedPage, strlen(blockedPage));
			}
			
			//Log
			char logstring[MAXLINE];
			strcpy(logstring, "");
			//Create a uri string to pass to log entry
			strcat(urlpath, hostname);
			strcat(urlpath, pathname);
			format_log_entry(logstring, (SA *)&sockaddr, urlpath, size, blocked, found);
		}
	} else {
		int blocked = -1;
		char logstring[MAXLINE];
		strcpy(logstring, "");
		//Create a uri string to pass to log entry
		strcat(urlpath, hostname);
		strcat(urlpath, pathname);
		printf("Can't connect\n");
		found = 0;
		
		format_log_entry(logstring, (SA *)&sockaddr, urlpath, size, blocked, found);
	}
	
} 

/*
void upper_case(char * str) {
	char *newstr, *p;
    p = newstr = strdup(str);
    while(*p++=toupper(*p));

    strcpy(str, newstr);
}
*/

//Function used to trim down a GET request to just the url. We can then pass that onto the parser.
void trimQuery (char * str) {
	//This will remove any prepended text
	if (strstr(str, "http://") != NULL) {
		str = strstr(str, "http://");
	} else {
		//Incorrectly formatted request
		return;
	}
	
	//Now find the second occurence of http if it exists
	char * noHeader = str + 7;		//Temporary string without the initial http
	if (strstr(noHeader, "HTTP/") != NULL) {
		//There is an appended html, create a new string without it
		char * strEnd = strstr(noHeader, "HTTP/");	//Gives us the string "HTTP/1.1"
		
		int finalLength = strlen(noHeader) - strlen(strEnd) + 7;	//Leave room for http://
		memcpy(str, str, finalLength);
		str[finalLength] = '\0';		
	} else {}
}

//Loop through the disallowed words array of char arrays and search the response for the disallowed words.
//If found, return 1, else return 0 and don't block.
int isBlocked(char * response, char disallowed[][MAXLINE], int numDisallowed) {
	//Loop over disallowed and check if any of the words are present
	int i;
	int blocked = 0;
	
	char toSearch[MAXRESPONSE];
	strcpy(toSearch, response);
	
	removeNewlines(toSearch);
	for(i = 0; i < numDisallowed; i++) {
		//printf("Disallowed Word: %s\n", disallowed[i]);
		removeNewlines(disallowed[i]);
		char substring[MAXRESPONSE];
		strcpy(substring, "");
		char * compare = strstr(response, disallowed[i]);
		memcpy(substring, &compare, strlen(disallowed[i]) - 1);
		if (strstr(response, disallowed[i]) != NULL) {
			//printf("Found %s in %s\n", substring, recvline);
			blocked = 1;
		}
	}
	
	return blocked;
}

//Removes the carriage returns and newlines from a string. Use this with disallowed words so that we can
//search without accidentally searching for \n and \r.
void removeNewlines (char * str) {
	//Remove carriage returns and newlines
	int len = strlen(str)+1;
	int i;
	for(i = 0; i < len; i++)
	{
		if(str[i] == '\r' || str[i] == '\n')
		{
			// Shift one left
			strncpy(&str[i],&str[i+1],len-i);
		}
	}
}

/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;
	int secure = 0;

    if (strncasecmp(uri, "HTTP://", 7) != 0) {
		if(strncasecmp(uri, "HTTPS://", 8) != 0){
			hostname[0] = '\0';
			return -1;
		}
		secure = 1;
    }
       
	//printf("Before host\n");
    /* Extract the host name */
    if(secure == 1){
		hostbegin = uri + 8;
    }else{
		hostbegin = uri + 7;
    }
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
	//printf("Before port\n");
    /* Extract the port number */
    *port = 80; /* default */	
    //if (*hostend == ':') { 
	//	*port = atoi(hostend + 1);
	//}
    
	//printf("Before path\n");
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
		pathname[0] = '/';
		pathname[1] = '\0';
    }
    else {
		strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), the size in bytes
 * of the response from the server (size), whether the page is blocked, and a buffer to use to build
   the log string.
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size, int blocked, int found)
{
    time_t now;
    char time_str[MAXLINE];
    strcpy(time_str, "");
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
	 
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;
	
	//Check if not found
	if (found == 0) {
		sprintf(logstring, "%s: %d.%d.%d.%d %s (NOTFOUND)", time_str, a, b, c, d, uri);
	}
	
    /* Return the formatted log entry string */
	if (blocked == 0 && found == 1) {
		sprintf(logstring, "%s: %d.%d.%d.%d %s %d", time_str, a, b, c, d, uri, size);
	} else if (blocked == 1 && found == 1) {
		sprintf(logstring, "%s: %d.%d.%d.%d %s %d (BLOCKED: Page has disallowed words)", time_str, a, b, c, d, uri, size);
	}
	
	//Print to the file
	FILE * file;
	file = fopen("proxy.log", "a+");
	fprintf(file, "%s\r\n", logstring);
	fclose(file);
}


