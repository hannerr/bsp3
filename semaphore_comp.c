
#define  LINUX_FH
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

static void clean_setfree  (SHARED_STRUCT * p_shared_struct);
static void error_function (SHARED_STRUCT * p_shared_struct, const char * p_error_message);
static int  create_sema        (SHARED_STRUCT * p_shared_struct, key_t key, long mem_size);


int sema_function     (int argc, char * const argv[],int art)
{
BOOL             f_parameter_ok = SEM_TRUE;
char             *p_end         = NULL;
long             ring_size      = 0;
int              opt            = 0;
uid_t            user_id        = (getuid () * 1000);
int              flags          = 0;
int              zeichen        = 0;
SHARED_STRUCT   sv_shared_info;

/************************************************************************/
/* Phase 1) Allgemeine Auswertung der internen Uebergabe                */
/************************************************************************/
if ((art != SEMA_EMPFAENGER) &&
    (art != SEMA_SENDER))
   {                                   /* Uebergabe ist nicht valide !  */
/*
 * ### FB_TMG: Da wäre eher ein assert(0) angebracht, weil das dürfte bei
 * Ihrem Code nicht passieren.
 */
   fprintf(stderr, "Usage: %s -h -m <ring buffer size> \n", argv[0]);
   
   exit(EXIT_FAILURE);
   }
/* endif: Phase 1) Allgemeine Auswertung der internen Uebergabe         */  
  
/************************************************************************/
/* Phase 2) Auswertung des SHELL Uebergabeparameter                     */
/************************************************************************/
/* Bestimmen der Uebergabeparameter !                                   */
while ((opt = getopt(argc, argv, "hm:")) != -1)
   {  
   /* Auswertung ob der Uebergabeparameter gefunden wurde !             */
   switch (opt)
      {
      case 'm':                        /* MEMORY-Size !                 */
         ring_size = strtol(optarg, &p_end, 10);
         
         /* Wurde ein Fehler gefunden ???                               */
/*
 * ### FB_TMG: Wenn Sie nicht vorher checken, ob strtol() LONG_MAX
 * zurückgeliefert hat, dann könnte errno auch aufgrund eines anderen
 * fehlerhaften Syscalls auf ERANGE stehen
 */  
         if (( errno                        == ERANGE) ||
             ((errno != 0)    && (ring_size == 0))     ||
             (*p_end                        != '\0')   ||
/*
 * ### FB_TMG: Unvollständige Fehlerbehandlung - Was, wenn ring_size
 * größer ist als der maximale Wert ist, den ein size_t aufnehmen kann
 * bzw. größer als SHMMAX
 */
             (ring_size                     >  INT_MAX)|| 
             (ring_size                     <= 0)) 
            {                          /* FEHLER !!                     */
            f_parameter_ok = SEM_FALSE;
            }
         /* endif: Wurde ein Fehler gefunden ???                        */
         break;
         
      case 'h':                        /* HELP !                        */
         f_parameter_ok = SEM_FALSE;
         break;
         
      default :                        /* FEHLER !!!                    */
         f_parameter_ok = SEM_FALSE;
         break;
      }
   /* endswitch: Auswertung ob der Uebergabeparameter gefunden wur      */
   }
/* endwhile: Bestimmen der Uebergabeparameter !                         */  

/* Wurde bei der Buffergroessen Bestimmung ein Fehler gefunden ?        */
if ((optind         != argc) || 
    (f_parameter_ok == SEM_FALSE))
   {                                   /*  */
   fprintf(stderr, "Usage: %s -h -m <ring buffer size> \n", argv[0]);
   /* no resource clean up necessary nothing reserved yet               */
   exit(EXIT_FAILURE);
   }
/* endif: Wurde bei der Buffergroessen Bestimmung ein Fehler gefund     */

/************************************************************************/
/* Phase 3) Default-Zuweisung                                           */
/************************************************************************/
sv_shared_info.sem_art            = art;
sv_shared_info.ring_id            = -1;
sv_shared_info.p_ring_buffer      = (int*) -1;
sv_shared_info.ring_zeiger        =  0;
sv_shared_info.semaphore [START]  = -1;
sv_shared_info.semaphore [END  ]  = -1;
sv_shared_info.p_name     = *argv;

/************************************************************************/
/* Phase 4) Auswertung des SHELL Uebergabeparameter                     */
/************************************************************************/
/* Auswertung der Art                                                   */
switch (sv_shared_info.sem_art)
   {
   case SEMA_SENDER:       /* SENDER                                 */
      sv_shared_info.semaphore [START] = create_sema (&sv_shared_info,user_id + KEY_SEM_0, ring_size);
      sv_shared_info.semaphore [END  ] = create_sema (&sv_shared_info,user_id + KEY_SEM_1, 0          );
      break;
   case SEMA_EMPFAENGER:   /* EMPFAENGER                             */
      sv_shared_info.semaphore [START] = create_sema (&sv_shared_info,user_id + KEY_SEM_1, 0          );
      sv_shared_info.semaphore [END  ] = create_sema (&sv_shared_info,user_id + KEY_SEM_0, ring_size);
      flags                            = SHM_RDONLY;
      break;
   default :                  /* SW-Bug !                               */
      fprintf(stderr, "Usage: %s <SW-BUG> \n", argv[0]);
      exit(EXIT_FAILURE);
      break;
   }
/* endswitch: Auswertung der Art                                        */

/* Anlegen eines Speichers wenn es noch nicht exisistiert               */
sv_shared_info.ring_id =  shmget(user_id + KEY_MEM, sizeof(int) * ring_size, 0660|IPC_CREAT);

/* Wurde ein Fehler erkannt ?                                           */
if (sv_shared_info.ring_id == -1)
   {                                   /* JA:-> Ausstieg !              */
   error_function (&sv_shared_info,"shmget");
   }
/* endif: Wurde ein Fehler erkannt ?                                    */

sv_shared_info.p_ring_buffer = shmat(sv_shared_info.ring_id, NULL, flags);

/* Wurde ein Fehler erkannt ???                                         */
if (sv_shared_info.p_ring_buffer == (int*) -1)
   {                                   /* JA:-> Ausstieg !              */
   error_function (&sv_shared_info,"shmat");
   }
/* endif: Wurde ein Fehler erkannt ???                                  */

/************************************************************************/
/* Phase 5) Eigentliches Hauptprogramm !                                */
/************************************************************************/
/* Solange bis fertig !                                                 */
while (1)
   {                                   
   /* Haben wir einen Sender ???                                        */
   if (sv_shared_info.sem_art == SEMA_SENDER)
      {                                /* Bestimmen des naechsten Charakters */
      zeichen = fgetc(stdin);
      
      /* Wurde ein Fehler erkannt ???                                  */
/*
 * ### FB_TMG: Sehr schön!
 */
      if (ferror(stdin))
         {                             /* JA:-> Abbruch !!!            */
         error_function(&sv_shared_info,"fgetc");
         }
      /* endif:  Wurde ein Fehler erkannt ???                          */
      }
   /* endif: Haben wir einen Sender ???                                 */
   
   /* Warten bis EMAPHORE frei ist !                                    */
   while (P(sv_shared_info.semaphore[START]) != 0)
      {                                
      /* Kann weitergemacht werden ???                                  */
      if (errno == EINTR)
         {                             /* JA:->                         */
         continue;
         }
      else
         {                             /* NEIN:-> FEHLER ABBRUCH !!!    */
         error_function (&sv_shared_info,"P");
         }
      /* endif: Kann weitergemacht werden ???                           */
      }
   /* endwhile: Warten bis EMAPHORE frei ist !                          */
   
   /* Auswertung der Art !                                              */
   switch (sv_shared_info.sem_art)
      {
      case SEMA_SENDER:   /* Sender                                  */
         sv_shared_info.p_ring_buffer[sv_shared_info.ring_zeiger] = zeichen;
         break;
      case SEMA_EMPFAENGER: /* Empfaenger                            */
         zeichen = sv_shared_info.p_ring_buffer[sv_shared_info.ring_zeiger];
         break;
      default :              /* Unbekannt !                             */
         break;
      }
   /* endswitch: Auswertung der Art !                                   */
   
   /* Warten bis EMAPHORE frei ist !                                    */
   while (V(sv_shared_info.semaphore[END]) != 0)
      {                                
      /* Kann weitergemacht werden ???                                  */
      if (errno == EINTR)
         {                             /* JA:->                         */
         continue;
         }
      else
         {                             /* NEIN:-> FEHLER ABBRUCH !!!    */
         error_function (&sv_shared_info,"V");
         }
      /* endif: Kann weitergemacht werden ???                           */
      }
   /* endwhile: Warten bis EMAPHORE frei ist !                          */
   
   /* Wurde ein EOF erkannt ?                                           */
   if (zeichen == EOF)
      {                                /* JA:->                         */
      break;
      }
   else
      {                                /* NEIN:-> Weiter gehts ...      */
      /* Haben wir den Empfaenger ??                                    */
      if (sv_shared_info.sem_art == SEMA_EMPFAENGER)
         {                             /* JA:-> Ausgabe ...             */
         /* Konnte eine Ausgabe durchgefuehrt werden ???                */
         if (fputc (zeichen, stdout) < 0)
            {                          /* NEIN:-> Fehler ...            */
            error_function(&sv_shared_info,"fputc");
            }
         /* endif: Konnte eine Ausgabe durchgefuehrt werden ???         */
         }
      /* endif: Haben wir den Empfaenger ??                             */
      }
   /* endif: Wurde ein EOF erkannt ?                                    */
   
   /* Zeiger weiterstellen                                              */
   sv_shared_info.ring_zeiger++;
   sv_shared_info.ring_zeiger = sv_shared_info.ring_zeiger % ring_size;
   }
/* endwhile: Solange bis fertig !                                       */

/************************************************************************/
/* Phase 6) Alle wieder aufraeumen                                      */
/************************************************************************/
/* Haben wir einen Empfaenger ???                                       */
if (sv_shared_info.sem_art == SEMA_EMPFAENGER)
   {                                   /* JA:-> Empfaenger !            */
   /* Wurde beim FFLUSH ein Fehler erkannt ???                          */
/*
 * ### FB_TMG: Sehr schön!
 */
   if (fflush(stdout) == EOF)
      {  
      error_function(&sv_shared_info, "fflush");
      }
   /* endif: Wurde beim FFLUSH ein Fehler erkannt ???                   */
   }
/* endif: Haben wir einen Empfaenger ???                                */

clean_setfree(&sv_shared_info);
  
return EXIT_SUCCESS;
}

