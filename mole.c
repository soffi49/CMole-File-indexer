//libraries used
#define _GNU_SOURCE 
#define _XOPEN_SOURCE 500  
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>         
#include <stdio.h>                  
#include <errno.h>          
#include <unistd.h>         
#include <ftw.h>             
#include <string.h>         
#include <stddef.h>           
#include <stdbool.h>        
#include <time.h>           
#include <pthread.h>        
#include <fcntl.h>          
#include <sys/types.h>      
#include <sys/stat.h> 

//header file with definitions
#include "config.h"

//usage- used for handling incorrect arguments
void usage(char *name)
{
        fprintf(stderr,"%s\n",name);
        exit(EXIT_FAILURE);
}

//strctures
typedef enum {jpeg=1,png,zip,gzip,dir} f_type;                              //enum file type         
typedef enum {largerthan,namepart,owner} command_t;                         //enum command type
const char* enum_as_string[]={"",".jpeg",".png",".zip",".gzip",".dir"};     //file type as string

typedef struct index_data                   //main structrue of index
{
    char file_name[MAX_NAME];               //file name
    char path[MAX_PATH];                    //path to the file
    off_t size;                             //size of the file
    uid_t owner_id;                         //owner id number
    f_type type;                            //type of the file

}index_data;

typedef struct index_structure              //structure containing indexes
{
    index_data* indexStruct;
    int indexStruct_size;

}index_structure;

typedef struct thread_struct                //structare containg all thread elements used in thread work
{
    pthread_t pth_id;                       //thread id number
    const char* directory;                  //the directory path for indexing
    const char* filepath;                   //full path to the file to save index informations
    bool is_open;                           //flag to check if thread is writing to the file
    bool is_indexing;                       //flag to check if thread is performing indexing
    int reidx_time;                         //saves info if the auto re-indexing should be performed
    time_t clock;                           //saves the time after last re-indexing
    pthread_mutex_t *mutex_open;            //mutex which is locked when there are operation on file (writing to file)
    pthread_mutex_t *mutex_indexing;        //mutex which is locked when the indexing is being performed
    pthread_mutex_t *mutex_clock;           //mutex which is locked when the time of clock is changing
    index_structure main;

}thread_struct;
//functions 

void read_arg(int argc,char** argv,char** path_d,char** path_f,int* n);                         //function to read arguments given by user
int indexing(const char* name,const struct stat *s,int type,struct FTW *f);                     //function which will perform indexing 
void* thread_work(void* arguments);                                                             //function performed by the indexing thread
void* reindexing(void* agruments);                                                              //function performed by thread responsible for running re-indexing
int check_file_type(const char* path);                                                          //helper function which detects file type
void cancelling(void* arg);                                                                     //clean-up handler used in exit! command
void count_types(thread_struct* struct_idx,int* zip,int* gzip,int* jpeg,int* png,int* dir);     //helper function for count command
void finding_info(char* command,command_t type,thread_struct* struct_idx);                      //function to perform largerthan, count and namepart
void write_to_file(const char* path,index_structure* struct_idx);                               //function for writing data to the file with path "path"
void read_a_file(const char* path,index_structure* struct_idx);                                 //function for reading data from the file with path "path"
ssize_t reading_buf(int fptr,void* buf,size_t size);                                            //helper function for reading
ssize_t writing_buf(int fptr,void* buf,size_t size);                                            //helper function for writing
void printing_info(FILE* stream,index_structure* struct_idx,command_t type,int x,char* temp);   // helper function for printing in largerthan namepart owner commands
void taking_file_time(char* path,thread_struct* pth);                                           //takes time from the file and saves it in thread
void command_read(thread_struct* pth,char* command);                                            //read commands from user
void create_thread(thread_struct* pth,index_structure main,pthread_mutex_t* mutex_op,
    pthread_mutex_t* mutex_ind,pthread_mutex_t* mutex_clc,char* path_f,char* path_d,int n);     //initialize thread values

index_structure tempIndex;

