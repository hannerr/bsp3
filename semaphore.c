
#include <stdio.h>
#include <stdlib.h> 
#include <sem182.h>
#include <errno.h>
#include <string.h> 
#include <unistd.h>
#include <limits.h>
#include <sys/shm.h>
#include "semallg.h"


#define  KEY_SEM_0        1
#define  KEY_SEM_1        2
#define  KEY_MEM          3

#define  ANZ_SEM          2
#define  START            0 
#define  END              1

#define  SEM_TRUE         1
#define  SEM_FALSE        0

 
typedef struct
   {                          
   int           sem_art;
   int           ring_id;
   int*          p_ring_buffer;  
   int           ring_zeiger;
   int           semaphore [ANZ_SEM];
   char *        p_name;
   } SHARED_STRUCT;                   
/* ______________________________TODO_________________________*/
/* die Hauptfunktion                                          */ 
int sema_function (int argc, char * const argv[],int art);
/* ______________________________TODO_________________________*/
/* aufräumen und frei machen                                   */
static void clean_setfree (SHARED_STRUCT * p_shared_struct);
static int  create_sema (SHARED_STRUCT * p_shared_struct, key_t key, long mem_size);
static void error_function (SHARED_STRUCT * p_shared_struct, const char * p_error_message);


/* semaphore erstellen                                       */
static int create_sema (SHARED_STRUCT * p_shared_struct, key_t key, long mem_size){
   int value  = 0;
   value = seminit(key, 0660, mem_size);

   /* Fehlerfall..*/
   if (value == -1){
   /* ..und es gibt eine Fehlernummer..*/
      if (errno == EEXIST){  
         value = semgrab(key);     
         if (value == -1) {    
            error_function (p_shared_struct, "semgrab");
            }
         }
      else {              
         error_function (p_shared_struct, "seminit");
         }
      }
   return value;
}


/* Fehlerbehandlung*/
static void error_function (SHARED_STRUCT* p_shared_struct, const char * p_error_message) {
   /* wenn es einen Fehler gibt.. */
   if ((p_shared_struct != NULL) && (p_error_message  != NULL))
      {     
      /* ..und es eine errno dazu auch gibt */                              
      if (errno != 0) {                                
         fprintf(stderr, "Usage: %s: error %s - %s \n", p_shared_struct->p_name, p_error_message , strerror(errno));
         }
      else
         {                                
         fprintf(stderr, "Usage: %s: error %s \n", p_shared_struct->p_name, p_error_message);
         }     
      /* aufräumen und freigeben*/
      clean_setfree (p_shared_struct);
      }
   exit(EXIT_FAILURE);
}



  

