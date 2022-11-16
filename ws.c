vscode git test
#include "ws.h"
#include "sock.h"
#include "rio.h"
#include "syn.h"
#include "urlcode.h"
int valuepos;
char testtmp[10]="         ",value[MAXLINE][6];  //����һ��ȫ�ֱ����Խ�������ͷ����
void *thread(void *vargp );    /* ����*/
void doit(int connfd );        /* ���� */
void read_requesthdrs(rio_t *rp);     /* ����ͷ */
void get_filetype(char *filename, char *filetype);    /* ��ȡMIME���� */
int parse_url(char *url, char *filename, char *cgiargs);     /* parse URL */
void serve_static(int fd, char *filename, int filesize);     /* ��̬����*/
void serve_dynamic(int fd, char *filename, char *cgiargs);   /* ��̬����*/
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);    /* error response */
void serve_dynamicpost(int fd, char *filename, char *cgiargs);

sbuf_t sbuf;     /* ���ӻ����� */

int main(int argc, char *argv[] )
{
    int listenfd, connfd, port;
    unsigned int clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid;
    int portused=1;
    int ret, i;    /* return-value, index */

    if(argc != 2){    /* ���û�����������Ļ�*/
        fprintf(stderr, "�÷�: %s <port>\n", argv[0]);    //δ���ö˿�
        exit(1);
    }
    port = atoi(argv[1]);    /* ��ʽ�� atoi�ǽ�char������ */
    
    sbuf_init(&sbuf, SBUFSIZE);    /* ͷ�������� */
    listenfd = open_listenfd(port);   /* �򿪽��ն˿� */

    for(i = 0; i < NTHREADS; i++){   /* ���߳�*/
        ret = pthread_create(&tid, NULL, thread, NULL);
        if(ret != 0){
            fprintf(stderr,  "create worker thread %d failed. \n", i);
        }
    }
    while(1){

        if(connfd==-1)  portused++;
        connfd = accept(listenfd, (SA*)&clientaddr, &clientlen);   /*  */
        if(connfd!=-1){
            printf("client (%s:%d) has established connection, and connfd is %d\n",
                   inet_ntoa(clientaddr.sin_addr),    /* �û�ip��ַ*/
                   ntohs(clientaddr.sin_port), connfd);    /* �˿�*/
            sbuf_insert(&sbuf, connfd);
        }else{
            printf("port %d is in used\n",port);
            sleep(1);
        }
        if(portused==4) exit(0);
    }

    return 0;
}

void *thread(void *vargp)   /* thread routine */
{
    pthread_detach(pthread_self());   /* �Զ��ͷŽ��� */
    while(1){
        int connfd = sbuf_remove(&sbuf);     
        doit(connfd);                       /* serve client */
        close(connfd);                      /* close connfd */
        printf("connfd %d has closed.\n", connfd);
    }
}

