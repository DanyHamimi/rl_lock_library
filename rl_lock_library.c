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
    //printf("shm_name: %s\n", shm_name);
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
            //printf("shm existe donc :\n");
            shm_fd = shm_open(shm_name, O_RDWR, S_IRUSR | S_IWUSR);
            if (shm_fd == -1) {
                descriptor.f = NULL;
                return descriptor;
            }
            descriptor.f = mmap(NULL, sizeof(rl_open_file), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

            strncpy(descriptor.f->pathname, pathname, PATH_MAX - 1);
            descriptor.f->pathname[PATH_MAX - 1] = '\0';
            //printf("on return le fichier sans mettre à jour le nombre de fichiers ouverts\n");
            for (int i = 0; i < rl_all_files.nb_files; i++) {
                if (strcmp(rl_all_files.tab_open_files[i]->pathname, pathname) == 0) {
                    pthread_mutex_lock(&descriptor.f->file_mutex);
                    descriptor.f->nbtimes_opened++;
                    pthread_mutex_unlock(&descriptor.f->file_mutex);
                    //printf("le fichier existait déjà et il est maintenant utilisé par %d processus\n", descriptor.f->nbtimes_opened);
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
    //printf("le fichier n'existait pas donc il y a mtn %d fichiers ouverts\n", rl_all_files.nb_files);
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
            ////printf("shm supprimé\n");
        }
    }

    /*//printf("il y a %d fichiers ouverts\n", rl_all_files.nb_files);
    //printf("le fichier a maintenant %d ouvertures\n", lfd.f->nbtimes_opened);*/
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

int addOwnerToLock(int w, owner lfd_owner, rl_open_file* lfd){
    rl_lock *lock = &(lfd->lock_table[w]);
    printf("nb owners %ld\n", lock->nb_owners);
    for(int i = 0; i < NB_OWNERS; i++){
        printf("proc %d des %d\n", lock->lock_owners[i].proc, lock->lock_owners[i].des);
        if(lock->lock_owners[i].proc == 0 && lock->lock_owners[i].des == 0){
            lock->lock_owners[i] = lfd_owner;
            lock->nb_owners++;
            printf("Le propriétaire a été ajouté\n");
            //print all propriétaires du verrou
            for(int j = 0; j < lock->nb_owners; j++){
                printf("proc %d des %d\n", lock->lock_owners[j].proc, lock->lock_owners[j].des);
            }
            return 0;
            printf("nb owners %ld\n", lock->nb_owners);

        }
    }
    return -1;
}

int checkChevauchement(int w,rl_open_file* filedescr, struct flock *lck, owner lfd_owner) {
    int isOwner = 0;
    rl_lock *lock = &(filedescr->lock_table[w]);
    
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
    //printf("lock_end %ld lck_end %ld\n", lock_end, lck_end);

    // Check for overlapping
    if (lock_end < lck->l_start || lck_end < lock->starting_offset) {
        // No overlap
        return 0;
    } else if (isOwner) {
        // If the owner is trying to set a lock, it can merge with the existing one.
        if (lck->l_type == F_RDLCK && lock->type == F_WRLCK) {
            // If the owner is trying to downgrade the lock from write to read, allow it
            return 5;
        } else if (lck->l_type == F_WRLCK && lock->type == F_RDLCK) {
            // If the owner is trying to upgrade the lock from read to write, check if there are other readers
            if (lock->nb_owners == 1) {
                printf("owner trying to upgrade from read to write\n");
                // If the owner is the only reader, allow the upgrade
                return 6;
            } else {
                // If there are other readers, deny the upgrade
                printf("Other readers so upgrade impossible\n");
                return -1;
            }
        } else {
            // If the lock type is the same, accepter en précisant s'il est "dedans", "prefixed" ou "suffixed" par rapport à l'autre ou bien s'il est englobe l'autre while printing the case
            if (lock->starting_offset == lck->l_start && lock_end == lck_end) {
                // If the locks are exactly the same, allow it 
                printf("same lock\n");
                return -1;
            }
            else if (lock->starting_offset <= lck->l_start && lock_end >= lck_end) {
                // If the existing lock contains the new one, allow it
                printf("existing lock contains new one\n");
                return 7;
            }
            else if (lock->starting_offset >= lck->l_start && lock_end <= lck_end) {
                // If the new lock contains the existing one, allow it
                printf("new lock contains existing one\n");
                return 8;
            }
            else if (lock->starting_offset <= lck->l_start && lock_end <= lck_end) {
                // If the existing lock is a prefix of the new one, allow it
                printf("existing lock is a prefix of the new one\n");
                return 9;
            }
            else if (lock->starting_offset >= lck->l_start && lock_end >= lck_end) {
                // If the existing lock is a suffix of the new one, allow it
                printf("existing lock is a suffix of the new one\n\n");
                return 10;
            }
            else {
                // If the locks are overlapping but none of the above cases, deny it
                printf("overlapping but not same\n");
                return -1;
            }
        }
    } else {
        // If there is an overlap and the owner is different
        if (lock->type == F_RDLCK && lck->l_type == F_RDLCK) {
            //if locks are the same position and length and both read locks then only add the owner to the lock and return 4
            if (lock->starting_offset == lck->l_start && lock_end == lck_end) {
                if(addOwnerToLock(w, lfd_owner, filedescr) < 0){
                    printf("Impossible le verrou est plein\n");
                    return -1;
                }
                return 4;
            }
            // If both locks are read locks, allow it
            return 0;
        } else {
            // If any of the locks is a write lock, deny it
            return -1;
        }
    }
}

void remove_dead_owners(rl_lock *lock, rl_descriptor *lfd, int i) {
    // Supprime les propriétaires de verrous morts
    for (size_t j = 0; j < lock->nb_owners; j++) {
        owner ow = lock->lock_owners[j];
        if (!is_process_alive(ow.proc)) {
            printf("On va supprimer proc %d des %d\n", lock->lock_owners[j].proc, lock->lock_owners[j].des);
                lock->lock_owners[j].proc = 0;
                lock->lock_owners[j].des = 0;
            lock->nb_owners--;
            printf("Processus mort supprimé\n");
            if (lock->nb_owners == 0) {
                // Nettoie le verrou s'il n'a plus de propriétaires
                if (i == lfd->f->first) {
                    lfd->f->first = lock->next_lock;
                    if (lfd->f->first == -1) {
                        lfd->f->first = -2;
                    }
                }
                for (int k = 0; k < NB_LOCKS; k++) {
                    if (lfd->f->lock_table[k].next_lock == i) {
                        lfd->f->lock_table[k].next_lock = lock->next_lock;
                    }
                }
                pthread_mutex_destroy(&(lock->lock_mutex));
                pthread_cond_destroy(&(lock->lock_condition));
                lock->next_lock = -2;
                lock->starting_offset = 0;
                lock->len = 0;
            }
        }
    }
}

int removeOwnerFromLock(rl_lock *lock, owner lfd_owner,rl_descriptor* lfd, int i){
    for (size_t j = 0; j < lock->nb_owners; j++) {
        owner ow = lock->lock_owners[j];
        if (ow.proc == lfd_owner.proc && ow.des == lfd_owner.des) {
            for (size_t k = j; k < lock->nb_owners - 1; k++) {
                //set values at 0 for the owner to be removed
                lock->lock_owners[k].proc = 0;
                lock->lock_owners[k].des = 0;
                lock->lock_owners[k] = lock->lock_owners[k + 1];
            }
            lock->nb_owners--;
            if (lock->nb_owners == 0) {
                for(int k = 0; k < NB_LOCKS; k++){
                    if(lfd->f->lock_table[k].next_lock == i){
                        lfd->f->lock_table[k].next_lock = lock->next_lock;
                    }
                }
                pthread_mutex_destroy(&(lock->lock_mutex));
                pthread_cond_destroy(&(lock->lock_condition));
                lock->next_lock = -2;
                lock->starting_offset = 0;
                lock->len = 0;
            }
            return 0;
        }
    }
    return -1;
}

int addNewLock(rl_descriptor* lfd, struct flock *lck, owner lfd_owner){
    //parcourir les verrous du fichier via next_lock et ajouter le verrou à la fin
    int index = lfd->f->first;
    int old_index = index;
    while(index != -1){
        old_index = index;
        index = lfd->f->lock_table[index].next_lock;
    }
    int valNewLock;
    //find the first empty lock
    for(int i = 0;i < NB_LOCKS; i++){
        if(lfd->f->lock_table[i].next_lock == -2){
            lfd->f->lock_table[i].next_lock = -1;
            lfd->f->lock_table[i].starting_offset = lck->l_start;
            lfd->f->lock_table[i].len = lck->l_len;
            lfd->f->lock_table[i].type = lck->l_type;
            lfd->f->lock_table[i].nb_owners = 1;
            lfd->f->lock_table[i].lock_owners[0] = lfd_owner;
            pthread_mutex_init(&(lfd->f->lock_table[i].lock_mutex), NULL);
            pthread_cond_init(&(lfd->f->lock_table[i].lock_condition), NULL);
            printf("Le verrou a été ajouté\n");
            valNewLock = i;
            break;
        }
    }
    printf("Old index : %d\n", old_index);
    lfd->f->lock_table[old_index].next_lock = valNewLock;
    //increment nb_owners of the lock
    /*for(int i = 0; i < NB_LOCKS; i++){
        printf("lock %d : %d\n", i, lfd.f->lock_table[i].next_lock);
    }*/
    return 0;
    
}



int deletelocknowners(rl_descriptor* lfd,int i){
    rl_lock *lock = &(lfd->f->lock_table[i]);
    if (lock->nb_owners == 0) {
        if(i == lfd->f->first){
            lfd->f->first = lock->next_lock;
        if(lfd->f->first == -1){
            lfd->f->first = -2;
        }
        }
        for(int k = 0; k < NB_LOCKS; k++){
            if(lfd->f->lock_table[k].next_lock == i){
                lfd->f->lock_table[k].next_lock = lock->next_lock;
            }
        }
        pthread_mutex_destroy(&(lock->lock_mutex));
        pthread_cond_destroy(&(lock->lock_condition));
        lock->next_lock = -2;
        lock->starting_offset = 0;
        lock->len = 0;
    }

    return 0;
    
}


int unlock(rl_descriptor lfd, struct flock *lck, owner lfd_owner){
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
                        deletelocknowners(&lfd,i);

                        printf("Le verrou a été débloqué\n");
                        return 0;
                    }
                }
            }
            //si la longueur 
            if(checkChevauchement(i,lfd.f, lck, lfd_owner) == 1){
                //check if the lock has more than one owner si oui on l'enleve juste de la liste des propriétaires et on crée des sous verrous à la fin sans supprimer le verrou existant qui a + d'un propriétaire
                if(lock->nb_owners > 1){
                    //si le verrou à + d'un owner faire les mêmes choses qu'en bas mais sans supprimer le verrou existant qui a + d'un propriétaire et en créant des sous verrous à la fin du verrou existant
                    printf("plusieurs propriétaires\n");
                    //si le verrou à débloquer est un préfixe du verrou existant (créer un nouveau verrou qu'on ajoute à la fin et enlever l'ownership du verrou existant sans le supprimer)
                    if(lock->starting_offset == lck->l_start && lock_end > lck_end){
                        printf("prefixe\n");
                        //on crée un nouveau verrou avec le reste du verrou existant
                        rl_lock new_lock = {.lock_owners = {lfd_owner}, .nb_owners = 1, .type = lock->type, .starting_offset = lck_end, .len = lock_end - lck_end, .next_lock = -1};
                        //on enleve l'ownership du verrou existant
                        removeOwnerFromLock(lock, lfd_owner, &lfd, i);
                        //on ajoute le nouveau verrou à la fin
                        for(int j = 0; j < NB_LOCKS; j++){
                            rl_lock *lock2 = &(lfd.f->lock_table[j]);
                            if(lock2->next_lock == -2){
                                lfd.f->lock_table[j] = new_lock;
                                lfd.f->lock_table[j].lock_owners[0] = lfd_owner;
                                lfd.f->lock_table[j].nb_owners = 1;
                                pthread_mutex_init(&(lfd.f->lock_table[j].lock_mutex), NULL);
                                pthread_cond_init(&(lfd.f->lock_table[j].lock_condition), NULL);
                                lfd.f->lock_table[i].next_lock = j;
                                return 0;
                            }
                        }
                    }
                    //si le verrou à débloquer est un suffixe du verrou existant (créer un nouveau verrou qu'on ajoute à la fin et enlever l'ownership du verrou existant sans le supprimer)
                    else if(lock->starting_offset < lck->l_start && lock_end == lck_end){
                        printf("suffixe (double)\n");
                        //on crée un nouveau verrou avec le reste du verrou existant
                        rl_lock new_lock = {.lock_owners = {lfd_owner}, .nb_owners = 1, .type = lock->type, .starting_offset = lock->starting_offset, .len = lck->l_start - lock->starting_offset, .next_lock = -1};
                        //on enleve l'ownership du verrou existant
                        removeOwnerFromLock(lock, lfd_owner, &lfd, i);
                        //on ajoute le nouveau verrou à la fin
                        for(int j = 0; j < NB_LOCKS; j++){
                            rl_lock *lock2 = &(lfd.f->lock_table[j]);
                            if(lock2->next_lock == -2){
                                lfd.f->lock_table[j] = new_lock;
                                lfd.f->lock_table[j].lock_owners[0] = lfd_owner;
                                lfd.f->lock_table[j].nb_owners = 1;
                                pthread_mutex_init(&(lfd.f->lock_table[j].lock_mutex), NULL);
                                pthread_cond_init(&(lfd.f->lock_table[j].lock_condition), NULL);
                                lfd.f->lock_table[i].next_lock = j;
                                return 0;
                            }
                        }
                    }
                    //si le verrou à débloquer est au milieu du verrou existant (créer deux nouveaux verrous qu'on ajoute à la fin et enlever l'ownership du verrou existant sans le supprimer)
                    else if(lock->starting_offset < lck->l_start && lock_end > lck_end){
                        printf("au milieu(double)\n");
                        //on crée un nouveau verrou avec le début du verrou existant
                        rl_lock new_lock = {.lock_owners = {lfd_owner}, .nb_owners = 1, .type = lock->type, .starting_offset = lock->starting_offset, .len = lck->l_start - lock->starting_offset, .next_lock = -1};
                        //on enleve l'ownership du verrou existant
                        removeOwnerFromLock(lock, lfd_owner, &lfd, i);
                        //on ajoute le nouveau verrou à la fin
                        for(int j = 0; j < NB_LOCKS; j++){
                            rl_lock *lock2 = &(lfd.f->lock_table[j]);
                            if(lock2->next_lock == -2){
                                lfd.f->lock_table[j] = new_lock;
                                lfd.f->lock_table[j].lock_owners[0] = lfd_owner;
                                lfd.f->lock_table[j].nb_owners = 1;
                                pthread_mutex_init(&(lfd.f->lock_table[j].lock_mutex), NULL);
                                pthread_cond_init(&(lfd.f->lock_table[j].lock_condition), NULL);
                                lfd.f->lock_table[i].next_lock = j;
                                return 0;
                            }
                        }
                        //on crée un nouveau verrou avec la fin du verrou existant
                        rl_lock new_lock2 = {.lock_owners = {lfd_owner}, .nb_owners = 1, .type = lock->type, .starting_offset = lck_end, .len = lock_end - lck_end, .next_lock = -1};
                        //on ajoute le nouveau verrou à la fin
                        for(int j = 0; j < NB_LOCKS; j++){
                            rl_lock *lock2 = &(lfd.f->lock_table[j]);
                            if(lock2->next_lock == -2){
                                lfd.f->lock_table[j] = new_lock2;
                                lfd.f->lock_table[j].lock_owners[0] = lfd_owner;
                                lfd.f->lock_table[j].nb_owners = 1;
                                pthread_mutex_init(&(lfd.f->lock_table[j].lock_mutex), NULL);
                                pthread_cond_init(&(lfd.f->lock_table[j].lock_condition), NULL);
                                lfd.f->lock_table[i].next_lock = j;
                                return 0;
                            }
                        }
                    }

                }
                //si le verrou à débloquer est un préfixe du verrou existant
                if(lock->starting_offset == lck->l_start && lock_end > lck_end){
                    printf("prefixe\n");
                    //on crée un nouveau verrou avec le reste du verrou existant
                    rl_lock new_lock = {.lock_owners = {lfd_owner}, .nb_owners = 1, .type = lock->type, .starting_offset = lck_end, .len = lock_end - lck_end, .next_lock = -1};
                    //on remplace le verrou existant par le nouveau
                    lfd.f->lock_table[i] = new_lock;
                    lfd.f->lock_table[i].lock_owners[0] = lfd_owner;
                    lfd.f->lock_table[i].nb_owners = 1;
                    pthread_mutex_init(&(lfd.f->lock_table[i].lock_mutex), NULL);
                    pthread_cond_init(&(lfd.f->lock_table[i].lock_condition), NULL);
                    printf("Le verrou a été débloqué dans l'interval : %ld - %ld\n", lck->l_start, lck_end);
                    printf("Le verrou existant maintenant est dans l'intervalle : %ld - %ld\n", lfd.f->lock_table[i].starting_offset, lfd.f->lock_table[i].starting_offset + lfd.f->lock_table[i].len);
                    return 0;
                }
                //si le verrou à débloquer est un suffixe du verrou existant
                else if(lock->starting_offset < lck->l_start && lock_end == lck_end){
                    printf("suffixe\n");
                    //on crée un nouveau verrou avec le reste du verrou existant
                    rl_lock new_lock = {.lock_owners = {lfd_owner}, .nb_owners = 1, .type = lock->type, .starting_offset = lock->starting_offset, .len = lck->l_start - lock->starting_offset, .next_lock = -1};
                    //on remplace le verrou existant par le nouveau
                    lfd.f->lock_table[i] = new_lock;
                    lfd.f->lock_table[i].lock_owners[0] = lfd_owner;
                    lfd.f->lock_table[i].nb_owners = 1;
                    pthread_mutex_init(&(lfd.f->lock_table[i].lock_mutex), NULL);
                    pthread_cond_init(&(lfd.f->lock_table[i].lock_condition), NULL);
                    printf("Le verrou a été débloqué\n");
                    return 0;
                }
                //si le verrou à débloquer est au milieu du verrou existant
                else if(lock->starting_offset < lck->l_start && lock_end > lck_end){
                    printf("au milieu\n");
                    //on crée un nouveau verrou avec le début du verrou existant
                    rl_lock new_lock = {.lock_owners = {lfd_owner}, .nb_owners = 1, .type = lock->type, .starting_offset = lock->starting_offset, .len = lck->l_start - lock->starting_offset, .next_lock = -1};
                    //on remplace le verrou existant par le nouveau
                    lfd.f->lock_table[i] = new_lock;
                    lfd.f->lock_table[i].lock_owners[0] = lfd_owner;
                    lfd.f->lock_table[i].nb_owners = 1;
                    pthread_mutex_init(&(lfd.f->lock_table[i].lock_mutex), NULL);
                    pthread_cond_init(&(lfd.f->lock_table[i].lock_condition), NULL);
                    //on crée un nouveau verrou avec la fin du verrou existant
                    rl_lock new_lock2 = {.lock_owners = {lfd_owner}, .nb_owners = 1, .type = lock->type, .starting_offset = lck_end, .len = lock_end - lck_end, .next_lock = -1};
                    //on remplace le verrou existant par le nouveau
                    for(int j = 0; j < NB_LOCKS; j++){
                        rl_lock *lock2 = &(lfd.f->lock_table[j]);
                        if(lock2->next_lock == -2){
                            lfd.f->lock_table[j] = new_lock2;
                            lfd.f->lock_table[j].lock_owners[0] = lfd_owner;
                            lfd.f->lock_table[j].nb_owners = 1;
                            pthread_mutex_init(&(lfd.f->lock_table[j].lock_mutex), NULL);
                            pthread_cond_init(&(lfd.f->lock_table[j].lock_condition), NULL);
                            lfd.f->lock_table[i].next_lock = j;
                            break;
                        }
                    }
                    printf("Les nouveaux verrous sont maintenant : %ld - %ld et %ld - %ld\n", lfd.f->lock_table[i].starting_offset, lfd.f->lock_table[i].starting_offset + lfd.f->lock_table[i].len, lfd.f->lock_table[lfd.f->lock_table[i].next_lock].starting_offset, lfd.f->lock_table[lfd.f->lock_table[i].next_lock].starting_offset + lfd.f->lock_table[lfd.f->lock_table[i].next_lock].len);
                    printf("Le verrou a été débloqué\n");
                    return 0;
                }

                //
            }
        }
    }
    return -1;
}

