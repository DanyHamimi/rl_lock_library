#ifndef RL_LOCK_LIBRARY_H
#define RL_LOCK_LIBRARY_H

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>


#define NB_OWNERS 10
#define NB_FILES 256
#define NB_LOCKS 256
#define MAX_LOCKS 256
#define PATH_MAX 256


typedef struct{
    pid_t proc;
    int des;
} owner;

typedef struct {
    int next_lock;
    off_t starting_offset;
    off_t len;
    short type; // F_RDLCK ou F_WRLCK
    size_t nb_owners;
    owner lock_owners[NB_OWNERS];
    pthread_mutex_t lock_mutex; 
    pthread_cond_t lock_condition; 
} rl_lock;

typedef struct {
    int first;
    rl_lock lock_table[NB_LOCKS];
    pthread_mutex_t file_mutex; 
    char pathname[PATH_MAX]; // On ajoute ce champs pour le 6.7 et pouvoir supprimer le shm quand tous les descripteurs associés à celui ci sont fermés
    int nbtimes_opened;
} rl_open_file;

typedef struct{
    int d;
    rl_open_file *f;
} rl_descriptor;

//struct flock{
    //short rl_type; /* F_RDLCK F_WRLCK F_UNLCK */
    //short rl_whence; /* SEEK_SET SEEK_CUR SEEK_END */
    //off_t rl_start; /*offset où le verrou commence*/
    //off_t len; /* la longueur de segment*/
   // pid_t pid; /* non utilisé dans le projet */
 //};
rl_descriptor rl_open(const char *pathname, int oflag, ...);

int rl_close(rl_descriptor fd);

int rl_fcntl(rl_descriptor lfd, int cmd, struct flock *lck);

//int rl_fcntl(rl_descriptor lfd, int cmd, struct rl_flock *lck);

rl_descriptor rl_dup(rl_descriptor lfd);
rl_descriptor rl_dup2(rl_descriptor lfd, int newd);

pid_t rl_fork();

int rl_init_library();

#endif
