#include <assert.h>
#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define MATRICES 4
// Size of the matrices
#define ROWS 32
#define COLS 32

// Size of each individual matrices in case we ever allow non-square matrices.
int rows[4] = { ROWS, ROWS, ROWS, ROWS } ;
int cols[4] = { COLS, COLS, COLS, COLS } ;

// The number of total nodes among which the computation is distributed. Must not be greater than ROWS
int node_count = -1;

// MPI node index of this node
int my_rank = -1;

/**
 * Contents:
 * There are 4 matrices stored in this array:
 * Input A
 * Input B
 * Result C (parallely computed)
 * Test result C (sequentially computed)
 *
 * Layout:
 * matrices[matrix_number][row][column] = integer_element;
 *
 * Dimensions: 
 * As specified in rows[matrix_number] and cols[matrix_number]
 *
 * Notice:
 * The elements of the columns are stored linearly in continous memory.
 *
 * @see http://en.wikipedia.org/wiki/Row-major_order
 */
int matrices[MATRICES][ROWS][COLS];

// For each async send of a row a request is added. Async sends both happen on master and slaves.
MPI_Request row_sends[ROWS];
int row_sends_index = 0;

// For each async receive of a row on the master a request is added. There are no async receives on the slaves.
MPI_Request row_receives[ROWS];
int row_receives_index = 0;

// For each async receive of a row on the master a buffer is added. There are no async receives on the slaves
int row_receive_buffers[ROWS][1 + 1 + COLS]; // 1 int for matrix index, 1 int for row index
int row_receive_buffers_index = 0;

// For each async send of a matrix on the master a request is added. There are no async sends on the slaves.
MPI_Request matrix_sends[ROWS]; // The maximal number of nodes which we can distribute the computation to is equal to the number of nodes.
int matrix_sends_index = 0;

/**
 * Each message we send between nodes is tagged with one of these types.
 */
enum MESSAGETYPES {
  MESSAGETYPE_ROW,
  MESSAGETYPE_MATRIX
};

void async_send_row(int matrix, int row, int rank) {
  fprintf(stderr, "%d: async_send_row(matrix=%d, row=%d, to=%d)\n", my_rank, matrix, row, rank);

  assert(matrix < MATRICES);
  assert(row < rows[matrix]);
  assert(rank < node_count);

  int buffer_size;
  MPI_Pack_size(1 + 1 + cols[matrix], MPI_INT, MPI_COMM_WORLD, &buffer_size);

  char buffer[buffer_size];
  int position = 0;

  MPI_Pack(&matrix, 1, MPI_INT, &buffer, buffer_size, &position, MPI_COMM_WORLD);
  MPI_Pack(&row, 1, MPI_INT, &buffer, buffer_size, &position, MPI_COMM_WORLD);
  MPI_Pack(&matrices[matrix][row], cols[matrix], MPI_INT, &buffer, buffer_size, &position, MPI_COMM_WORLD);

  assert(row_sends_index < ARRAY_SIZE(row_sends));
  MPI_Isend(&buffer, position, MPI_PACKED, rank, MESSAGETYPE_ROW, MPI_COMM_WORLD, &row_sends[row_sends_index++]);
}

int sync_receive_row() {
  fprintf(stderr, "%d: sync_receive_row()\n", my_rank);

  MPI_Status status;
  MPI_Probe(MPI_ANY_SOURCE, MESSAGETYPE_ROW, MPI_COMM_WORLD, &status);

  int buffer_size;
  MPI_Get_count(&status, MPI_PACKED, &buffer_size);

  char buffer[buffer_size];
  int buffer_position = 0;
  MPI_Recv(&buffer, buffer_size, MPI_PACKED, MPI_ANY_SOURCE, MESSAGETYPE_ROW, MPI_COMM_WORLD, NULL);

  int matrix;
  MPI_Unpack(&buffer, buffer_size, &buffer_position, &matrix, 1, MPI_INT, MPI_COMM_WORLD);
  assert(matrix < MATRICES);

  int row;
  MPI_Unpack(&buffer, buffer_size, &buffer_position, &row, 1, MPI_INT, MPI_COMM_WORLD);
  assert(row < rows[matrix]);

  MPI_Unpack(&buffer, buffer_size, &buffer_position, &matrices[matrix][row], cols[matrix], MPI_INT, MPI_COMM_WORLD);

  fprintf(stderr, "%d: sync_receive_row(): Received row %d\n", my_rank, row);

  return row;
}

