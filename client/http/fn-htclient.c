/*
 * FroNTier client API
 * 
 * Author: Sergey Kosyakov
 *
 * $Header$
 *
 * $Id$
 *
 */
 
#include <fn-htclient.h>

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


#define MAX_NAME_LEN	128
#define URL_FMT_SRV	"http://%127[^/]/%127s"
#define URL_FMT_PROXY	"http://%s"


static void set_err_msg(FrontierHttpClnt *c,int err,const char *msg)
 {
  c->err_code=err;
  if(c->err_msg) free(c->err_msg);
  c->err_msg=strdup(msg);
 }
 
 
static void set_err_errno(FrontierHttpClnt *c,int err)
 {
  char *msg;
  
  msg=strerror(err);
  set_err_msg(c,err,msg);
 }
 

FrontierHttpClnt *frontierHttpClnt_create()
 {
  FrontierHttpClnt *c;
  
  c=malloc(sizeof(FrontierHttpClnt));
  if(!c) return c;
  bzero(c,sizeof(FrontierHttpClnt));
  c->socket=-1;
  
  return c;
 }
 
 
int frontierHttpClnt_addServer(FrontierHttpClnt *c,const char *url)
 {
  FrontierUrlInfo *fui;
  
  if(c->total_server+1>=FRONTIER_SRV_MAX_NUM) 
   {
    set_err_msg(c,EINVAL,"Server list is full");
    return -1;
   }
  
  fui=frontier_CreateUrlInfo(url);
  if(!fui) 
   {
    set_err_errno(c,ENOMEM);
    return -1;
   }
  
  if(fui->err_msg) 
   {
    set_err_msg(c,EINVAL,fui->err_msg);
    frontier_DeleteUrlInfo(fui);
    return -1;
   }
  
  if(!fui->path)
   {
    set_err_msg(c,EINVAL,"Server path name is missing");
    frontier_DeleteUrlInfo(fui);
    return -1;   
   }
   
  c->server[c->total_server]=fui;
  c->total_server++;
  
  return 0;
 }

 
int frontierHttpClnt_addProxy(FrontierHttpClnt *c,const char *url)
 {
  FrontierUrlInfo *fui;
  
  if(c->total_proxy+1>=FRONTIER_SRV_MAX_NUM)
   {
    set_err_msg(c,EINVAL,"Proxy list is full");
    return -1;
   }
    
  fui=frontier_CreateUrlInfo(url);
  if(!fui) 
   {
    set_err_errno(c,ENOMEM);
    return -1;
   }
  
  if(fui->err_msg) 
   {
    set_err_msg(c,EINVAL,fui->err_msg);
    frontier_DeleteUrlInfo(fui);
    return -1;
   }
   
  c->proxy[c->total_proxy]=fui;
  c->total_proxy++;
  
  return 0;
 }
 
 
 
void frontierHttpClnt_setCacheRefreshFlag(FrontierHttpClnt *c,int is_refresh)
 {
  c->is_refresh=is_refresh;
 }
 
 
 
static int http_read(FrontierHttpClnt *c)
 {
  //printf("\nhttp_read\n");
  //bzero(c->buf,FRONTIER_HTTP_BUF_SIZE);
  c->data_pos=0;
  
  c->data_size=frontier_read(c->socket,c->buf,FRONTIER_HTTP_BUF_SIZE);
  if(c->data_size<0)
   {
    set_err_errno(c,errno);
   }
  
  return c->data_size;
 }

 
static int read_char(FrontierHttpClnt *c,char *ch)
 {
  int ret;
  
  if(c->data_pos+1>c->data_size)
   {
    ret=http_read(c);
    if(ret<=0) return ret;
   }
  *ch=c->buf[c->data_pos];
  c->data_pos++;
  return 1;
 }

 
static int test_char(FrontierHttpClnt *c,char *ch)
 {
  int ret;
  
  if(c->data_pos+1>c->data_size)
   {
    ret=http_read(c);
    if(ret<=0) return ret;
   }
  *ch=c->buf[c->data_pos];
  return 1;
 }
 
 
static int read_line(FrontierHttpClnt *c,char *buf,int buf_len)
 {
  int i;
  int ret;
  char ch,next_ch;
  
  i=0;
  while(1)
   {
    ret=read_char(c,&ch);
    if(ret<0) return ret;
    if(!ret) break;
    
    if(ch=='\r' || ch=='\n')
     {
      ret=test_char(c,&next_ch);
      if(ret<0) return ret;
      if(!ret) break;      
      if(ch=='\r' && next_ch=='\n') read_char(c,&ch);	// Just ignore
      break;
     }
    buf[i]=ch;
    i++;
    if(i>=buf_len-1) break;
   }
  buf[i]=0;
  return i;
 }
   
  
#define FN_REQ_BUF 16384
 
