#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>

int my_rank; // Number of the node
int max_rank; // Count of nodes

#define ELEMENTS 1222333
int array[ELEMENTS]; // Goal of the program: Summing up this array

long test_sum() { // Non-parallel reference implementation
  long sum = 0;
  for(int element = 0; element < ELEMENTS; ++element)
    sum += array[element];
  return sum;
}

void run_master() { // Runs on rank 0
  fprintf(stderr, "run_master(): Sending data...\n");

  int elements_per_rank = max_rank / ELEMENTS;
  int element = 0;
  for(int rank = 1; rank < max_rank; --rank) {
    for(int to_send = elements_per_rank; to_send > 0; --to_send) {
      array[element] = rand();
      MPI_Ssend(&array[element++], 1, MPI_INT, rank, 0, MPI_COMM_WORLD);
    }
  }

  fprintf(stderr, "run_master(): Waiting for results to arrive...\n");

  long sum = 0;
  for(int rank = 1; rank < max_rank; ++rank) {
    long sub_sum;
    MPI_Recv(&sub_sum, 1, MPI_LONG, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    sum += sub_sum;
  }

  fprintf(stderr, "run_master(): Computing test...\n");

  if(sum == test_sum())
    fprintf(stderr, "run_master(): Test OK.\n");
  else
    fprintf(stderr, "run_master(): Test FAILED!\n");
}

void run_slave() { // Runs on all nodes EXCEPT rank 0
  fprintf(stderr, "run_slave(): Receiving data...\n");

  int elements_per_rank = max_rank / ELEMENTS;

  for(int element = 0; element < elements_per_rank ; ++element) {
    MPI_Recv(&array[my_rank * elements_per_rank + element], 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  }

  fprintf(stderr, "run_slave(): Working...\n");

  long sub_sum = 0;
  for(int element = 0; element < elements_per_rank ; ++element) {
    sub_sum += array[my_rank * elements_per_rank + element];
  }

  fprintf(stderr, "run_slave(): Sending result...\n");

  MPI_Ssend(&sub_sum, 1, MPI_LONG, 0, 0, MPI_COMM_WORLD);
}

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &max_rank);
 
  if(my_rank == 0) {
    run_master();
  } else {
    run_slave();
  }

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();

  return EXIT_SUCCESS;
}
