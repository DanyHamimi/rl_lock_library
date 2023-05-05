#include "rl_lock_library.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


static struct {
    int nb_files;
    rl_open_file *tab_open_files[NB_FILES];
} rl_all_files;



char *get_shm_name(const char *pathname, const char *prefix) {
    struct stat st;
    if (stat(pathname, &st) == -1) {
        return NULL;
    }
    char *shm_name = NULL;
    if (asprintf(&shm_name, "/%s_%u_%llu", prefix, st.st_dev, (unsigned long long) st.st_ino) == -1) {
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
        pthread_mutexattr_t mutex_attr;
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&descriptor.f->file_mutex, &mutex_attr);
        printf("creation finie\n");
    }
    else if (shm_fd == -1) {
        if (errno == EEXIST) {
            shm_fd = shm_open(shm_name, O_RDWR, S_IRUSR | S_IWUSR);
            if (shm_fd == -1) {
                descriptor.f = NULL;
                return descriptor;
            }
            descriptor.f = mmap(NULL, sizeof(rl_open_file), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        } else {
            descriptor.f = NULL;
            return descriptor;
        }

        printf("ouverture finie\n");
    }
    rl_all_files.tab_open_files[rl_all_files.nb_files] = descriptor.f;
    rl_all_files.nb_files++;
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
            }
        }
    }
    
    return 0;
}