int frontierHttpClnt_open(FrontierHttpClnt *c,const char *url)
 {
  int ret;
  int len;
  struct sockaddr_in *sin;
  struct addrinfo *addr;
  char buf[FN_REQ_BUF];
  FrontierUrlInfo *fui_proxy;
  FrontierUrlInfo *fui_server;
  int is_proxy;
  
  fui_proxy=c->proxy[c->cur_proxy];
  fui_server=c->server[c->cur_server];
  
  if(!fui_server)
   {
    set_err_msg(c,EINVAL,"No available server");
    return -1;
   }
  
  if(fui_proxy)
   {
    printf("Using proxy\n");
    if(!fui_proxy->addr)
     {
      if(frontier_resolv_host(fui_proxy))
       {
        set_err_msg(c,EINVAL,fui_proxy->err_msg);
	return -1;
       }
     }
    addr=fui_proxy->addr;
    is_proxy=1;
   }
  else
   {
    if(!fui_server->addr)
     {
      if(frontier_resolv_host(fui_server))
       {
        set_err_msg(c,EINVAL,fui_server->err_msg);
	return -1;
       }
     }
    addr=fui_server->addr;
    is_proxy=0;
   }
     
  do
   {
    c->socket=frontier_socket();
    if(c->socket<0)
     {
      set_err_errno(c,errno);
      return errno;
     }
    
    sin=(struct sockaddr_in*)(addr->ai_addr);
    if(is_proxy) 
     {
      sin->sin_port=htons((unsigned short)(fui_proxy->port));
     }
    else
     {
      sin->sin_port=htons((unsigned short)(fui_server->port));
     }
  
    ret=frontier_connect(c->socket,addr->ai_addr,addr->ai_addrlen);
    if(ret==0) break;
    
    close(c->socket);        
    addr=addr->ai_next;
   }while(addr);
  
  if(ret)
   {
    set_err_errno(c,errno);
    return errno;
   }
   
  bzero(buf,FN_REQ_BUF);
  
  if(is_proxy)
   {
    len=snprintf(buf,FN_REQ_BUF,"GET %s/%s HTTP/1.0\r\nHost: %s\r\n",fui_server->url,url,fui_server->host);
   }
  else
   {
    len=snprintf(buf,FN_REQ_BUF,"GET /%s/%s HTTP/1.0\r\nHost: %s\r\n",fui_server->path,url,fui_server->host);
   }
  if(len>=FN_REQ_BUF)
   {
    set_err_msg(c,EINVAL,"Request is too big");
    return EINVAL;
   }
   
  if(c->is_refresh)
   {
    ret=snprintf(buf+len,FN_REQ_BUF-len,"Pragma: no-cache\r\n\r\n");
   }
  else
   {
    ret=snprintf(buf+len,FN_REQ_BUF-len,"\r\n");
   }
  if(ret>=FN_REQ_BUF-len)
   {
    set_err_msg(c,EINVAL,"Request is too big");
    return EINVAL;
   }
  len+=ret;
   
  printf("Request <%s>\n",buf);
   
  ret=frontier_write(c->socket,buf,len);
  if(ret<0)
   {
    set_err_errno(c,errno);
    return errno;
   }
  if(ret!=len)
   {
    set_err_msg(c,-1,"Write can not send the whole request");
    return -1;
   }

  // Read status line
  ret=read_line(c,buf,FN_REQ_BUF);
  printf("Status line <%s>\n",buf);
  if(ret<0)
   {
    set_err_msg(c,-1,"Read error");
    return -1;   
   }
  if(strcmp(buf,"HTTP/1.0 200 OK") && strcmp(buf,"HTTP/1.1 200 OK"))
   {
    set_err_msg(c,-1,"Bad server response");
    return -1;       
   }
      
  do
   {   
    ret=read_line(c,buf,FN_REQ_BUF);
    printf("Buf <%s>\n",buf);
    if(ret<0)
     {
      set_err_msg(c,-1,"Read error");
      return -1;   
     }
   }while(*buf);
            
  return 0;
 }
 
 
 
int frontierHttpClnt_read(FrontierHttpClnt *c,char *buf,int buf_len)
 {
  int ret;
  int available;
  
  if(c->data_pos+1>c->data_size)
   {
    ret=http_read(c);
    if(ret<0)
     {
      set_err_errno(c,errno);
     }    
    if(ret<=0) return ret;
   }
   
  available=c->data_size-c->data_pos;
  available=buf_len>available?available:buf_len;
  bcopy(c->buf+c->data_pos,buf,available);
  c->data_pos+=available;
    
  return available;
 }
 
 
void frontierHttpClnt_close(FrontierHttpClnt *c)
 {
  close(c->socket);
  c->socket=-1;
 }
 
 
void frontierHttpClnt_delete(FrontierHttpClnt *c)
 {
  int i;
  
  for(i=0;i<FRONTIER_SRV_MAX_NUM && c->server[i]; i++) frontier_DeleteUrlInfo(c->server[i]);
  for(i=0;i<FRONTIER_SRV_MAX_NUM && c->proxy[i]; i++) frontier_DeleteUrlInfo(c->proxy[i]);
  
  if(c->socket>=0) frontierHttpClnt_close(c);
  if(c->err_msg) free(c->err_msg);
  
  free(c);
 }

