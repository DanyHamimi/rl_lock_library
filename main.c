#include "rl_lock_library.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main() {
    // Initialisation de la bibliothèque
    rl_init_library();

    // Ouverture d'un fichier avec rl_open
    rl_descriptor descriptor = rl_open("dadada.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (descriptor.d == -1) {
        perror("Erreur lors de l'ouverture du fichier");
        return 1;
    }

    printf("Fichier ouvert avec succès\n");

    // Verrouillage du fichier avec rl_fcntl
    struct flock lock;
    lock.l_type = F_RDLCK;  // Verrou d'écriture
    lock.l_whence = SEEK_SET;
    lock.l_start = 10;
    lock.l_len = 20;  // Verrouille tout le fichier   10-30
    if (rl_fcntl(descriptor, F_SETLK, &lock) == -1) {
        printf("Vérouillage impossible\n");
        rl_close(descriptor);
        return 1;
    }

    lock.l_start = 50;
    lock.l_len = 10 ;
    lock.l_type = F_RDLCK;
    if (rl_fcntl(descriptor, F_SETLK, &lock) == -1) {
        printf("Vérouillage impossible\n");
        rl_close(descriptor);
        return 1;
    }



    close(descriptor.d);

    /*lock.l_start = 50;
    lock.l_len = 20;  
    if (rl_fcntl(descriptor, F_SETLK, &lock) == -1) {
        printf("Vérouillage impossible\n");
        rl_close(descriptor);
        return 1;
    }

    lock.l_start = 200;
    lock.l_len = 20;  
    if (rl_fcntl(descriptor, F_SETLK, &lock) == -1) {
        printf("Vérouillage impossible\n");
        rl_close(descriptor);
        return 1;
    }

    lock.l_type = F_UNLCK;
    lock.l_start = 50;
    lock.l_len = 20; 

    if (rl_fcntl(descriptor, F_SETLK, &lock) == -1) {
        perror("Erreur lors du déverrouillage du fichier");
        return 1;
    }
    lock.l_type = F_WRLCK;  // Verrou de lecture
    lock.l_start = 0;
    lock.l_len = 50; // 5-15
    if (rl_fcntl(descriptor, F_SETLK, &lock) == -1) {
        perror("Erreur lors du déverrouillage du fichier");
        return 1;
    }*/

    sleep(600);

    /*lock.l_start = 0;
    lock.l_len = 5;
    rl_fcntl(descriptor, F_SETLK, &lock);

    lock.l_start = 7;
    lock.l_len = 2;
    rl_fcntl(descriptor, F_SETLK, &lock);



    printf("Fichier verrouillé avec succès\n");

    // Écriture dans le fichier verrouillé
    const char *message = "Hello, world!";
    if (write(descriptor.d, message, strlen(message)) == -1) {
        perror("Erreur lors de l'écriture dans le fichier");
        return 1;
    }

    printf("Message écrit dans le fichier\n");
    
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;  // Verrouille tout le fichier

    if (rl_fcntl(descriptor, F_SETLK, &lock) == -1) {
        printf("Erreur lors du verrouillage du fichier\n");
        rl_close(descriptor);
        return 1;
    }
    // Déverrouillage du fichier avec rl_fcntl*/
    /*lock.l_type = F_UNLCK;

    if (rl_fcntl(descriptor, F_SETLK, &lock) == -1) {
        perror("Erreur lors du déverrouillage du fichier");
        return 1;
    }

    printf("Fichier déverrouillé avec succès\n");*/

    // Fermeture du descripteur de fichier avec rl_close
    /*if (rl_close(descriptor) == -1) {
        perror("Erreur lors de la fermeture du descripteur de fichier");
        return 1;
    }*/
}
