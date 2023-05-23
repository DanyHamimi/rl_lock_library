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
<<<<<<< HEAD
    rl_descriptor descriptor = rl_open("daniel", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
=======
    rl_descriptor descriptor = rl_open("boos", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
>>>>>>> 1a2d309... Started to lock things and wait
    if (descriptor.d == -1) {
        perror("Erreur lors de l'ouverture du fichier");
        return 1;
    }

    printf("Fichier ouvert avec succès\n");

    // Verrouillage du fichier avec rl_fcntl
    struct flock lock;
    lock.l_type = F_WRLCK;  // Verrou d'écriture
    lock.l_whence = SEEK_SET;
<<<<<<< HEAD
    lock.l_start = 0;
    lock.l_len = 0;  // Verrouille tout le fichier   10-30
=======
    lock.l_start = 10;
    lock.l_len = 20;  // Verrouille tout le fichier   10-30
>>>>>>> 1a2d309... Started to lock things and wait
    if (rl_fcntl(descriptor, F_SETLK, &lock) == -1) {
        printf("Vérouillage impossible\n");
        rl_close(descriptor);
        return 1;
    }
<<<<<<< HEAD
        printf("cc\n");

    rl_descriptor descriptor2 = rl_open("azert", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (descriptor2.d == -1) {
        perror("Erreur lors de l'ouverture du fichier");
        return 1;
    }
    if (rl_fcntl(descriptor2, F_SETLK, &lock) == -1) {
        printf("Vérouillage impossible\n");
        rl_close(descriptor);
        return 1;
    }
    rl_close(descriptor2);


    return 0;
    

    

    
}


=======
}
>>>>>>> 1a2d309... Started to lock things and wait
