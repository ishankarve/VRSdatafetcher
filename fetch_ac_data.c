#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <string.h> 
#include <my_global.h>
#include <mysql.h>
#include <syslog.h>
#include <jansson.h>

//define server parameters
#define WEBHOST		"sdm.virtualradarserver.co.uk"
#define PAGE		"/track_listen.php"
#define WEBIP2		"5.39.89.202"
#define	WEBPORT		80
#define	USERAGENT	"DelhiSpotter"

struct sample_response
{
  char Icao[50];
  char Country[60];
  char Registration[20];
  char Manufacturer[100];
  char Model[100];
  char ModelIcao[50];
  char Operator[100];
  char OperatorIcao[10];
  char Serial[50];
  char YearBuilt[50];
  
}
  sample_response;


char* process_request(char *http_response)
{
	char *httpbody; int i,j;
	httpbody = strstr(http_response, "\r\n\r\n");
	if(httpbody) 
    httpbody += 4; /* move ahead 4 chars*/
	
	
    return httpbody;
	
	
}
void finish_with_error(MYSQL *con)
{
  fprintf(stderr, "%s\n", mysql_error(con));
  mysql_close(con);
  exit(1);        
}


int main(int argc, char *argv[])
{
		//declare variables
		struct sample_response response;
		struct sockaddr_in server_addr;
		struct hostent *server_host;
		int x,y,z;
		int true=1;int i=0,j=0;
		int server_socket_id;
        char post_data[1024],recv_data[1024];   
        char post_string[15];
		char msql[222];
		server_host=gethostbyname(WEBIP2);
		//bzero(&(server_addr.sin_zero),8); 
        server_addr.sin_family = AF_INET;         
        server_addr.sin_port = htons(WEBPORT);     
        server_addr.sin_addr = *((struct in_addr *)server_host->h_addr);
        ssize_t n;
		
		json_t *root;
		json_error_t error;
		
		
		MYSQL *mysqlconn;
		MYSQL_RES *mysqlresult;
		MYSQL_ROW mysqlrow;
		int num_fields, num_rows;
		mysqlconn = mysql_init(NULL);
		
		
		//connect to databases
		if (mysqlconn == NULL) 
		{
			printf("Error %u: %s\n", mysql_errno(mysqlconn), mysql_error(mysqlconn));
			syslog(LOG_ERR, "Error %u: %s\n", mysql_errno(mysqlconn), mysql_error(mysqlconn));
			syslog(LOG_ERR, "%s", "Terminating program execution\n");
			exit(1);
		}
		if (mysql_real_connect(mysqlconn, "127.0.0.1", "root","123456", "airplot_sync_db", 0, NULL, 0) == NULL) 
		{
				printf("Error %u: %s\n", mysql_errno(mysqlconn), mysql_error(mysqlconn));
				syslog(LOG_ERR, "Error %u: %s\n", mysql_errno(mysqlconn), mysql_error(mysqlconn));
				syslog(LOG_ERR, "%s", "Terminating program execution\n");
				exit(1);
		}
		else
		{
				printf("Connected to MySQL database successfully\n");
				syslog(LOG_NOTICE,"%s","Connected to MySQL database successfully.\n");

		}
		snprintf(msql,400, "select hex_code from basestation WHERE Icao like '';");
		//printf ("%s\r\n", msql);
		mysql_query(mysqlconn,msql);
		mysqlresult=mysql_store_result(mysqlconn);
		if (mysqlresult==NULL)
		{
			finish_with_error(mysqlconn);
		}
		num_fields=mysql_num_fields(mysqlresult);
		num_rows=mysql_num_rows(mysqlresult);
		if (num_rows>0)
		{	
			while((mysqlrow=mysql_fetch_row(mysqlresult))&& num_fields>0)
			{
				for (x=0;x<num_fields;x++)
				{
					printf("Checking Data for Code %s\r\n",mysqlrow[x]);
					snprintf(post_string,15,"icaos=%s ", mysqlrow[x]); 			
					//conect to webserver	
					//create socket to connect to webserver
					if ((server_socket_id = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
						{
							perror("Error Creating Webserver Socket");
							exit(1);
						}
					if (connect(server_socket_id, (struct sockaddr *)&server_addr,sizeof(struct sockaddr)) == -1) 
						{
							perror("Error Connecting to Web Server");
							exit(1);
						}	
					//bzero(&post_data,1024);
					sprintf(post_data,"POST /Aircraft/GetAircraftByIcaos HTTP/1.1\r\n"
									  "Host: sdm.virtualradarserver.co.uk\r\n"
									  "Expect: 100-continue\r\n"
									  "Accept-Encoding: gzip, deflate\r\n"
									  "Content-Type: application/x-www-form-urlencoded\r\n"
									  "Content-Length: %d\r\n\r\n"
									  "%s",12,post_string);
								  
								  
					write(server_socket_id,post_data,strlen(post_data)); 
					
					n = read(server_socket_id, recv_data, 1024);
					recv_data[n] = '\0';
					close(server_socket_id);
					//printf("%s",recv_data);
					root = json_loads(process_request(recv_data), 0, &error);

					if(!root)
					{
						fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
						syslog(LOG_ERR, "error: on line %d: %s\n", error.line, error.text);
						return 1;
					}

					if(!json_is_array(root))
					{
						fprintf(stderr, "error: root is not an array\n");
						syslog(LOG_ERR, "error: root is not an array\n");
						json_decref(root);
						return 1;
					}
					
					for(x = 0; x < json_array_size(root); x++)
					{
						json_t *data, *icao, *country, *registration, *manufacturer, *model,*modelicao,*operator,*operatoricao,*serial,*yearbuilt;
						//json_t *seen,*validposition;
						data = json_array_get(root, x);
						icao = json_object_get(data, "Icao");
						country = json_object_get(data, "Country");
						registration = json_object_get(data, "Registration");
						manufacturer = json_object_get(data, "Manufacturer");
						model = json_object_get(data, "Model");
						modelicao = json_object_get(data, "ModelIcao");
						operator = json_object_get(data, "Operator");
						operatoricao = json_object_get(data, "OperatorIcao");
						serial = json_object_get(data, "Serial");
						yearbuilt = json_object_get(data, "YearBuilt");
						//formulate sql query
						//snprintf(msql,300, "INSERT INTO basestation (hex_code,Icao, Country, Registration, Manufacturer, Model, ModelIcao, Operator, OperatorIcao, Serial, YearBuilt)VALUES (\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\");", json_string_value(icao),json_string_value(icao),json_string_value(country),json_string_value(registration),json_string_value(manufacturer),json_string_value(model),json_string_value(modelicao),json_string_value(operator),json_string_value(operatoricao),json_string_value(serial),json_string_value(yearbuilt));
						snprintf(msql,300, "UPDATE basestation SET hex_code=\"%s\",Icao=\"%s\",Country=\"%s\",Registration=\"%s\",Manufacturer=\"%s\",Model=\"%s\",ModelIcao=\"%s\",Operator=\"%s\",OperatorIcao=\"%s\",Serial=\"%s\",YearBuilt=\"%s\" WHERE hex_code like \"%s\";", json_string_value(icao),json_string_value(icao),json_string_value(country),json_string_value(registration),json_string_value(manufacturer),json_string_value(model),json_string_value(modelicao),json_string_value(operator),json_string_value(operatoricao),json_string_value(serial),json_string_value(yearbuilt),json_string_value(icao));
						//printf ("%s\r\n",msql);
						mysql_query(mysqlconn,msql);
					
					}
			}
		}
		mysql_free_result(mysqlresult);
		}	
		
		mysql_close(mysqlconn);	
		json_decref(root);
			
	
		exit(0);
}