/**
*
* \brief free all reserved resources. 
*        checks every resource if already inialized and only free it if necessary   
*
* \param argv command line arguments
* \param shared_mem struct to handover shared memory data
* \param type is receiver or sender 
*
*/
static void clean_setfree (SHARED_STRUCT* p_shared_struct){  
int i_anz_sem;

/* Uebergabe valide ???                                                  */
if (p_shared_struct != NULL)
   {                                   /* JA:->                          */
   /* Wurde ein Pointer zugewiesen ???                                   */
   if (p_shared_struct->p_ring_buffer != (int*)-1)
      {                                /* JA:->                          */
      /* Kann der Speicher wieder ausgeblendet werden ???                */
      if (shmdt(p_shared_struct->p_ring_buffer) == -1)
         {                             /* NEIN:-> Error ausgeben !       */
         fprintf(stderr, "Usage: %s: %s - %s \n", p_shared_struct->p_name, "error shmdt", strerror(errno));
         
         }
      /* endif: Kann der Speicher wieder ausgeblendet werden ???         */
      p_shared_struct->p_ring_buffer = (int*)-1;
      }
   /* endif: Wurde ein Pointer zugewiesen ???                            */
   
   /* Auswertung nur fuer den Empfaenger                                 */
/*
 * ### FB_TMG: Im Fehlerfall sollen Sie doch *alle* Resourcen
 * wegräumen, die Sie bis dahin angelegt haben [-2]
 */
   if (p_shared_struct->sem_art == SEMA_EMPFAENGER)
      {                     
      /* Alle Semaphore durchlaufen !                                    */
      for (i_anz_sem = 0 ;i_anz_sem < ANZ_SEM ; i_anz_sem++)
         {  
         /* Kann dieser Semaphore zurueckgegeben werden ???              */
         if (p_shared_struct->semaphore [i_anz_sem] != -1)
            {                          /* JA:->                          */
            /* Fehler bei der Rueckgabe ???                              */
            if (semrm(p_shared_struct->semaphore [i_anz_sem]) == -1)
               {                       /* JA:-> Ausgabe !                */
               fprintf(stderr, "Usage: %s: %s (%d) - %s \n", p_shared_struct->p_name, "error semrm",i_anz_sem ,strerror(errno));
               }
            /* endif:  Fehler bei der Rueckgabe ???                      */
            p_shared_struct->semaphore [i_anz_sem] = -1;
            }
         /* endif: Kann dieser Semaphore zurueckgegeben werden ???       */
         }
      /* endfor: Alle Semaphore durchlaufen !                            */
      
      /* Soll das Shared Memeory Segment (Ringpuffer) entfernt werden ?  */
      if (p_shared_struct->ring_id != -1)
         {                             /* JA:->                          */
         /* Kann es entfernt werden ???                                  */
         if (shmctl(p_shared_struct->ring_id, IPC_RMID, NULL) == -1)
            {                          /* NEIN:-> Fehler !!!             */
            fprintf(stderr, "Usage: %s: %s - %s \n", p_shared_struct->p_name, "error shmctl", strerror(errno));
            }
         /* endif: Kann es entfernt werden ???                           */
         p_shared_struct->ring_id = -1;
         }
      /* endif: Soll das Shared Memeory Segment (Ringpuffer) entfernt .. */
      }
   /* endif: Auswertung nur fuer den Receiver !!                         */
   }
else
   {                                   /* SCHWERER SW-Bug !!             */
   exit(EXIT_FAILURE);
   }
/* endif: Uebergabe valide ???                                           */
}