void async_receive_row(int rank) {
  fprintf(stderr, "%d: async_receive_row(rank=%d)\n", my_rank, rank);
  assert(rank < node_count);

  int buffer_size = ARRAY_SIZE(row_receive_buffers[row_receive_buffers_index]);

  assert(row_receive_buffers_index < ARRAY_SIZE(row_receive_buffers));
  assert(row_receives_index < ARRAY_SIZE(row_receives));
  assert(row_receive_buffers_index == row_receives_index);
  MPI_Irecv(&row_receive_buffers[row_receive_buffers_index++], buffer_size, MPI_INT, rank, MESSAGETYPE_ROW, MPI_COMM_WORLD, &row_receives[row_receives_index++]);
}

void wait_for_async_receives() {
  fprintf(stderr, "%d: wait_for_async_receives()\n", my_rank);

  MPI_Waitall(row_receives_index, row_receives, NULL);

  for(int i=0; i < row_receive_buffers_index; ++i) {
    int matrix = row_receive_buffers[i][0];
    int row = row_receive_buffers[i][1];
    assert(matrix == 2);
    assert(row < rows[matrix]);

    fprintf(stderr,"%d: wait_for_async_receives(): Copying row %d of matrix %d from receive buffer...\n", my_rank, row, matrix);
    memcpy(&matrices[matrix][row], &row_receive_buffers[i][2], cols[matrix]*sizeof(matrices[matrix][row][0]));
  }
}

void async_send_matrix(int matrix, int rank) {
  fprintf(stderr, "%d: async_send_matrix(matrix=%d, to=%d)\n", my_rank, matrix, rank);

  assert(matrix < MATRICES);
  assert(rank < node_count);

  int buffer_size;
  MPI_Pack_size(1 + rows[matrix] * cols[matrix], MPI_INT, MPI_COMM_WORLD, &buffer_size);

  char buffer[buffer_size];
  int position = 0;

  MPI_Pack(&matrix, 1, MPI_INT, &buffer, buffer_size, &position, MPI_COMM_WORLD);
  MPI_Pack(&matrices[matrix], rows[matrix] * cols[matrix], MPI_INT, &buffer, buffer_size, &position, MPI_COMM_WORLD);

  assert(matrix_sends_index < ARRAY_SIZE(matrix_sends));
  MPI_Isend(&buffer, position, MPI_PACKED, rank, MESSAGETYPE_MATRIX, MPI_COMM_WORLD, &matrix_sends[matrix_sends_index++]);
}

void sync_receive_matrix() {
  fprintf(stderr, "%d: sync_receive_matrix()\n", my_rank);

  MPI_Status status;
  MPI_Probe(MPI_ANY_SOURCE, MESSAGETYPE_MATRIX, MPI_COMM_WORLD, &status);

  int buffer_size;
  MPI_Get_count(&status, MPI_PACKED, &buffer_size);

  char buffer[buffer_size];
  int buffer_position = 0;
  MPI_Recv(&buffer, buffer_size, MPI_PACKED, MPI_ANY_SOURCE, MESSAGETYPE_MATRIX, MPI_COMM_WORLD, NULL);

  int matrix;
  MPI_Unpack(&buffer, buffer_size, &buffer_position, &matrix, 1, MPI_INT, MPI_COMM_WORLD);
  assert(matrix < MATRICES);

  MPI_Unpack(&buffer, buffer_size, &buffer_position, &matrices[matrix], rows[matrix] * cols[matrix], MPI_INT, MPI_COMM_WORLD);

  fprintf(stderr, "%d: sync_receive_matrix(): Received matrix %d\n", my_rank, matrix);
}

void generate_random_matrix(int matrix) {
  for(int row=0; row < rows[matrix]; ++row) {
    for(int col=0; col < cols[matrix]; ++col) {
      matrices[matrix][row][col] = rand() % 10; // WARNING: Don't use % if you want truely random numbers!
    }
  }
}

void zero_matrix(int matrix) {
  memset(&matrices[matrix], 0, sizeof(matrices[matrix][0][0]) * rows[matrix] * cols[matrix]);
}

