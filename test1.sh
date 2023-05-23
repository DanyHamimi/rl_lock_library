#!/bin/bash

# Exécution de a.out en arrière-plan
./test1 &

# Exécution de b.out en arrière-plan
./test2 &

# Attendre la fin des deux processus
wait

# Afficher un message lorsque les deux processus sont terminés
echo "Les programmes a.out et b.out ont été exécutés simultanément."