int main(int argc,char** argv){
    int n=0;                                                                                    //variable storing -t argument 
    char* path_d=NULL;                                                                          //variable storing -d argument 
    char* path_f=NULL;                                                                          //variable storing -f argument 
    read_arg(argc,argv,&path_d,&path_f,&n);
    thread_struct* pth;                                                                         //pointer to thread structure

    if((pth=(thread_struct*)calloc(1,sizeof(thread_struct)))==NULL) ERR("calloc()");

    index_structure main;                                                                       //pointer to main index structure
    pthread_mutex_t mutex_op=PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mutex_ind=PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mutex_clc=PTHREAD_MUTEX_INITIALIZER;
    create_thread(pth,main,&mutex_op,&mutex_ind,&mutex_clc,path_f,path_d,n);

    int fptr;                                                                                   //file pointer
    fptr=TEMP_FAILURE_RETRY(open(path_f,O_RDONLY|O_CREAT|O_EXCL,0666));
    if(fptr==-1){   
        if(EEXIST!=errno) ERR("open()");
        read_a_file(path_f,&pth->main);                                    
        if(n!=0){
            taking_file_time(path_f,pth);
            if((pthread_create(&(pth->pth_id),NULL,reindexing,pth))!=0) ERR("pthread_create()"); }
        }
    else{
        if((TEMP_FAILURE_RETRY(close(fptr)))==-1) ERR("close()");
        if((pthread_create(&(pth->pth_id),NULL,thread_work,pth))!=0) ERR("pthread_create()");
        if(n!=0) { if((pthread_create(&(pth->pth_id),NULL,reindexing,pth))!=0) ERR("pthread_create()"); } 
    }
    char command[MAX_COMMAND];                                                                  //user command variable

    command_read(pth,command); 
    if(strcmp(command,"exit!")!=0){
        free(pth->main.indexStruct);
        if(pthread_mutex_destroy(pth->mutex_indexing)!=0) ERR("pthread_mutex_destroy()");
    }
    if((strcmp(command,"exit")!=0&&(strcmp(command,"exit!"))!=0))
    if((pthread_join(pth->pth_id,NULL))!=0) ERR("pthread_join()");
    if(pthread_mutex_destroy(pth->mutex_open)!=0) ERR("pthread_mutex_destroy()");
    if(pthread_mutex_destroy(pth->mutex_clock)!=0) ERR("pthread_mutex_destroy()");
    free(pth);
    return EXIT_SUCCESS;
}

void create_thread(thread_struct* pth,index_structure main,pthread_mutex_t* mutex_op,pthread_mutex_t* mutex_ind,pthread_mutex_t* mutex_clc,char* path_f,char* path_d,int n){

    if((main.indexStruct=(index_data*)calloc(1,sizeof(index_data)))==NULL) ERR("calloc()");
    main.indexStruct_size=0;
    pth->main=main;
    pth->directory=path_d;
    pth->filepath=path_f;
    pth->reidx_time=n;
    pth->is_indexing=false;
    pth->is_open=false;
    pth->mutex_indexing=mutex_ind;
    pth->mutex_open=mutex_op;
    pth->mutex_clock=mutex_clc;
}

void taking_file_time(char* path,thread_struct* pth){

    struct stat _time;                                              //variable storing file stats
    if((stat(path,&(_time)))!=0) ERR("fstat()");                    
    pthread_mutex_lock(pth->mutex_clock);
    pth->clock=_time.st_mtim.tv_sec;                                
    pthread_mutex_unlock(pth->mutex_clock);
}

void command_read(thread_struct* pth,char* command){
    while(1) {
        if((fgets(command,MAX_COMMAND,stdin))==NULL) ERR("fgets()");                                        
        command[strlen(command)-1]='\0';                                               
        if(strcmp(command,"exit")==0){
            if(pth->is_open==true) { if((pthread_join(pth->pth_id,NULL))!=0) ERR("pthread_join()"); }
            break;                                                                     
        } 
        else if(strcmp(command,"exit!")==0){
            pthread_mutex_lock(pth->mutex_indexing);
            if(pth->is_indexing==true){
                if((pthread_cancel(pth->pth_id))!=0) ERR("pthread_cancel()");          
                pthread_mutex_unlock(pth->mutex_indexing);
            }
            else if(pth->is_open==true){
                pthread_mutex_unlock(pth->mutex_indexing);
                if((pthread_join(pth->pth_id,NULL))!=0) ERR("pthread_join()");                              
            }
            else pthread_mutex_unlock(pth->mutex_indexing);
            break;
        }
        else if(strcmp(command,"count")==0){
            int zip=0,gzip=0,jpeg=0,png=0,dir=0;                                                            //variables of each file type amount
            count_types(pth,&zip,&gzip,&jpeg,&png,&dir);
            }
        else if(strncmp(command,"largerthan",10)==0) finding_info(command,largerthan,pth);   
        else if(strncmp(command,"namepart",8)==0) finding_info(command,namepart,pth);
        else if(strncmp(command,"owner",5)==0) finding_info(command,owner,pth);
        else if(strcmp(command,"index")==0){
            pthread_mutex_lock(pth->mutex_indexing);
            if(pth->is_indexing==true) { 
                pthread_mutex_unlock(pth->mutex_indexing);
                if(puts("Warning: indexing is already performing")==EOF) ERR("puts()"); }
            else{ 
                if((pthread_create(&(pth->pth_id),NULL,thread_work,pth))!=0) ERR("pthread_create()");
                pthread_mutex_unlock(pth->mutex_indexing);}
        }
        else { if(printf("The command is invalid. It should be either exit, exit!, index_data, count, largerthan [size], namepart [name] or owner [uid].\n")<0) ERR("printf()");}
    }
}


