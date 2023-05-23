#include "rl_lock_library.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main() {
    // Initialisation de la bibliothèque
    rl_init_library();

    // Ouverture d'un fichier avec rl_open
    rl_descriptor descriptor = rl_open("test.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (descriptor.d == -1) {
        perror("Erreur lors de l'ouverture du fichier");
        return 1;
    }

    printf("Fichier ouvert avec succès\n");

    // Verrouillage du fichier avec rl_fcntl
    struct flock lock;
    lock.l_type = F_WRLCK;  // Verrou d'écriture
    lock.l_whence = SEEK_SET;
    lock.l_start = 2;
    lock.l_len = 3;  // Verrouille tout le fichier

    if (rl_fcntl(descriptor, F_SETLK, &lock) == -1) {
        perror("Erreur lors du verrouillage du fichier");
        return 1;
    }

    printf("Fichier verrouillé avec succès\n");

    // Écriture dans le fichier verrouillé
    const char *message = "Hello, world!";
    if (write(descriptor.d, message, strlen(message)) == -1) {
        perror("Erreur lors de l'écriture dans le fichier");
        return 1;
    }

    printf("Message écrit dans le fichier\n");
    sleep(10);

    // Déverrouillage du fichier avec rl_fcntl
    lock.l_type = F_UNLCK;

    if (rl_fcntl(descriptor, F_SETLK, &lock) == -1) {
        perror("Erreur lors du déverrouillage du fichier");
        return 1;
    }

    printf("Fichier déverrouillé avec succès\n");

    // Fermeture du descripteur de fichier avec rl_close
    if (rl_close(descriptor) == -1) {
        perror("Erreur lors de la fermeture du descripteur de fichier");
        return 1;
    }

    printf("Descripteur de fichier fermé avec succès\n");

    return 0;
}
