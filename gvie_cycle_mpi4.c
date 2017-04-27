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
  int partner_up = (world_rank+1) % world_size;
  int partner_down = (world_rank-1) % world_size;

  printf("[%d] Lignes de %d à %d\n", world_rank, offset, lines+offset-1);
  // ***** Affectation des lignes par process *****

  // allocation dynamique sinon stack overflow...
  char (*tt)[hm][lm] = calloc(sizeof(char[hm][lm]), LONGCYCLE);  // tableau de tableaux
  int value = 0;
  int finalValue = 0;
  int kill = 0;

  /* initialisation du premier tableau */
  init(hm, lm, tt[0]);
  MPI_Op MyOp;
  MPI_Op_create(ppcm_op, 1, &MyOp);

  gettimeofday(&tv_init, 0);

  for (size_t i=0 ; i<ITER ; i++) {
    /* calcul du nouveau tableau i+1 en fonction du tableau i */
    if (world_rank == ROOT) {
      calcnouv(hm, lm, tt[i%LONGCYCLE], tt[(i+1)%LONGCYCLE], offset, lines);
      MPI_Ssend(tt[(i+1)%LONGCYCLE], hm*lm, MPI_CHAR, partner_up, 0, MPI_COMM_WORLD);
      MPI_Recv(tt[(i+1)%LONGCYCLE], hm*lm, MPI_CHAR, partner_down, 0, MPI_COMM_WORLD,MPI_STATUS_IGNORE);
    }
    else {
      MPI_Recv(tt[(i+1)%LONGCYCLE], hm*lm, MPI_CHAR, partner_down, 0, MPI_COMM_WORLD,MPI_STATUS_IGNORE);
      calcnouv(hm, lm, tt[i%LONGCYCLE], tt[(i+1)%LONGCYCLE], offset, lines);
      MPI_Ssend(tt[(i+1)%LONGCYCLE], hm*lm, MPI_CHAR, partner_up, 0, MPI_COMM_WORLD);
    }
    /* comparaison du nouveau tableau avec les (LONGCYCLE-1) précédents */
    value=0;
    for (size_t j=LONGCYCLE-1; j>0; j--)
    {
      if (egal(hm, lm, tt[(i+1)%LONGCYCLE], tt[(i+1+j)%LONGCYCLE], offset, lines)) {
        value = (LONGCYCLE-j);
        break;
      }
    }
    MPI_Reduce(&value, &finalValue, 1, MPI_INT, MyOp, ROOT, MPI_COMM_WORLD);

    if (world_rank == ROOT && finalValue != 0)
    {
      // On attend les autres process
      gettimeofday(&tv_end, 0);
      printf("[%d] Cycle trouvé : longueur %d\n",
              world_rank,
              finalValue);
      printf("Calcul : %lfs.\n", DIFFTEMPS(tv_init,tv_end));
      kill = 1;
    }
    MPI_Bcast(&kill, 1, MPI_INT, ROOT, MPI_COMM_WORLD);
    if (kill)
      goto CLEANUP;
  }

  gettimeofday(&tv_end, 0);
  printf("pas de cycle trouvé en %d itérations\n", ITER);
  printf("Calcul : %lfs.\n", DIFFTEMPS(tv_init,tv_end));

 CLEANUP:
  free(tt);
  MPI_Finalize();
  return EXIT_SUCCESS;
}