/**
*
* \brief Function is called, if an error occured.
*        It prints the error message, call a function to free
*        all resources and exit  
*
* \param shared_info struct to handover shared memory data
* \param Pointer error_message the error message decribes the the error which occured
*
*/
static void error_function (SHARED_STRUCT* p_shared_struct, const char * p_error_message)
{
/* Sind die Uebergabeparameter valide ???                                */
if ((p_shared_struct != NULL) && 
    (p_error_message  != NULL))
   {                                   /* JA:-> Error Handling durchfuehren ! */
   /* Kanne eine errno Auswertung durchgefuehrt werden ???               */
   if (errno != 0)
      {                                /*  JA:-> Ausabe der ERRNO als Zusatzinfo*/
      fprintf(stderr, "Usage: %s: error %s - %s \n", p_shared_struct->p_name, p_error_message , strerror(errno));
      }
   else
      {                                /* NEIN:-> Ausgabe OHNE Errno-Text */
      fprintf(stderr, "Usage: %s: error %s \n", p_shared_struct->p_name, p_error_message);
      }
   /* endif: Kanne eine errno Auswertung durchgefuehrt werden ???        */
   
   /* Alle Resourcen Aufraeumen und aussteigen !                         */
   clean_setfree (p_shared_struct);
   }
/* endif: Sind die Uebergabeparameter valide ???                         */

exit(EXIT_FAILURE);
}


