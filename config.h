/*The config file defines the values which will be used in mole program*/

#define MAX_PATH 300    //MAX_PATH- the maximum size of the provided path
#define MAX_NAME 100    //MAX_NAME- maximum size of the provided file name
#define MAX_TYPE 5      //MAX_TYPE- maximum size of file type
#define MAX_COMMAND 100 //MAX_COMMAND- maximum size of provided command
#define MAXFD 20        //MAXFD-maximum number of file descriptors

//ERR- macro handling function errors
#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE)) 