void read_arg(int argc,char** argv,char** path_d,char** path_f,int* n){

    int c;                                                                                                //variable used in getoped
    struct stat info_path;                                                                                //variable storing info about path_d given by user
    while((c=getopt(argc,argv,"d:f:t:"))!=-1){

        switch (c){

        case 'd': 
            *path_d=optarg; 
            if(stat(*path_d,&info_path)!=0) usage("Cannot access directory");
            else if(!(S_ISDIR(info_path.st_mode))) usage("Given path is incorrect");              
            break; 
        case 'f': 
            *path_f=optarg;
            break;
        case 't': 
            (*n)=atoi(optarg);       
            if((*n)<30||(*n)>7200) usage("Value given to atoi is incorrect");
            break;
        default: usage("Usage: -d [dir_path] -f [file_path] -t [time_in_s]");
            break;
        }
    }
    if(*path_d==NULL){
        if((*path_d=getenv("MOLE_DIR"))==NULL) ERR("getenv()");
    }
    if(*path_f==NULL){
        if((*path_f=getenv("MOLE_INDEX_PATH"))==NULL) {

            char* path_name;                                        
            if((path_name=getenv("HOME"))==NULL) ERR("getenv()");
            strcat(path_name,"/.mole-index_data");
            *path_f=path_name;                    
        }
    }
}

int indexing(const char* name,const struct stat *s,int type,struct FTW *f){

    if( type==FTW_DP || type==FTW_F){

        if(type==FTW_F && check_file_type(name)==0) return 0;        
        tempIndex.indexStruct_size++;
        if((tempIndex.indexStruct=(index_data*)realloc(tempIndex.indexStruct,sizeof(index_data)*tempIndex.indexStruct_size))==NULL) ERR("realloc()");    
        if(strchr(name,'/')==NULL){

            if(strlen(name)>MAX_NAME){

                if((TEMP_FAILURE_RETRY(fprintf(stderr,"Name has to be no longer thatn 50 characters\n")))<0) ERR("fprintf()"); //printing the error/warning message to stderr
                return 0;
            }
            strcpy(tempIndex.indexStruct[tempIndex.indexStruct_size-1].file_name,name);
            strcpy(tempIndex.indexStruct[tempIndex.indexStruct_size-1].path,name);
        }
        else{

            if(strlen(strrchr(name,'/'))>MAX_NAME){

                if((TEMP_FAILURE_RETRY(fprintf(stderr,"Name has to be no longer thatn 50 characters\n")))<0) ERR("fprintf()"); //printing the error/warning message to stderr
                return 0;
            }
            if(strlen(name)>MAX_PATH){

                if((TEMP_FAILURE_RETRY(fprintf(stderr,"Path has to be no longer thatn 100 characters\n")))<0) ERR("fprintf()");
                return 0;
            }
            strcpy(tempIndex.indexStruct[tempIndex.indexStruct_size-1].file_name,strrchr(name,'/'));
            strcpy(tempIndex.indexStruct[tempIndex.indexStruct_size-1].path,name);
        }
        tempIndex.indexStruct[tempIndex.indexStruct_size-1].owner_id=s->st_uid;                                  
        tempIndex.indexStruct[tempIndex.indexStruct_size-1].size=s->st_size;                                        
        if(type==FTW_DP)
            tempIndex.indexStruct[tempIndex.indexStruct_size-1].type=dir;
        else
            tempIndex.indexStruct[tempIndex.indexStruct_size-1].type=check_file_type(name);         
    }
    return 0;
}

