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
#include <frontier.h>
#include <endian.h>
#include <sys/types.h>
#include <stdlib.h>
#include "fn-internal.h"

extern void *(*frontier_mem_alloc)(size_t size);
extern void (*frontier_mem_free)(void *p);

union u_Buf32 { int v; char b[4]; };
union u_Buf64 { long long v; double d; char b[8]; };

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline int n2h_i32(const char *p) 
 {
  union u_Buf32 u; 
  u.b[3]=p[0]; 
  u.b[2]=p[1]; 
  u.b[1]=p[2]; 
  u.b[0]=p[3];  
  return u.v;
 }
static inline long long n2h_i64(const char *p) 
 {
  union u_Buf64 u; 
  u.b[7]=p[0]; 
  u.b[6]=p[1]; 
  u.b[5]=p[2]; 
  u.b[4]=p[3];  
  u.b[3]=p[4]; 
  u.b[2]=p[5]; 
  u.b[1]=p[6]; 
  u.b[0]=p[7];  
  return u.v;
 }
static inline double n2h_d64(const char *p) 
 {
  union u_Buf64 u; 
  u.b[7]=p[0]; 
  u.b[6]=p[1]; 
  u.b[5]=p[2]; 
  u.b[4]=p[3];  
  u.b[3]=p[4]; 
  u.b[2]=p[5]; 
  u.b[1]=p[6]; 
  u.b[0]=p[7];  
  return u.d;
 }
#else
#warning Big endian order
#define n2h_i32(p) (*((int*)(p)))
#define n2h_i64(p) (*((long long*)(p)))
#define n2h_d64(p) (*((double*)(p)))
#endif /*__BYTE_ORDER*/

int frontier_n2h_i32(const void* p){return n2h_i32(p);}
float frontier_n2h_f32(const void* p){return (float)n2h_i32(p);}
long long frontier_n2h_i64(const void* p){return n2h_i64(p);}
double frontier_n2h_d64(const void* p){return n2h_d64(p);}

FrontierRSBlob *frontierRSBlob_get(FrontierChannel u_channel,int n,int *ec)
 {
  Channel *chn=(Channel*)u_channel;
  FrontierResponse *resp;
  FrontierPayload *fp;
  FrontierRSBlob *rs=(void*)0;

  resp=chn->resp;
  
  if(n>resp->payload_num)
   {
    *ec=FRONTIER_ENORS;
    return rs;
   }

  fp=resp->payload[n-1];

  rs=frontier_mem_alloc(sizeof(FrontierRSBlob));
  if(!rs)
   {
    *ec=FRONTIER_EMEM;
    return rs;
   }

  rs->buf=fp->blob;
  rs->size=fp->blob_size;
  rs->pos=0;
  rs->nrec=fp->nrec;

  *ec=FRONTIER_OK;
  return rs;
 }


void frontierRSBlob_close(FrontierRSBlob *rs,int *ec)
 {
  *ec=FRONTIER_OK;

  if(!rs) return;
  frontier_mem_free(rs);
 }


void frontierRSBlob_rsctl(FrontierRSBlob *rs,int ctl_fn,void *data,int size,int *ec)
 {
  *ec=FRONTIER_OK;
 }


void frontierRSBlob_start(FrontierRSBlob *rs,int *ec)
 {
  *ec=FRONTIER_OK;
  rs->pos=0;
 }


char frontierRSBlob_getByte(FrontierRSBlob *rs,int *ec)
 {
  char ret;

  if(rs->pos>=rs->size)
   {
    *ec=FRONTIER_ENOROW;
    return (char)-1;
   }
  ret=rs->buf[rs->pos];
  rs->pos++;
  *ec=FRONTIER_OK;
  return ret;
 }


int frontierRSBlob_getInt(FrontierRSBlob *rs,int *ec)
 {
  int ret;

  if(rs->pos>=rs->size-3)
   {
    *ec=FRONTIER_ENOROW;
    return -1;
   }
  ret=n2h_i32(rs->buf+rs->pos);
  rs->pos+=4;
  *ec=FRONTIER_OK;
  return ret;
 }


long long frontierRSBlob_getLong(FrontierRSBlob *rs,int *ec)
 {
  long long ret;

  if(rs->pos>=rs->size-7)
   {
    *ec=FRONTIER_ENOROW;
    return -1;
   }
  ret=n2h_i64(rs->buf+rs->pos);
  rs->pos+=8;
  *ec=FRONTIER_OK;
  return ret;
 }


double frontierRSBlob_getDouble(FrontierRSBlob *rs,int *ec)
 {
  double ret;

  if(rs->pos>=rs->size-7)
   {
    *ec=FRONTIER_ENOROW;
    return -1;
   }
  ret=n2h_d64(rs->buf+rs->pos);
  rs->pos+=8;
  *ec=FRONTIER_OK;
  return ret;
 }