void doit(int fd)     /* */
{
	/*
	������һЩ�ܿӵ��ĵط�Ҫע��
	1.strcasecmp�����Ƚ������ַ� �����һ������true һ������false
	*/
    int is_static;   /* �ж��Ƿ�Ϊ��̬��ҳ */
    struct stat sbuf;   /* �ṹ�� */
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE],bufs[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    char c;
    char cgipost[MAXLINE];
    int content_length=-1;
    rio_t rio;    /* ��ȡio*/
    int numchars = 1;
    rio_readinitb(&rio, fd);    /*  */
//    printf("%s\n",buf);
    rio_readlineb(&rio, buf, MAXLINE );  
    sscanf(buf, "%s %s %s", method,url,version);    
   //  printf("%s", buf); ���Է�����������Ϣ��


    if( strcasecmp(method, "GET")&&strcasecmp(method, "POST") ){    /* method is not GET or POST */
        clienterror(fd, method, "501", "Not implemented", "CTL server does not implement this method except GET and POST");

      return ;
    }
    read_requesthdrs(&rio);   /* ��������ͷ*/
    /*deal url*/
    is_static = parse_url( url, filename, cgiargs );
    /* return*/
    if( stat(filename, &sbuf) < 0 ){  
        clienterror( fd, filename, "404", "Not Found",
                   "CTL server could't find this file"); //404���ļ�
        return ;
    }
    if(is_static){   /* �Ƿ�Ϊ��̬��ҳ*/
        if( !(S_ISREG( sbuf.st_mode )) || !(S_IRUSR & sbuf.st_mode) ){  /* don't have permission? */
        clienterror( fd, filename, "403", "Forbidden",
                    "CTL server couldn't read this file");
             return ;
        }
        serve_static( fd, filename, sbuf.st_size);   /* ��̬web*/
    }else{ /* ��̬��ҳ*/
        if( !(S_ISREG( sbuf.st_mode )) || !(S_IXUSR & sbuf.st_mode) ){  /* don't have permission? */
            clienterror( fd, filename, "403", "Forbidden",
                       "CTL server couldn't read this file");
            return ;
        }
        if( strcasecmp(method, "GET") ){
           // printf("%s",value[3]); //post���� ftu
            content_length=atoi(value[valuepos]); //post���ȸ�ʽ��
//            printf("\n\n%s \n%s\n %s\n%s\n%s\n",value[0],value[1],value[2],value[3],value[4]);
            for (int i = 0; i < content_length; i++) {
                // �����ȡ
                rio_read(&rio, &c, 1);
//                printf("%c",c);//ftu �鿴��ȡ�Ƿ�����
                cgipost[i]=c;
               // printf("%s time=%d",cgipost,i);  //ftu
             }
	            printf("post-data=%s\n",cgipost);
            serve_dynamicpost( fd, filename, cgipost);
          //  clienterror( fd, filename, "501", "Sorry","CTL Server connot procces post request now"); //ftu ������Ӧ�ϵ�鿴
           return ;
         }
        if( strcasecmp(method, "POST") ){    
             serve_dynamic( fd, filename, cgiargs);  /* ��̬��ҳ*/
        }
    }
}

void clienterror( int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

  /* �������󷵻���ҳ*/
    sprintf( body , "<html><title>CTL Server</title>");
    sprintf( body, "%s<body bgcolor=""#bfa"">\r\n", body);
    sprintf( body, "%s<h1>%s:%s</h1> \r\n", body, errnum, shortmsg);
    sprintf( body, "%s%s:%s:\r\n", body, longmsg, cause );
    sprintf( body, "%s<hr color=\"black\"><span style=\"font-size=20px;\">CTL server</span>\r\n", body);

   /* �������ͷ*/
    sprintf( buf, "HTTP/1.1 %s %s \r\n", errnum, shortmsg);
    rio_writen( fd, buf, strlen(buf) );
    sprintf( buf, "Content-Type: text/html\r\n");
    rio_writen(fd,buf, strlen(buf) );
    sprintf( buf, "Content-length: %ld\r\n\r\n", strlen(body) );
    rio_writen( fd, buf, strlen(buf) );

    /* ���������*/
    rio_writen( fd, body, strlen(body) );
}


void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE], key[MAXLINE];
    char *p;
    int i=0;
    rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")){   /*ֱ����ֵ*/
        p = strchr(buf, ':');/*��������ͷ*/
        if(p){
            *p = '\0';
            sscanf(buf, "%s", key);
            sscanf(p+1, "%s", value[i]);
            printf("%d    %s : %s\n",i, key, value[i]);
            if(!strcasecmp(key, "Content-Length")) valuepos=i;/*��¼post����*/
            i++;
        }
        rio_readlineb(rp, buf, MAXLINE );    /* ��ȡ��һ��*/
    }
    return  ;
}

