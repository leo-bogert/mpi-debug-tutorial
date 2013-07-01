#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define items 1222333
int array[items]; // Goal of the program: Summing up this array. (Instead use allocated memory in real programs!)
long sum = 0; // The result of the computation

long sum__sequential_reference_implementation() { // Non-parallel reference implementation
  long s = 0;
  for(int item = 0; item < items; ++item)
    s += array[item];
  return s;
}

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);

  int my_rank; // Number of the node
  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

  int node_count; // Total number of nodes
  MPI_Comm_size(MPI_COMM_WORLD, &node_count);
  
  // The root must load the input data to distribute to the other nodes
  if(my_rank == 0) {
    // In our case it generates a random array as input data
    srand(time(NULL));
    for(int item = 0; item < items; ++item)
      array[item] = rand();
  }
  
  int items_per_rank = items / node_count;
  int remainder_items = items % node_count;
  int* my_work;
  MPI_Alloc_mem(items_per_rank * sizeof(int), MPI_INFO_NULL, &my_work);
 
  // MPI_Scatter is a collective operation which distributes an equal-sized part of the given array to each node.
  MPI_Scatter(&array[remainder_items] /* send buffer */, items_per_rank /* send count per node */, MPI_INT /* send type */,
	      my_work /* receive buffer on each node */, items_per_rank /* receive count */ , MPI_INT /* receive type */, 
	      0 /* send buffer is stored on this rank */, MPI_COMM_WORLD /* communication channel */);
 
  // This is the actual working-loop
  long sub_sum = 0;
  while(items_per_rank > 0)
    sub_sum += my_work[--items_per_rank];

  if(my_rank == 0) { // Scatter cannot deal with a division remainder so we manually deal with it
    while(remainder_items > 0)
      sub_sum += array[--remainder_items];
  }

  MPI_Free_mem(my_work);

  // MPI_Reduce with op-code MPI_SUM is a collective operation which sums up the input sub_sum of each node
  // into single a resulting output sum on the master.
  MPI_Reduce(&sub_sum /* input to sum up */, &sum /* output */, 1 /* input count */, MPI_LONG /* input type */,
	     MPI_SUM /* operation */, 0 /* output is stored on this rank */, MPI_COMM_WORLD /* communication channel */);
 
  if(my_rank == 0) {
    // The result of the computation now is available on rank 0.
    // We compare it with the sequential reference implementation to test our parallel implementation.
    if(sum == sum__sequential_reference_implementation())
      fprintf(stderr, "Test OK.\n");
    else
      fprintf(stderr, "Test FAILED!\n");
  }

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
  return EXIT_SUCCESS;
}
