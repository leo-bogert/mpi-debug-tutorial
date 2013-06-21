#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int my_rank; // Number of the node
int max_rank; // Count of nodes

#define ITEMS 1222333
int array[ITEMS]; // Goal of the program: Summing up this array

void randomize_array() {
  srand(time(NULL));
  for(int item = 0; item < ITEMS; ++item) {
    array[item] = rand();
  }
}

long test_sum() { // Non-parallel reference implementation
  long sum = 0;
  for(int item = 0; item < ITEMS; ++item)
    sum += array[item];
  return sum;
}

void run_master() { // Runs on rank 0
  randomize_array();

  fprintf(stderr, "run_master(): Sending data...\n");

  int items_per_rank = ITEMS / (max_rank-1) ; // -1 because master does not process items
  int item = 0;
  for(int rank = 1; rank < max_rank; --rank) {
    for(int to_send = items_per_rank; to_send > 0; --to_send) {
      MPI_Ssend(&array[item++], 1, MPI_INT, rank, 0, MPI_COMM_WORLD);
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

  int items_per_rank = ITEMS / (max_rank-1); // -1 because master does not process items

  for(int item = 0; item < items_per_rank ; ++item) {
    MPI_Recv(&array[item], 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  }

  fprintf(stderr, "run_slave(): Working...\n");

  long sub_sum = 0;
  for(int item = 0; item < items_per_rank ; ++item) {
    sub_sum += array[item];
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