int parse_url( char *url, char *filename, char *cgiargs)
{
    char *ptr;
    if( !strstr(url, "cgi-bin") ){    /* �Ƿ�Ϊ��̬ */
        strcpy( cgiargs, "");   /* û�в��� */
        strcpy( filename, "./html");  /* Ĭ��·��*/
        strcat( filename, url);  /* */
        if( url[ strlen(url) -1 ] == '/')  /* home��ҳ*/
            strcat( filename,"home.html");
        return 1;   /* �����Ǿ�̬��ҳ */
    }else{    /* ��̬��ҳ */
       ptr = strchr( url, '?');  /* �ҵ��ʺŵ�λ�� */
        if( ptr ){   /* has arguments ? */
             strcpy( cgiargs, ptr+1 ) ;
            *ptr = '\0';
    }else{  /* û�в���	*/
        strcpy(cgiargs, "");
    }
    strcpy( filename, ".");
    strcat( filename, url );
    return 0;   /* û�о�̬��ҳ*/
    }
}
void serve_static( int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* ��������ͷ*/
    get_filetype( filename, filetype );
    sprintf( buf, "HTTP/1.1 200 OK \r\n" );
    sprintf( buf, "%sServer: A simple web server\r\n", buf );
    sprintf( buf, "%sContent-length: %d\r\n", buf, filesize );
    sprintf( buf, "%sContent-Type: %s\r\n\r\n", buf, filetype );
    rio_writen( fd, buf, strlen(buf) );

    /* ������Ӧ�� */
    srcfd = open( filename, O_RDONLY, 0);
    /* ����ram */
    srcp = mmap( 0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0 );  close( srcfd );    /* close fd */
    rio_writen( fd, srcp, filesize );
    munmap( srcp, filesize);
}

/* MIME����ָʾ*/    
void get_filetype(char *filename, char *filetype)
{
    if( strstr(filename, ".html") )     /* html */
        strcpy( filetype, "text/html" );
    else if( strstr(filename, ".htm") )     /* htm */
        strcpy( filetype, "text/htm" );
    else if(strstr(filename, ".xml"))
        strcpy(filetype, "application/xml");
    else if(strstr(filename, ".pgn"))
        strcpy(filetype, "image/pgn");
    else if( strstr( filename, ".jpg") )   /* jpg */
        strcpy( filetype, "image/jpeg");
    else if( strstr(filename, ".gif") )    /* gif */
        strcpy( filetype, "image/gif");
    else if(strstr(filename, ".ico"))
        strcpy(filetype, "image/x-icon");
    else if(strstr(filename, ".pdf"))
        strcpy(filetype, "application/pdf");
    else  /* others */
        strcpy( filetype, "text/plain");
}
void serve_dynamic( int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist [] = {NULL};
    extern char **environ;
        sprintf( buf, "HTTP/1.1 200 OK\r\n");    /*״̬��*/
    rio_writen( fd, buf, strlen(buf) );
    sprintf( buf, "Server: CTL server \r\n");  
    rio_writen( fd, buf, strlen(buf));
    sprintf( buf, "Content-Type: text/html\r\n");
    rio_writen(fd,buf, strlen(buf) );
    if( fork() == 0 ){   /* �ӽ��� */
	   setenv( "QUERY_STRING", cgiargs, 1);/*���û�������*/
 	   dup2( fd, STDOUT_FILENO);   /* redirect stdout to client */
   	   execve( filename, emptylist, environ);   /*  */
    }
    wait(NULL);    /*�ȴ��ӽ��̽��� */
}

void serve_dynamicpost( int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist [] = {NULL};
    extern char **environ;
        sprintf( buf, "HTTP/1.1 200 OK\r\n");    /*״̬��*/
    rio_writen( fd, buf, strlen(buf) );
    sprintf( buf, "Server: CTL server \r\n");
    rio_writen( fd, buf, strlen(buf));
    sprintf( buf, "Content-Type: text/html\r\n");
    rio_writen(fd,buf, strlen(buf) );
    if( fork() == 0 ){   /* �ӽ��� */
           setenv( "QUERY_STRING", cgiargs, 1);/*���û�������*/
           setenv("Content_Length",value[valuepos],1);
           dup2( fd, STDOUT_FILENO);   /* redirect stdout to client */
           execve( filename, emptylist, environ);   /*����cgi����*/
    }
    wait(NULL);    /*�ȴ��ӽ��̽��� */
}
