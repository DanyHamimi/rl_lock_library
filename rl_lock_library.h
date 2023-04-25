#ifndef RL_LOCK_LIBRARY_H
#define RL_LOCK_LIBRARY_H

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define MAX_LOCKS 1024 // Vous pouvez ajuster cette valeur selon vos besoins

// Structure pour stocker les informations sur les verrous personnalisés
typedef struct {
    pid_t pid;              // PID du processus
    off_t start;            // Position de début du verrou
    off_t end;              // Position de fin du verrou
    int type;               // Type de verrou (F_RDLCK ou F_WRLCK)
} CustomFileLock;

// Prototypes de fonctions pour gérer les verrous dans la mémoire partagée
int add_lock(CustomFileLock* shared_locks, CustomFileLock new_lock);
int remove_lock(CustomFileLock* shared_locks, CustomFileLock lock_to_remove);
int check_lock(CustomFileLock* shared_locks, CustomFileLock lock_to_check);

// Prototypes de fonctions pour verrouiller et déverrouiller un fichier en utilisant les verrous personnalisés
int custom_lock(int fd, int lock_type, off_t start, off_t len);
int custom_unlock(int fd, off_t start, off_t len);

#endif // RL_LOCK_LIBRARY_H