/**
*
* \brief Function to get one semaphore 
*
* \param shared_info struct to handover shared memory data
* \param key for which the semaphore should be created
* \param mem_size memory size
*
* \return handle to the semaphore
*
*/
static int create_sema (SHARED_STRUCT * p_shared_struct, key_t key, long mem_size)
{
int value  = 0;

value = seminit(key, 0660, mem_size);

/* Wurde ein Fehler gefunden ???                                        */
if (value == -1)
   {                                   /* JA:->                         */
   /* Auswertung des Fehlergrunds !                                     */
/*
 * ### FB_TMG: Hier errno auf EEXIST zu prüfen ist sehr sauber! [+2]
 */
   if (errno == EEXIST)
      {  
      value = semgrab(key);
      
      /* Wurde ein Fehler gefunden ???                                  */
      if (value == -1)
         {                             /* JA:-> Abbruch !               */
         error_function (p_shared_struct, "semgrab");
         }
      /* endif: Wurde ein Fehler gefunden ???                           */
      }
   else
      {                                /* Allgemeiner Fehler !          */
      error_function (p_shared_struct, "seminit");
      }
   /* endif: Auswertung des Fehlergrunds !                              */
   }
/* endif: Wurde ein Fehler gefunden ???                                 */

return value;
}
  
/*
 * =================================================================== eof ==
 */