void* thread_work(void* arguments){

    thread_struct* temp=arguments;                                                                   //saving arguments to temporary thread_struct
    tempIndex.indexStruct_size=0;
    if((tempIndex.indexStruct=(index_data*)calloc(1,sizeof(index_data)))==NULL) ERR("calloc()");
    pthread_cleanup_push(cancelling,temp);                                                              
    temp->is_indexing=true;
    if(temp->main.indexStruct_size==0){

        if(pthread_mutex_lock(temp->mutex_indexing)!=0) ERR("pthread_mutex_lock()");
        if((nftw(temp->directory,indexing,MAXFD,FTW_DEPTH))==-1) ERR("nftw()");
    }
    else{

        if((nftw(temp->directory,indexing,MAXFD,FTW_DEPTH))==-1) ERR("nftw()");
        if(pthread_mutex_lock(temp->mutex_indexing)!=0) ERR("pthread_mutex_lock()");
    }
    temp->main.indexStruct_size=tempIndex.indexStruct_size;
    if((temp->main.indexStruct=(index_data*)realloc(temp->main.indexStruct,sizeof(index_data)*temp->main.indexStruct_size))==NULL) ERR("realloc()");
    memcpy(temp->main.indexStruct,tempIndex.indexStruct,sizeof(index_data)*temp->main.indexStruct_size);
    if(temp->reidx_time!=0){

        pthread_mutex_lock(temp->mutex_clock);
        if((time(&(temp->clock)))==-1) ERR("time()");                                                   
        pthread_mutex_unlock(temp->mutex_clock);
    }
    temp->is_indexing=false;
    if(pthread_mutex_unlock(temp->mutex_indexing)!=0) ERR("pthread_mutex_unlock()");
    pthread_cleanup_pop(0);                                                                         
    free(tempIndex.indexStruct);
    if((TEMP_FAILURE_RETRY(fprintf(stdout,"Indexing has finished\n")))<0) ERR("fprintf()");                              
    if(pthread_mutex_lock(temp->mutex_open)!=0) ERR("pthread_mutex_unlock()");
    temp->is_open=true;                                                                          
    write_to_file(temp->filepath,&(temp->main));                                                   
    temp->is_open=false;                                                                         
    if(pthread_mutex_unlock(temp->mutex_open)!=0) ERR("pthread_mutex_unlock()");
    return NULL;
}

void* reindexing(void* arguments){

    thread_struct* temp=arguments;                                                                      //saving arguments in temporary variable
    int err;                                                                                            //error indication
    while (1){

        pthread_mutex_lock(temp->mutex_clock);
        if((time(NULL)-temp->clock)>temp->reidx_time && temp->is_indexing==false){

            pthread_mutex_unlock(temp->mutex_clock);
            temp->is_indexing=true;                                                                   
            if((err=pthread_create(&(temp->pth_id),NULL,thread_work,temp))!=0) ERR("pthread_creat()"); 
            sleep(2);
        }
        else{

            pthread_mutex_unlock(temp->mutex_clock);
            sleep(temp->reidx_time-(time(NULL)-temp->clock));   
        }       
    }
    return NULL;
}

ssize_t reading_buf(int fptr,void* buf,size_t size)
{
    ssize_t c;                                      //number of read chars
    ssize_t length=0;                               //read length
    do{

        c=TEMP_FAILURE_RETRY(read(fptr,buf,size));
        if(c<0) return c;
        if(c==0) return length;
        buf+=c;
        length+=c;
        size-=c;
    } while (size>0);

    return length;
}

ssize_t writing_buf(int fptr,void* buf,size_t size)
{
    ssize_t c;                                      //number of read chars
    ssize_t length = 0;                             //read length
    do
    {
        c=TEMP_FAILURE_RETRY(write(fptr,buf,size));
        if(c<0) return c;
        buf+=c;
        length+=c;
        size-=c;
    } while (size>0);

    return length;
}

void write_to_file(const char* path,index_structure* struct_idx)
{
    int fptr;                                                                                       //file pointer
    if((fptr=TEMP_FAILURE_RETRY(open(path,O_WRONLY|O_CREAT|O_TRUNC,0666)))==-1) ERR("open()");
    for(int i=0;i<struct_idx->indexStruct_size;i++)
    {
         if(writing_buf(fptr,&struct_idx->indexStruct[i],sizeof(index_data))<0) ERR("write()");
    }    
    if((TEMP_FAILURE_RETRY(close(fptr)))==-1) ERR("close()");
}

void read_a_file(const char* path,index_structure* struct_idx)
{
    int count=-1;
    int fptr;
    if((fptr=TEMP_FAILURE_RETRY(open(path,O_RDONLY,0666)))==-1) ERR("open()");
    index_data buf;
    while(count!=0)
    {
        if((count=reading_buf(fptr,&buf,sizeof(index_data)))<0) ERR("read()");
        struct_idx->indexStruct_size++;
        struct_idx->indexStruct=(index_data*)realloc(struct_idx->indexStruct,sizeof(index_data)*struct_idx->indexStruct_size);
        struct_idx->indexStruct[struct_idx->indexStruct_size-1]=buf; 
    }
    if((TEMP_FAILURE_RETRY(close(fptr)))==-1) ERR("close()");
}