int rl_fcntl(rl_descriptor lfd, int cmd, struct flock *lck) {
    printAllVerrousOccup();
    owner lfd_owner = {.proc = getpid(), .des = lfd.d};
    
    if (cmd == F_SETLK) {
            for (int i = 0; i < NB_LOCKS; i++) {
                rl_lock *lock = &(lfd.f->lock_table[i]);
                if (lock->nb_owners > 0) {
                    remove_dead_owners(lock, &lfd, i);
                }
            }
        //check if first == -2
        //printf("f-first = %d\n", lfd.f->first);
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
                printf("1er lock posé\n");
                return 0;
            }
            else{
                // Commande non supportée
                errno = EINVAL;
                return -1;
            }
        }
        else{
            //printf("isTakenPostBoucle = %d\n", isTaken);
            if(lck->l_type == F_UNLCK){
                
                unlock(lfd, lck, lfd_owner);
                //print all lock and their owners pid
                for(int i = 0; i < NB_LOCKS; i++){
                    rl_lock *lock = &(lfd.f->lock_table[i]);
                    if(lock->nb_owners > 0){
                        printf("DANY %d\n", i);
                        for(int j = 0; j < lock->nb_owners; j++){
                            printf("proc %d des %d\n", lock->lock_owners[j].proc, lock->lock_owners[j].des);
                        }
                    }
                    
                }

            }
            else if(lck->l_type == F_WRLCK || lck->l_type == F_RDLCK ){
                            //check if the lock is already taken
                int isTaken = 0;
                for(int i = 0;i < NB_LOCKS;i++){
                    rl_lock *lock = &(lfd.f->lock_table[i]);
                    if(lock->nb_owners > 0){
                        isTaken = checkChevauchement(i,lfd.f, lck, lfd_owner);
                        //printf("isTaken = %d\n", isTaken);
                        if(isTaken == -1){
                            printf("lock déjà pris par un autre au même endroit\n");
                            return -1;
                        }
                        if(isTaken == 1){
                            printf("lock déjà pris par le même proprio donc à voir dans un intervalle commun\n");
                            break;
                        }
                    }
                }
                if(isTaken ==0){
                    printf("Aucun souci on pose le lock\n");
                    addNewLock(&lfd, lck, lfd_owner);
                }
                else{
                    if(isTaken > 0 ){ 
                        for(int i = 0;i < NB_LOCKS;i++){
                            rl_lock *lock = &(lfd.f->lock_table[i]);
                            if(lock->nb_owners > 0){
                                int lock_end = lock->starting_offset + lock->len;
                                int lck_end = lck->l_start + lck->l_len;

                                printf("lock compris entre %ld et %d\n", lock->starting_offset, lock_end);
                                printf("lck compris entre %ld et %d\n", lck->l_start, lck_end);
                                if(lock->starting_offset >= lck->l_start && lock_end >= lck_end){
                                    //write case 
                                }
                            }
                        }

                    }
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


