#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int my_rank; // Number of the node
int max_rank; // Count of nodes

#define ITEMS 1222333
int array[ITEMS]; // Goal of the program: Summing up this array
long sum = 0; // The result of the computation

long sum__sequential_reference_implementation() { // Non-parallel reference implementation
  long s = 0;
  for(int item = 0; item < ITEMS; ++item)
    s += array[item];
  return s;
}

void run_master() { // Runs on rank 0
  srand(time(NULL));
  for(int item = 0; item < ITEMS; ++item) {
    array[item] = rand();
  }

  int items_per_rank = ITEMS / (max_rank-1) ; // -1 because master does not process items
  int item = 0;
  for(int rank = 1; rank < max_rank; ++rank) { // Send data to slaves
    MPI_Ssend(&array[item], items_per_rank, MPI_INT, rank, 0, MPI_COMM_WORLD);
    item += items_per_rank;
  }

  long sub_sum = 0; // Dummy sub sum for the master
  MPI_Reduce(&sub_sum, &sum, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

  if(sum == sum__sequential_reference_implementation())
    fprintf(stderr, "run_master(): Test OK.\n");
  else
    fprintf(stderr, "run_master(): Test FAILED!\n");
}

void run_slave() { // Runs on all nodes EXCEPT rank 0
  int items_per_rank = ITEMS / (max_rank-1); // -1 because master does not process items

  // Receive data
  MPI_Recv(&array[items_per_rank * (my_rank-1)], items_per_rank, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  long sub_sum = 0;
  for(int item = 0; item < items_per_rank ; ++item) { // Do the work
    sub_sum += array[items_per_rank * (my_rank-1) + item];
  }

  MPI_Reduce(&sub_sum, &sum, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
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
