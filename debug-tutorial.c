#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int my_rank; // Number of the node
int node_count; // Total number of nodes

#define ITEMS 1222333
int array[ITEMS]; // Goal of the program: Summing up this array
long sum = 0; // The result of the computation

long sum__sequential_reference_implementation() { // Non-parallel reference implementation
  long s = 0;
  for(int item = 0; item < ITEMS; ++item)
    s += array[item];
  return s;
}

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &node_count);
 
  // The root must load the input data to distribute to the other nodes
  if(my_rank == 0) {
    // In our case it generates a random array as input data
    srand(time(NULL));
    for(int item = 0; item < ITEMS; ++item)
      array[item] = rand();
  }
  
  int items_per_rank = ITEMS / node_count;
  int remainder_items = ITEMS % node_count;
  int* my_work;
  MPI_Alloc_mem(items_per_rank * sizeof(int), MPI_INFO_NULL, &my_work);
 
  MPI_Scatter(&array[remainder_items], items_per_rank, MPI_INT, my_work, items_per_rank, MPI_INT, 0, MPI_COMM_WORLD);
 
  // This is the actual working-loop
  long sub_sum = 0;
  for(int item = 0; item < items_per_rank; ++item)
    sub_sum += my_work[item];

  if(my_rank == 0) {
    while(remainder_items-- > 0)
      sub_sum += array[remainder_items];
  }

  MPI_Free_mem(my_work);

  MPI_Reduce(&sub_sum, &sum, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
 
  if(my_rank == 0) {
    // The root can now process the result of the computation.
    // In our case, we compare it with the sequential reference implementation.
    if(sum == sum__sequential_reference_implementation())
      fprintf(stderr, "Test OK.\n");
    else
      fprintf(stderr, "Test FAILED!\n");
  }

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();

  return EXIT_SUCCESS;
}
