#include <assert.h>
#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

// The number of total nodes among which the computation is distributed. Must not be greater than ROWS
int node_count;

// MPI node index of this node.
int my_rank;

#define MATRICES 4
#define ROWS 300
#define COLS ROWS
int matrices[MATRICES][ROWS][COLS];

/**
 * Each message we send between nodes is tagged with one of these types.
 */
enum MESSAGETYPES {
  MESSAGETYPE_ROW,
  MESSAGETYPE_MATRIX
};

void sync_send_row(int matrix, int row, int rank) {
  fprintf(stderr, "%d: sync_send_row(matrix=%d, row=%d, to=%d)\n", my_rank, matrix, row, rank);

  assert(matrix < MATRICES);
  assert(row < ROWS);
  assert(rank < node_count);

  MPI_Ssend(&matrix, 1, MPI_INT, rank, MESSAGETYPE_ROW, MPI_COMM_WORLD);
  MPI_Ssend(&row, 1, MPI_INT, rank, MESSAGETYPE_ROW, MPI_COMM_WORLD);
  MPI_Ssend(&matrices[matrix][row], COLS, MPI_INT, rank, MESSAGETYPE_ROW, MPI_COMM_WORLD);
}

int sync_receive_row() {
  fprintf(stderr, "%d: sync_receive_row()\n", my_rank);

  int matrix;
  MPI_Recv(&matrix, 1, MPI_INT, MPI_ANY_SOURCE, MESSAGETYPE_ROW, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  int row;
  MPI_Recv(&row, 1, MPI_INT, MPI_ANY_SOURCE, MESSAGETYPE_ROW, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  MPI_Recv(&matrices[matrix], COLS, MPI_INT, MPI_ANY_SOURCE, MESSAGETYPE_ROW, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  fprintf(stderr, "%d: sync_receive_row(): Received row %d of matrix %d\n", my_rank, row, matrix);

  return row;
}

void sync_send_matrix(int matrix, int rank) {
  fprintf(stderr, "%d: sync_send_matrix(matrix=%d, to=%d)\n", my_rank, matrix, rank);

  assert(matrix < MATRICES);
  assert(rank < node_count);

  MPI_Ssend(&matrix, 1, MPI_INT, rank, MESSAGETYPE_MATRIX, MPI_COMM_WORLD);
  MPI_Ssend(&matrices[matrix], ROWS * COLS, MPI_INT, rank, MESSAGETYPE_MATRIX, MPI_COMM_WORLD);
}

void sync_receive_matrix() {
  fprintf(stderr, "%d: sync_receive_matrix()\n", my_rank);

  int matrix;
  MPI_Recv(&matrix, 1, MPI_INT, MPI_ANY_SOURCE, MESSAGETYPE_MATRIX, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  MPI_Recv(&matrices[matrix], ROWS * COLS, MPI_INT, MPI_ANY_SOURCE, MESSAGETYPE_MATRIX, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  fprintf(stderr, "%d: sync_receive_matrix(): Received matrix %d\n", my_rank, matrix);
}

void generate_random_matrix(int matrix) {
  srand(time(NULL));

  for(int row=0; row < ROWS; ++row) {
    for(int col=0; col < COLS; ++col) {
      matrices[matrix][row][col] = rand() % 10; // WARNING: Don't use % if you want truely random numbers!
    }
  }
}

void zero_matrix(int matrix) {
  memset(&matrices[matrix], 0, sizeof(matrices[matrix][0][0]) * ROWS * COLS);
}

void compute_test_matrix() {
  for(int i=0; i < ROWS; ++i) {
    for(int j=0; j < COLS; ++j) {
      matrices[3][i][j] = 0;
      for(int k=0; k < COLS; ++k) {
	matrices[3][i][j] += matrices[0][i][k] * matrices[1][k][j];
      }
    }
  }
}

void compare_matrices(int matrix1, int matrix2) {
  bool match = true;

  for(int i=0; i < ROWS; ++i) {
    for(int j=0; j < COLS; ++j) {
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

  for(int row=0; row < ROWS; ++row) {
    for(int col=0; col < COLS; ++col) {
      printf("%d ", matrices[matrix][row][col]);
    }
    printf("\n");
  }

  printf("\n");
}

int get_row_count_for_node(int rank) {
  assert(rank < node_count);

  int rows_per_node = ROWS / (node_count-1);
  int remainder_rows = ROWS % (node_count-1);
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
    sync_send_matrix(1, node);

    int rows_to_send = get_row_count_for_node(node);

    do {
      sync_send_row(0, row, node);
      sync_receive_row(node);
      ++row;
      --rows_to_send;
    } while(rows_to_send > 0);
  }

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

    for(int col1=0; col1 < COLS ; ++col1) {
      int out_element = 0;

      for(int col0=0; col0 < COLS; ++col0) {
	out_element += matrices[0][row][col0] * matrices[1][col0][col1];
      }

      matrices[2][row][col1] = out_element;
    }

    sync_send_row(2, row, 0);
  } while(--row_count > 0);
}

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &node_count);

  int exit_code = EXIT_SUCCESS;

  if(node_count > ROWS) {
    fprintf(stderr, "Number of nodes is greater than row count: %d > %d", node_count, ROWS);
    exit_code = EXIT_FAILURE;
    goto cleanup; // TODO: Is this a valid programming pattern at ZDV?
  }
 
  if(my_rank == 0) {
    run_master();
  } else {
    run_slave();
  }

 cleanup:
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();

  return exit_code;
}
