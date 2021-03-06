#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <locale.h>
#include <mpi.h>

#include "functions.h"

// nombre maximal d'itérations
#define ITER 10000

/* longueur cycle recherche de cycle (-1) */
#define LONGCYCLE 51

// On definit le process racine
#define ROOT 0

int main(int argc, char* argv[argc+1]) {

  // ***** Initialisation de l'environnement MPI *****
  MPI_Init(NULL, NULL);
  int world_size, world_rank;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  // ***** Initialisation de l'environnement MPI *****


  setlocale(LC_ALL, "");
  struct timeval tv_init, tv_end;

  size_t hm = 600;
  size_t lm = 800;
  switch (argc) {
  default:;
    lm = strtoull(argv[2], 0, 0);
  case 2:;
    hm = strtoull(argv[1], 0, 0);
  case 1:;
  case 0:;
  }

  // ***** Affectation des lignes par process *****
  if (world_rank == ROOT && hm%world_size != 0) {
    printf("On doit avoir %zu = \"k\" * %d\n", hm, world_size);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  MPI_Barrier(MPI_COMM_WORLD);

  int lines = hm/world_size;
  int offset = lines*world_rank;

  printf("[%d] Lignes de %d à %d\n", world_rank, offset, lines+offset-1);
  // ***** Affectation des lignes par process *****

  // allocation dynamique sinon stack overflow...
  char (*tt)[hm][lm] = calloc(sizeof(char[hm][lm]), LONGCYCLE);  // tableau de tableaux

  /* initialisation du premier tableau */
  init(hm, lm, tt[0]);

  gettimeofday(&tv_init, 0);

  for (size_t i=0 ; i<ITER ; i++) {
    /* calcul du nouveau tableau i+1 en fonction du tableau i */
    calcnouv(hm, lm, tt[i%LONGCYCLE], tt[(i+1)%LONGCYCLE], offset, lines);

    /* comparaison du nouveau tableau avec les (LONGCYCLE-1) précédents */
    for (size_t j=LONGCYCLE-1; j>0; j--)
    {
      if (egal(hm, lm, tt[(i+1)%LONGCYCLE], tt[(i+1+j)%LONGCYCLE], offset, lines)) {
        // ***** On attend que chaque process finit et on finalize *****
        MPI_Barrier(MPI_COMM_WORLD);
        gettimeofday(&tv_end, 0);
        printf("[%d] Cycle trouvé : iteration %zu, longueur %zu\n",
                world_rank,
                i+1-(LONGCYCLE-j),
                LONGCYCLE-j);
        printf("[%d] Calcul : %lfs.\n", world_rank, DIFFTEMPS(tv_init,tv_end));
        goto CLEANUP;
        // ***** On attend que chaque process finit et on finalize *****
      }
    }
  }

  gettimeofday(&tv_end, 0);
  printf("pas de cycle trouvé en %d itérations\n", ITER);
  printf("Calcul : %lfs.\n", DIFFTEMPS(tv_init,tv_end));

 CLEANUP:
  free(tt);
  MPI_Finalize();
  return EXIT_SUCCESS;
}