void compute_test_matrix() {
  assert(cols[0] == rows[1]);

  for(int i=0; i < rows[3]; ++i) {
    for(int j=0; j < cols[3]; ++j) {
      matrices[3][i][j] = 0;
      for(int k=0; k < cols[0]; ++k) {
	matrices[3][i][j] += matrices[0][i][k] * matrices[1][k][j];
      }
    }
  }
}

void compare_matrices(int matrix1, int matrix2) {
  bool match = true;

  for(int i=0; i < rows[matrix1]; ++i) {
    for(int j=0; j < cols[matrix2]; ++j) {
      if(matrices[matrix1][i][j] != matrices[matrix2][i][j]) {
	fprintf(stderr, "mismatch at %d,%d: %d != %d\n", i, j, matrices[matrix1][i][j], matrices[matrix2][i][j]);
	match = false;
      }
      //if(matrices[matrix1][i][j] == matrices[matrix2][i][j])
      //  fprintf(stderr, "match at %d,%d: %d == %d\n", i, j, matrices[matrix1][i][j], matrices[matrix2][i][j]);
    }
  }

  if(match)
    printf("OK: parallel and sequential computation match.\n");
  else
    printf("FAIL: parallel and sequential computation do NOT match!\n");
}

void print_matrix(int matrix) {
  printf("matrix %d:\n", matrix);

  for(int row=0; row < rows[matrix]; ++row) {
    for(int col=0; col < cols[matrix]; ++col) {
      printf("%d ", matrices[matrix][row][col]);
    }
    printf("\n");
  }

  printf("\n");
}

int get_row_count_for_node(int rank) {
  assert(rank < node_count);

  int rows_per_node = rows[0] / (node_count-1);
  int remainder_rows = rows[0] % (node_count-1);
  int row_count = rows_per_node;

  if(rank == (node_count-1) && remainder_rows > 0)
    row_count += remainder_rows;

  fprintf(stderr, "%d: get_row_count_for_node(%d): node_count=%d rows_per_node=%d remainder_rows=%d row_count=%d\n", my_rank, rank, node_count, rows_per_node, remainder_rows, row_count);

  assert(row_count > 0);

  return row_count;
}

void run_master() {
  fprintf(stderr, "%d: run_master()\n", my_rank);

  generate_random_matrix(0); // input matrix A, we distribute the rows of it among the nodes
  generate_random_matrix(1); // input matrix B, we send this one completely to all nodes
  zero_matrix(2); // output matrix
  zero_matrix(3); // test matrix

  //for(int matrix = 0; matrix < 4; ++matrix)
  //  print_matrix(matrix);

  int row = 0;
  for(int node=1; node < node_count; ++node) {
    async_send_matrix(1, node);

    int rows_to_send = get_row_count_for_node(node);

    do {
      async_send_row(0, row, node);
      async_receive_row(node);
      ++row;
      --rows_to_send;
    } while(rows_to_send > 0);
  }

  fprintf(stderr, "%d: Waiting for results to arrive...\n", my_rank);
  wait_for_async_receives(); 

  compute_test_matrix();
  compare_matrices(2, 3); // compare parallel and sequential computation

  //for(int matrix = 0; matrix < 4; ++matrix)
  //  print_matrix(matrix);
}

void run_slave() {
  fprintf(stderr, "%d: run_slave()\n", my_rank);

  sync_receive_matrix();

  int row_count = get_row_count_for_node(my_rank);

  do {
    int row = sync_receive_row();

    for(int col1=0; col1 < cols[1] ; ++col1) {
      int out_element = 0;

      for(int col0=0; col0 < cols[0]; ++col0) {
	out_element += matrices[0][row][col0] * matrices[1][col0][col1];
      }

      matrices[2][row][col1] = out_element;
    }

    async_send_row(2, row, 0);
  } while(--row_count > 0);
}

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &node_count);

  if(node_count > ROWS) {
    fprintf(stderr, "Number of nodes is greater than row count: %d > %d", node_count, ROWS);
    return EXIT_FAILURE;
  }
  
  if(my_rank == 0) {
    run_master();
  } else {
    run_slave();
  }

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();

  return EXIT_SUCCESS;
}
