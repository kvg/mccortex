#include "global.h"
#include <time.h> // srand

#include "cmd.h"
#include "util.h"
#include "file_util.h"
#include "db_graph.h"
#include "db_node.h"
#include "binary_kmer.h"
#include "graph_format.h"
#include "path_format.h"
#include "graph_walker.h"

static const char usage[] =
"usage: traversal [options] <input.ctx>\n"
"  Get graph traversal statistics\n"
"  Options: [ -m <mem> | -h <kmers> | -p <paths.ctp> ]\n"
"           [ --nsamples <N> | --colour <c> ]\n";

int main(int argc, char **argv)
{
  ctx_msg_out = stdout;
  CmdArgs args;
  cmd_alloc(&args, argc, argv);
  cmd_accept_options(&args, "mhp");
  argc = args.argc;
  argv = args.argv;

  size_t num_samples = 10000, colour = 0;

  while(argc > 0 && argv[0][0] == '-') {
    if(strcmp(argv[0],"--nsamples") == 0) {
      unsigned long tmp;
      if(argc == 1 || !parse_entire_ulong(argv[1], &tmp))
        print_usage(usage, "--nsamples <N> requires an integer argument");
      num_samples = tmp;
      argv += 2; argc -= 2;
    }
    else if(strcmp(argv[0],"--colour") == 0) {
      unsigned long tmp;
      if(argc == 1 || !parse_entire_ulong(argv[1], &tmp))
        print_usage(usage, "--colour <c> requires an integer argument");
      colour = tmp;
      argv += 2; argc -= 2;
    }
    else print_usage(usage, "Unknown argument: %s", argv[0]);
  }

  if(argc != 1) print_usage(usage, NULL);
  const char *input_ctx_path = argv[0];

  /* initialize random seed: */
  srand(time(NULL));

  // probe binary
  boolean is_binary = false;
  GraphFileHeader gheader = {.capacity = 0};

  if(!graph_file_probe(input_ctx_path, &is_binary, &gheader))
    print_usage(usage, "Cannot read binary file: %s", input_ctx_path);
  else if(!is_binary)
    print_usage(usage, "Input binary file isn't valid: %s", input_ctx_path);

  // probe paths files
  boolean valid_paths_file = false;
  PathFileHeader pheader = {.capacity = 0};
  size_t i, j;

  if(args.num_ctp_files == 0)
    status("No path files (.ctp) to load");
  else if(args.num_ctp_files > 1)
    print_usage(usage, "Cannot load >1 .ctp file at the moment [use pmerge]");

  for(i = 0; i < args.num_ctp_files; i++)
  {
    if(!paths_file_probe(args.ctp_files[i], &valid_paths_file, &pheader))
      print_usage(usage, "Cannot read .ctp file: %s", args.ctp_files[i]);
    else if(!valid_paths_file)
      die("Invalid .ctp file: %s", args.ctp_files[i]);
  }


  // Get starting bkmer
  // BinaryKmer bkmer, bkey;
  // if(strlen(start_kmer) != kmer_size) die("length of kmer does not match kmer_size");
  // bkmer = binary_kmer_from_str(start_kmer, kmer_size);

  //
  // Decide on memory
  //
  size_t bits_per_kmer, kmers_in_hash, path_mem;

  bits_per_kmer = sizeof(Edges)*8 + gheader.num_of_cols + sizeof(uint64_t)*8;
  kmers_in_hash = cmd_get_kmers_in_hash(&args, bits_per_kmer,
                                        gheader.num_of_kmers, false);
  path_mem = args.mem_to_use -
             (kmers_in_hash * (sizeof(BinaryKmer)*8+bits_per_kmer)) / 8;

  char path_mem_str[100];
  bytes_to_str(path_mem, 1, path_mem_str);
  status("[memory] paths: %s\n", path_mem_str);

  dBGraph db_graph;
  GraphWalker wlk;

  db_graph_alloc(&db_graph, gheader.kmer_size, gheader.num_of_cols, 1, kmers_in_hash);
  graph_walker_alloc(&wlk);

  size_t node_bit_fields = round_bits_to_words64(db_graph.ht.capacity);

  db_graph.col_edges = calloc2(db_graph.ht.capacity, sizeof(Edges));
  db_graph.node_in_cols = calloc2(node_bit_fields * gheader.num_of_cols,
                                  sizeof(uint64_t));
  db_graph.kmer_paths = malloc2(db_graph.ht.capacity * sizeof(uint64_t));
  memset((void*)db_graph.kmer_paths, 0xff, db_graph.ht.capacity * sizeof(uint64_t));

  uint64_t *visited = calloc2(2 * node_bit_fields, sizeof(uint64_t));

  uint8_t *path_store = malloc2(path_mem);
  path_store_init(&db_graph.pdata, path_store, path_mem, gheader.num_of_cols);

  // Load graph
  SeqLoadingStats *stats = seq_loading_stats_create(0);
  SeqLoadingPrefs prefs = {.into_colour = 0, .db_graph = &db_graph,
                           .merge_colours = false,
                           .boolean_covgs = false,
                           .must_exist_in_graph = false,
                           .empty_colours = true};

  graph_load(input_ctx_path, &prefs, stats, NULL);
  seq_loading_stats_free(stats);

  hash_table_print_stats(&db_graph.ht);

  // Load path files
  for(i = 0; i < args.num_ctp_files; i++) {
    paths_format_read(args.ctp_files[i], &pheader, &db_graph,
                      &db_graph.pdata, false);
  }

  // Find start node
  // hkey_t node;
  // Orientation orient;
  // db_node_get_key(bkmer, kmer_size, bkey);
  // node = hash_table_find(&db_graph.ht, bkey);
  // orient = db_node_get_orientation(bkmer, bkey);
  Nucleotide lost_nuc;

  // char bkmerstr[MAX_KMER_SIZE+1];

  hkey_t node;
  Orientation orient;

  size_t len, junc, prev_junc;
  size_t total_len = 0, total_junc = 0, dead_ends = 0, n = 0;
  size_t lengths[num_samples], junctions[num_samples];

  size_t path_cap = 4096;
  hkey_t *path = malloc2(path_cap * sizeof(hkey_t));

  for(i = 0; i < num_samples; i++)
  {
    orient = rand() & 0x1;
    node = db_graph_rand_node(&db_graph);

    len = junc = 0;
    path[len++] = node;

    graph_walker_init(&wlk, &db_graph, colour, colour, node, orient);
    lost_nuc = binary_kmer_first_nuc(wlk.bkmer, db_graph.kmer_size);
    prev_junc = edges_get_outdegree(db_graph.col_edges[node], orient) > 1;

    // binary_kmer_to_str(wlk.bkmer, kmer_size, bkmerstr);
    // printf("%3zu %s\n", len, bkmerstr);
    // graph_walker_print_state(&wlk);

    while(graph_traverse(&wlk) && !db_node_has_traversed(visited, wlk.node, wlk.orient))
    {
      db_node_set_traversed(visited, wlk.node, wlk.orient);
      graph_walker_node_add_counter_paths(&wlk, lost_nuc);
      lost_nuc = binary_kmer_first_nuc(wlk.bkmer, db_graph.kmer_size);
      if(len == path_cap) {
        path_cap *= 2;
        path = realloc2(path, path_cap * sizeof(hkey_t));
      }
      path[len++] = wlk.node;
      junc += prev_junc;
      prev_junc = edges_get_outdegree(db_graph.col_edges[wlk.node], wlk.orient) > 1;
      // binary_kmer_to_str(wlk.bkmer, kmer_size, bkmerstr);
      // printf("%3zu %s\n", len, bkmerstr);
    }

    // printf(" len: %zu junc: %zu\n", len, junc);

    graph_walker_finish(&wlk);

    for(j = 0; j < len; j++)
      db_node_fast_clear_traversed(visited, path[j]);

    dead_ends += (edges_get_outdegree(db_graph.col_edges[wlk.node], wlk.orient) == 0);
    lengths[n] = len;
    junctions[n] = junc;
    total_len += len;
    total_junc += junc;
    n++;
  }

  free(path);

  status("\n");
  status("total_len: %zu; total_junc: %zu (%.2f%% junctions)\n",
         total_len, total_junc, (100.0*total_junc)/total_len);
  status("dead ends: %zu / %zu\n", dead_ends, num_samples);
  status("mean length: %.2f\n", (double)total_len / n);
  status("mean junctions: %.1f per contig, %.2f%% nodes (1 every %.1f nodes)\n",
          (double)total_junc / n, (100.0 * total_junc) / total_len,
          (double)total_len / total_junc);

  qsort(lengths, n, sizeof(size_t), cmp_size);
  qsort(junctions, n, sizeof(size_t), cmp_size);

  double median_len = MEDIAN(lengths, n);
  double median_junc = MEDIAN(junctions, n);

  status("Median contig length: %.2f\n", median_len);
  status("Median junctions per contig: %.2f\n", median_junc);

  free(visited);
  free(db_graph.col_edges);
  free(db_graph.node_in_cols);
  free((void*)db_graph.kmer_paths);
  free(path_store);

  graph_header_dealloc(&gheader);
  paths_header_dealloc(&pheader);
  graph_walker_dealloc(&wlk);
  db_graph_dealloc(&db_graph);

  cmd_free(&args);
  return EXIT_SUCCESS;
}