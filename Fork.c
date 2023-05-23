#include "rl_lock_library.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

//Cas Exclusive
int main() {
    // Initialisation de la bibliothèque
    rl_init_library();

    // Ouverture d'un fichier avec rl_open
    rl_descriptor descriptor = rl_open("oui", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (descriptor.d == -1) {
        perror("Erreur lors de l'ouverture du fichier");
        return 1;
    }

    printf("Fichier ouvert avec succès\n");

    // Verrouillage du fichier avec rl_fcntl
    struct flock lock;
    lock.l_type = F_RDLCK;  // Verrou d'écriture
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;  // Verrouille tout le fichier   10-30
    if (rl_fcntl(descriptor, F_SETLK, &lock) == -1) {
        printf("Vérouillage impossible\n");
        rl_close(descriptor);
        return 1;
    }
    pid_t a = rl_fork();
    if(a == 0){
        printf("Fils\n");
    }
    else{
        printf("Pere\n");
    }
    

    

    
}