int check_file_type(const char* path)
{
    int fptr;                                                                          //file pointer
    unsigned char bytes[3]={0};                                                        //variable storing 3 first bytes of file    
    if((fptr=TEMP_FAILURE_RETRY(open(path,O_RDONLY,0666)))==-1) ERR("fopen()");                        
    if((reading_buf(fptr,bytes,3))<0) ERR("read()");                                                          
    if((TEMP_FAILURE_RETRY(close(fptr)))==-1) ERR("close()");                              
    if(bytes[0]==0xFF && bytes[1]== 0xD8 && bytes[2]==0xFF) return jpeg;  
    if(bytes[0]==0x89 && bytes[1]== 0x50 && bytes[2]==0x4E) return png;  
    if(bytes[0]==0x1F && bytes[1]== 0x8B) return gzip;                    
    if(bytes[0]==0x50 && bytes[1]== 0x4B && bytes[2]==0x03) return zip;  
    return 0;                                                                 
}

void cancelling(void* arg)
{
    thread_struct* temp=arg;
    pthread_mutex_unlock(temp->mutex_indexing);  
    free(temp->main.indexStruct);
}

void count_types(thread_struct* pth,int* _zip,int* _gzip,int* _jpeg,int* _png,int* _dir) 
{
    pthread_mutex_lock(pth->mutex_indexing);
    for(int i=0;i<pth->main.indexStruct_size;i++)          
    {
        if(pth->main.indexStruct[i].type==jpeg)
            (*_jpeg)++;
        else if(pth->main.indexStruct[i].type==png)
            (*_png)++;
        else if(pth->main.indexStruct[i].type==zip)
            (*_zip)++;
        else if(pth->main.indexStruct[i].type==gzip)
            (*_gzip)++;
        else if(pth->main.indexStruct[i].type==dir)
            (*_dir)++;
    }
    pthread_mutex_unlock(pth->mutex_indexing);

    printf("\nTypes count:\njpeg: %d\npng: %d\ngzip: %d\nzip: %d\ndir: %d\n\n",*_jpeg,*_png,*_gzip,*_zip,*_dir); 
}

void finding_info(char* command,command_t type,thread_struct* pth)
{
    char* temp;
    int x;
    int count=0;
    if(type==largerthan){
            if((temp=(char*)calloc(strlen(command)-10,sizeof(char)))==NULL) ERR("calloc()");
            memcpy(temp,&command[11],strlen(command)-10);
            if((x=atoi(temp))==0) ERR("atoi()");
    }
    else if(type==namepart){
            if((temp=(char*)calloc(strlen(command)-8,sizeof(char)))==NULL) ERR("calloc()");
            memcpy(temp,&command[9],strlen(command)-8);
    }
    else if(type==owner){
            if((temp=(char*)calloc(strlen(command)-5,sizeof(char)))==NULL) ERR("calloc()");
            memcpy(temp,&command[6],strlen(command)-5);
            if((x=atoi(temp))==0) ERR("atoi()");
    }
    pthread_mutex_lock(pth->mutex_indexing);
    for(int i=0;i<pth->main.indexStruct_size;i++){
        if((type==largerthan && pth->main.indexStruct[i].size>x)||(type==namepart && strstr(pth->main.indexStruct[i].file_name,temp)!=NULL) || (type==owner && pth->main.indexStruct[i].owner_id==x))
            count++;
    }
    if(count<3 || !getenv("PAGER")){
        printing_info(stdout,&pth->main,type,x,temp);
        pthread_mutex_unlock(pth->mutex_indexing);
    }
    else{
        FILE *fptr;
        if((fptr=popen(getenv("PAGER"),"w"))==NULL) ERR("popen()");
        printing_info(fptr,&pth->main,type,x,temp);
        pthread_mutex_unlock(pth->mutex_indexing);
        if((pclose(fptr))==-1) ERR("pclose()"); 
    }
    free(temp);
}

void printing_info(FILE* stream,index_structure* struct_idx,command_t type,int x,char* temp)
{
    for(int i=0;i<struct_idx->indexStruct_size;i++)
    {
        if((type==0 && struct_idx->indexStruct[i].size>x)||(type==1 && strstr(struct_idx->indexStruct[i].file_name,temp)!=NULL) || (type==2 && struct_idx->indexStruct[i].owner_id==x))
            if((TEMP_FAILURE_RETRY(fprintf(stream,"%s %s %ld %s\n",struct_idx->indexStruct[i].path,struct_idx->indexStruct[i].file_name,struct_idx->indexStruct[i].size,enum_as_string[struct_idx->indexStruct[i].type])))<0) ERR("fprintf()");
    }
}


