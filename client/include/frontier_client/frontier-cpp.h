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
#ifndef __HEADER_H_FRONTIER_FRONTIER_CPP_H_
#define __HEADER_H_FRONTIER_FRONTIER_CPP_H_

#include <string>
#include <vector>

extern "C"
 {
#include <frontier_client/frontier.h>
 };

namespace frontier{

enum encoding_t {BLOB};

class DataSource;

class Request
 {
  private:
   friend class DataSource;
   std::string obj_name;
   std::string v;
   encoding_t enc;
   std::string key1;
   std::string val1;
   std::vector<std::string> *v_key;
   std::vector<std::string> *v_val;

  public:
   explicit 
   Request(const std::string& name,
           const std::string& version,
           const encoding_t& encoding,
           const std::string& key,
           const std::string& value):
          obj_name(name),v(version),enc(encoding),key1(key),val1(value),v_key(NULL),v_val(NULL){};
   void addKey(const std::string& key,const std::string& value);
   virtual ~Request();
 };


int init();

// Enum sucks
typedef unsigned char BLOB_TYPE;
const BLOB_TYPE BLOB_BIT_NULL=(1<<7);

const BLOB_TYPE BLOB_TYPE_NONE=BLOB_BIT_NULL;
const BLOB_TYPE BLOB_TYPE_BYTE=0;
const BLOB_TYPE BLOB_TYPE_INT4=1;
const BLOB_TYPE BLOB_TYPE_INT8=2;
const BLOB_TYPE BLOB_TYPE_FLOAT=3;
const BLOB_TYPE BLOB_TYPE_DOUBLE=4;
const BLOB_TYPE BLOB_TYPE_TIME=5;
const BLOB_TYPE BLOB_TYPE_ARRAY_BYTE=6;
const BLOB_TYPE BLOB_TYPE_EOR=7;

const char *getFieldTypeName(BLOB_TYPE t);  

class DataSource;
 
class AnyData
 {
  private:
   friend class DataSource;
   union
    {
     long long i8;
     double d;
     struct{char *p;unsigned int s;}str;    
     int i4;
     float f;    
     char b;
    } v;
   int isNull;   // I do not use "bool" here because of compatibility problems [SSK]   
   int type_error;   
   BLOB_TYPE t;  // The data type

   int castToInt();
   long long castToLongLong();
   float castToFloat();
   double castToDouble();
   std::string* castToString();

  public:     
   explicit AnyData(): isNull(0),type_error(FRONTIER_OK),t(BLOB_TYPE_NONE){}
   inline void set(int i4){t=BLOB_TYPE_INT4;v.i4=i4;type_error=FRONTIER_OK;}
   inline void set(long long i8){t=BLOB_TYPE_INT8;v.i8=i8;type_error=FRONTIER_OK;}
   inline void set(float f){t=BLOB_TYPE_FLOAT;v.f=f;type_error=FRONTIER_OK;}
   inline void set(double d){t=BLOB_TYPE_DOUBLE;v.d=d;type_error=FRONTIER_OK;}
   inline void set(long long t,int time){t=BLOB_TYPE_TIME;v.i8=t;type_error=FRONTIER_OK;}
   inline void set(unsigned int size,char *ptr){t=BLOB_TYPE_ARRAY_BYTE;v.str.s=size;v.str.p=ptr;type_error=FRONTIER_OK;}
   inline void setEOR(){t=BLOB_TYPE_EOR;type_error=FRONTIER_OK;}
   inline BLOB_TYPE type() const{return t;}
   inline int isEOR() const{return (t==BLOB_TYPE_EOR);}

   inline int getInt(){if(isNull) return 0;if(t==BLOB_TYPE_INT4) return v.i4; return castToInt();}
   inline long long getLongLong(){if(isNull) return 0; if(t==BLOB_TYPE_INT8 || t==BLOB_TYPE_TIME) return v.i8; return castToLongLong();}
   inline float getFloat(){if(isNull) return 0.0; if(t==BLOB_TYPE_FLOAT) return v.f; return castToFloat();}
   inline double getDouble(){if(isNull) return 0.0;if(t==BLOB_TYPE_DOUBLE) return v.d; return castToDouble();}
   inline std::string* getString(){if(isNull) return NULL; if(t==BLOB_TYPE_ARRAY_BYTE) return new std::string(v.str.p,v.str.s); return castToString();}   
   
   inline void clean(){if(t==BLOB_TYPE_ARRAY_BYTE && v.str.p) {delete[] v.str.p; v.str.p=NULL;}} // Thou art warned!!!
   
   ~AnyData(){clean();} // Thou art warned!!!   
 };
 

 
class DataSource
 {
  private:
   unsigned long channel;
   std::string *uri;
   void *internal_data;
   BLOB_TYPE last_field_type;

  public:
   int err_code;
   std::string err_msg;
   
   explicit DataSource(const std::string& server_url="",const std::string* proxy_url=NULL);
   
   // If reload!=0 then all requested objects will be refreshed at all caches
   // New object copy will be obtained directly from server
   void setReload(int reload);
   
   // Get data for Requests
   void getData(const std::vector<const Request*>& v_req);
   
   // Each Request generates a payload. Payload numbers started with 1.
   // So, to get data for the first Request call setCurrentLoad(1)
   void setCurrentLoad(int n);
   
   // Check error for this particular payload.
   // If error happes for any payload that payload and all subsequent payloads (if any) are empty
   int getCurrentLoadError() const;
   // More detailed (hopefully) error explanation.
   const char* getCurrentLoadErrorMessage() const;
   
   // Data fields extractors
   // These methods change DS position to the next field
   // Benchmarking had shown that inlining functions below does not improve performance
   int getAnyData(AnyData* buf);   
   int getInt();
   long getLong();
   long long getLongLong();
   double getDouble();
   float getFloat();
   long long getDate();
   std::string *getString();
   std::string *getBlob();
   
   // Current pyload meta info
   unsigned int getRecNum();
   unsigned int getRSBinarySize();
   unsigned int getRSBinaryPos();
   BLOB_TYPE lastFieldType(){return last_field_type;} // Original type of the last extracted field
   BLOB_TYPE nextFieldType(); // Next field type. THIS METHOD DOES NOT CHANGE DS POSITION !!!
   inline int isEOR(){return (nextFieldType()==BLOB_TYPE_EOR);}  // End Of Record. THIS METHOD DOES NOT CHANGE DS POSITION !!!
   inline int isEOF(){return (getRSBinarySize()==getRSBinaryPos());} // End Of File
   
   virtual ~DataSource();
 };


class CDFDataSource:public virtual DataSource
 {
  public:
   explicit CDFDataSource(const std::string& server_url="",const std::string* proxy_url=NULL):DataSource(server_url,proxy_url){}
   
   std::vector<unsigned char> *getRawAsArrayUChar();
   std::vector<int> *getRawAsArrayInt();
   std::vector<float> *getRawAsArrayFloat();
   std::vector<double> *getRawAsArrayDouble();
   std::vector<long> *getRawAsArrayLong();
 };
 
 

}; // namespace frontier


#endif //__HEADER_H_FRONTIER_FRONTIER_CPP_H_
