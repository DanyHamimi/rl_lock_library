// HAMIMI Dany 21952735
// KAABECHE Rayane 21955498

#define _GNU_SOURCE

#include "rl_lock_library.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>



static struct {
    int nb_files;
    rl_open_file *tab_open_files[NB_FILES];
} rl_all_files;


int rl_init_library() {
    rl_all_files.nb_files = 0;
    for (int i = 0; i < NB_FILES; i++) {
        rl_all_files.tab_open_files[i] = NULL;
    }
    return 0;
}

char *get_shm_name(const char *pathname, const char *prefix) {
    struct stat st;
    if (stat(pathname, &st) == -1) {
        return NULL;
    }
    char *shm_name = NULL;
    if (asprintf(&shm_name, "/%s_%lu_%llu", prefix, st.st_dev, (unsigned long long) st.st_ino) == -1) {
        return NULL;
    }
    printf("shm_name: %s\n", shm_name);
    return shm_name;
}



rl_descriptor rl_open(const char *pathname, int oflag, ...) {
    rl_descriptor descriptor;
    int fd = open(pathname, oflag);
    if (fd == -1) {
        descriptor.d = -1;
        descriptor.f = NULL;
        return descriptor;
    }
    descriptor.d = fd;

    char *shm_name = get_shm_name(pathname, "f");
    if (shm_name == NULL) {
        descriptor.f = NULL;
        return descriptor;
    }
    int shm_fd = shm_open(shm_name,  O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);
    if(shm_fd > 0){
        if(ftruncate(shm_fd, sizeof(rl_open_file)) == -1){
            descriptor.f = NULL;
            return descriptor;
        }
        descriptor.f = mmap(NULL, sizeof(rl_open_file), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        for(int i = 0; i < NB_LOCKS; i++){
            descriptor.f->lock_table[i].next_lock = -2;
        }

        descriptor.f->first = -2;


        pthread_mutexattr_t mutex_attr;
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);

        // Bloquer le mutex avant de modifier la mémoire partagée.
        pthread_mutex_lock(&descriptor.f->file_mutex);

        strncpy(descriptor.f->pathname, pathname, PATH_MAX - 1);
        descriptor.f->pathname[PATH_MAX - 1] = '\0';

        // Déverrouiller le mutex après la modification de la mémoire partagée.
        pthread_mutex_unlock(&descriptor.f->file_mutex);

        pthread_mutex_init(&descriptor.f->file_mutex, &mutex_attr);

    }
    else if (shm_fd == -1) {
        if (errno == EEXIST) {
            printf("shm existe donc :\n");
            shm_fd = shm_open(shm_name, O_RDWR, S_IRUSR | S_IWUSR);
            if (shm_fd == -1) {
                descriptor.f = NULL;
                return descriptor;
            }
            descriptor.f = mmap(NULL, sizeof(rl_open_file), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

            strncpy(descriptor.f->pathname, pathname, PATH_MAX - 1);
            descriptor.f->pathname[PATH_MAX - 1] = '\0';
            printf("on return le fichier sans mettre à jour le nombre de fichiers ouverts\n");
            for (int i = 0; i < rl_all_files.nb_files; i++) {
                printf("1- %s\n", rl_all_files.tab_open_files[i]->pathname);
                printf("2- %s\n", pathname);
                if (strcmp(rl_all_files.tab_open_files[i]->pathname, pathname) == 0) {
                    pthread_mutex_lock(&descriptor.f->file_mutex);
                    descriptor.f->nbtimes_opened++;
                    pthread_mutex_unlock(&descriptor.f->file_mutex);
                    printf("le fichier existait déjà et il est maintenant utilisé par %d processus\n", descriptor.f->nbtimes_opened);
                    if (shm_fd != -1) {
                        munmap(shm_name, sizeof(rl_open_file));
                        close(shm_fd);
                    }
                    return descriptor;
                }
            }

        } else {
            descriptor.f = NULL;
            return descriptor;
        }

    }
    rl_all_files.tab_open_files[rl_all_files.nb_files] = descriptor.f;
    pthread_mutex_lock(&descriptor.f->file_mutex);
    descriptor.f->nbtimes_opened++;
    pthread_mutex_unlock(&descriptor.f->file_mutex);
    rl_all_files.nb_files++;
    printf("le fichier n'existait pas donc il y a mtn %d fichiers ouverts\n", rl_all_files.nb_files);
    munmap(shm_name, sizeof(rl_open_file));
    close(shm_fd);
    return descriptor;
}


int rl_close(rl_descriptor lfd) {
    int ret = close(lfd.d);
    if (ret == -1) {
        return ret;
    }
    rl_open_file *open_file = lfd.f;
    owner lfd_owner = { .proc = getpid(), .des = lfd.d };
    for (int i = 0; i < NB_LOCKS; i++) {
        rl_lock *lock = &(open_file->lock_table[i]);
        if (lock->nb_owners > 0) {
            for (size_t j = 0; j < lock->nb_owners; j++) {
                owner ow = lock->lock_owners[j];
                if (ow.proc == lfd_owner.proc && ow.des == lfd_owner.des) {
                    for (size_t k = j; k < lock->nb_owners - 1; k++) {
                        lock->lock_owners[k] = lock->lock_owners[k + 1];
                    }
                    lock->nb_owners--;
                    if (lock->nb_owners == 0) {
                        pthread_mutex_destroy(&(lock->lock_mutex));
                        pthread_cond_destroy(&(lock->lock_condition));
                        lock->next_lock = -2;
                        lock->starting_offset = 0;
                        lock->len = 0;
                    }
                    break;
                }
                else{
                }
            }
        }
        else{
        }
    }
    //check if file is still open
    for (int i = 0; i < NB_FILES; i++) {
        if (rl_all_files.tab_open_files[i] != NULL) {
            if (strcmp(rl_all_files.tab_open_files[i]->pathname, open_file->pathname) == 0) {
                rl_all_files.tab_open_files[i]->nbtimes_opened--;
                //printf("le fichier a maintenant %d ouvertures\n", rl_all_files.tab_open_files[i]->nbtimes_opened);
                if (rl_all_files.tab_open_files[i]->nbtimes_opened == 0) {
                    rl_all_files.tab_open_files[i] = NULL;
                    rl_all_files.nb_files--;
                    //printf("fichier fermé il y a %d fichiers ouverts\n", rl_all_files.nb_files);
                    break;
                }
            }
        }
    }

    if (lfd.f->nbtimes_opened == 0) {
        char *shm_name = get_shm_name(lfd.f->pathname, "f");
        if (shm_name != NULL) {
            shm_unlink(shm_name);
            free(shm_name);
            printf("shm supprimé\n");
        }
    }

    /*printf("il y a %d fichiers ouverts\n", rl_all_files.nb_files);
    printf("le fichier a maintenant %d ouvertures\n", lfd.f->nbtimes_opened);*/
    return 0;
}

int is_process_alive(pid_t pid) {
    if (kill(pid, 0) == 0) {
        return 1;
    }
    return 0;
}

int printAllVerrousOccup(){
    for (int i = 0; i < NB_FILES; i++) {
        if (rl_all_files.tab_open_files[i] != NULL) {
            printf("fichier %s\n", rl_all_files.tab_open_files[i]->pathname);
            for (int j = 0; j < NB_LOCKS; j++) {
                rl_lock *lock = &(rl_all_files.tab_open_files[i]->lock_table[j]);
                if (lock->nb_owners > 0) {
                    printf("verrou %d\n", j);
                    for (int k = 0; k < lock->nb_owners; k++) {
                        printf("proc %d des %d\n", lock->lock_owners[k].proc, lock->lock_owners[k].des);
                        //print intervalle du verou
                        printf("de %ld à %ld\n", lock->starting_offset, lock->starting_offset + lock->len);
                    }
                }
            }
        }
    }
    return 0;
}
int checkChevauchement(int w,rl_open_file filedescr, struct flock *lck, owner lfd_owner) {
    int isOwner = 0;
    rl_lock *lock = &(filedescr.lock_table[w]);
    
    for (int i = 0; i < lock->nb_owners; i++) {
        if (lock->lock_owners[i].proc == lfd_owner.proc && lock->lock_owners[i].des == lfd_owner.des) {
            isOwner = 1;
            break;
        }
    }
    int len = lck->l_len;
    if(lck->l_len == 0){
        len = 2<<25;
    }
    off_t lock_end = lock->starting_offset + lock->len;
    off_t lck_end = lck->l_start + len;
    printf("lock_end %ld lck_end %ld\n", lock_end, lck_end);

    // Check for overlapping
    if (lock_end < lck->l_start || lck_end < lock->starting_offset) {
        // No overlap
        return 0;
    } else if (isOwner) {
        // If the owner is trying to set a lock, it can merge with the existing one.
        if (lck->l_type == F_RDLCK && lock->type == F_WRLCK) {
            // If the owner is trying to downgrade the lock from write to read, allow it
            return 1;
        } else if (lck->l_type == F_WRLCK && lock->type == F_RDLCK) {
            // If the owner is trying to upgrade the lock from read to write, check if there are other readers
            if (lock->nb_owners == 1) {
                // If the owner is the only reader, allow the upgrade
                return 1;
            } else {
                // If there are other readers, deny the upgrade
                return -1;
            }
        } else {
            // If the lock type is the same, allow it (merge)
            return 1;
        }
    } else {
        // If there is an overlap and the owner is different
        if (lock->type == F_RDLCK && lck->l_type == F_RDLCK) {
            // If both locks are read locks, allow it
            return 0;
        } else {
            // If any of the locks is a write lock, deny it
            return -1;
        }
    }
}



int rl_fcntl(rl_descriptor lfd, int cmd, struct flock *lck) {
    printAllVerrousOccup();
    owner lfd_owner = {.proc = getpid(), .des = lfd.d};
    
    if (cmd == F_SETLK) {
            for (int i = 0; i < NB_LOCKS; i++) {
                rl_lock *lock = &(lfd.f->lock_table[i]);
                if (lock->nb_owners > 0) {
                    for (size_t j = 0; j < lock->nb_owners; j++) {
                        owner ow = lock->lock_owners[j];
                        if (!is_process_alive(ow.proc)) {
                            for (size_t k = j; k < lock->nb_owners - 1; k++) {
                                lock->lock_owners[k] = lock->lock_owners[k + 1];
                            }
                            lock->nb_owners--;
                            printf("processus mort supprimé\n");
                            if (lock->nb_owners == 0) {
                                if(i == lfd.f->first){
                                    lfd.f->first = lock->next_lock;
                                    if(lfd.f->first == -1){
                                        lfd.f->first = -2;
                                    }
                                }
                                for(int k = 0; k < NB_LOCKS; k++){
                                    if(lfd.f->lock_table[k].next_lock == i){
                                        lfd.f->lock_table[k].next_lock = lock->next_lock;
                                    }
                                }
                                pthread_mutex_destroy(&(lock->lock_mutex));
                                pthread_cond_destroy(&(lock->lock_condition));
                                lock->next_lock = -2;
                                lock->starting_offset = 0;
                                lock->len = 0;
                            }
                            break;
                        }
                    }
                }
            }
        //check if first == -2
        printf("f-first = %d\n", lfd.f->first);
        if(lfd.f->first == -2){
            //check if its unlock return 0
            if(lck->l_type == F_UNLCK){
                printf("unlock posé\n");
                return 0;
            }
            else if (lck->l_type == F_RDLCK || lck->l_type == F_WRLCK){
                lfd.f->first = 0;
                rl_lock new_lock = {.lock_owners = {lfd_owner}, .nb_owners = 1, .type = lck->l_type, .starting_offset = lck->l_start, .len = lck->l_len, .next_lock = -1};
                lfd.f->lock_table[0] = new_lock;
                lfd.f->lock_table[0].lock_owners[0] = lfd_owner;
                lfd.f->lock_table[0].nb_owners = 1;
                pthread_mutex_init(&(lfd.f->lock_table[0].lock_mutex), NULL);
                pthread_cond_init(&(lfd.f->lock_table[0].lock_condition), NULL);


                printf("lock posé\n");
                return 0;
            }
            else{
                // Commande non supportée
                errno = EINVAL;
                return -1;
            }
        }
        else{
            //sort all process by checking if they are alive and if they are not, remove them

            //check if the lock is already taken
            int isTaken = 0;
            for(int i = 0;i < NB_LOCKS;i++){
                rl_lock *lock = &(lfd.f->lock_table[i]);
                if(lock->nb_owners > 0){
                    isTaken = checkChevauchement(i,*lfd.f, lck, lfd_owner);
                    printf("isTaken = %d\n", isTaken);
                    if(isTaken == -1){
                        printf("lock déjà pris\n");
                        return -1;
                    }
                    if(isTaken == 1){
                        printf("lock déjà pris mais peut être mergé\n");
                        break;
                    }
                }
            }
            printf("isTakenPostBoucle = %d\n", isTaken);
            if(lck->l_type == F_UNLCK){
                // ici on va débloquer le verrou
                for(int i = 0; i < NB_LOCKS; i++){
                    rl_lock *lock = &(lfd.f->lock_table[i]);
                    if(lock->nb_owners > 0){
                        off_t lock_end = lock->starting_offset + lock->len;
                        off_t lck_end = lck->l_start + lck->l_len;
                        // si le verrou à débloquer est exactement le même que celui existant
                        if(lock->starting_offset == lck->l_start && lock_end == lck_end){
                            for (size_t j = 0; j < lock->nb_owners; j++) {
                                owner ow = lock->lock_owners[j];
                                if (ow.proc == lfd_owner.proc && ow.des == lfd_owner.des) {
                                    // Supprimer le propriétaire du verrou
                                    for (size_t k = j; k < lock->nb_owners - 1; k++) {
                                        lock->lock_owners[k] = lock->lock_owners[k + 1];
                                    }
                                    lock->nb_owners--;
                                    // Si le verrou n'a plus de propriétaires, détruisez-le
                                    if (lock->nb_owners == 0) {
                                        if(i == lfd.f->first){
                                            lfd.f->first = lock->next_lock;
                                            if(lfd.f->first == -1){
                                                lfd.f->first = -2;
                                            }
                                        }
                                        for(int k = 0; k < NB_LOCKS; k++){
                                            if(lfd.f->lock_table[k].next_lock == i){
                                                lfd.f->lock_table[k].next_lock = lock->next_lock;
                                            }
                                        }
                                        pthread_mutex_destroy(&(lock->lock_mutex));
                                        pthread_cond_destroy(&(lock->lock_condition));
                                        lock->next_lock = -2;
                                        lock->starting_offset = 0;
                                        lock->len = 0;
                                    }
                                    printf("Le verrou a été débloqué\n");
                                    return 0;
                                }
                            }
                        }
                        //TODO : si le verrou à débloquer est à l'intérieur d'un verrou existant 
                    }
                }
                printf("Aucun verrou correspondant trouvé pour débloquer\n");
                return 0;
            }
            else if(lck->l_type == F_WRLCK || lck->l_type == F_RDLCK ){
                if(isTaken ==0){
                    //while the next of first is not -1 we go to the next
                    int next = lfd.f->first;
                    int last = -1;
                    while(next != -1){
                        last = next;
                        next = lfd.f->lock_table[next].next_lock;
                    }
                    //we have the last lock
                    //create new lock with lck and add it to the end of the list then set next of last to the new lock
                    rl_lock new_lock = {.lock_owners = {lfd_owner}, .nb_owners = 1, .type = lck->l_type, .starting_offset = lck->l_start, .len = lck->l_len, .next_lock = -1};
                    //find a lock with -2 value and replace it with the new lock
                    for(int i = 0; i < NB_LOCKS; i++){
                        rl_lock *lock = &(lfd.f->lock_table[i]);
                        if(lock->next_lock == -2){
                            lfd.f->lock_table[i] = new_lock;
                            lfd.f->lock_table[i].lock_owners[0] = lfd_owner;
                            lfd.f->lock_table[i].nb_owners = 1;
                            pthread_mutex_init(&(lfd.f->lock_table[i].lock_mutex), NULL);
                            pthread_cond_init(&(lfd.f->lock_table[i].lock_condition), NULL);
                            if(last != -1){
                                lfd.f->lock_table[last].next_lock = i;
                            }
                            else{
                                lfd.f->first = i;
                            }
                            printf("lock posé\n");
                            return 0;
                        }
                    }
                }
                else{
                    //Gerer chevauchements...
                }
                
                }
            }


    } else {
        // Commande non supportée
        errno = EINVAL;
        return -1;
    }
    
    return 0;
}


