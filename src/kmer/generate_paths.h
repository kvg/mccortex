#ifndef GENERATE_PATHS_H_
#define GENERATE_PATHS_H_

#include "seq_file.h"

#include "graph_typedef.h"
#include "db_graph.h"
#include "loading_stats.h"

typedef struct
{
  seq_file_t *const file1, *const file2;
  const Colour ctpcol, ctxcol;
  uint32_t ins_gap_min, ins_gap_max;
  const uint8_t fq_offset, fq_cutoff, hp_cutoff;
  SeqLoadingStats *stats; // results are placed here
} GeneratePathsTask;

typedef struct GenPathWorker GenPathWorker;

extern boolean gen_paths_print_inserts;

// Estimate memory required per worker thread
size_t gen_paths_worker_est_mem(const dBGraph *db_graph);

GenPathWorker* gen_paths_workers_alloc(size_t n, dBGraph *graph);
void gen_paths_workers_dealloc(GenPathWorker *mem, size_t n);

// workers array must be at least as long as tasks
void generate_paths(GeneratePathsTask *tasks, size_t num_tasks,
                    GenPathWorker *workers, size_t num_workers);

// Save gap size distribution
// base_fmt is the beginning of the file name - the reset is <num>.csv or something
// insert_sizes is true if gaps are insert gaps,
//                 false if gaps are due to sequencing errors
void gen_paths_dump_gap_sizes(const char *base_fmt,
                              const uint64_t *arr, size_t arrlen,
                              size_t kmer_size, boolean insert_sizes,
                              size_t nreads);

#endif /* GENERATE_PATHS_H_ */