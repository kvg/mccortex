SHELL:=/bin/bash -euo pipefail

CTXDIR=../../..
CTX=$(CTXDIR)/bin/ctx$(K)
DNACAT=$(CTXDIR)/libs/seq_file/bin/dnacat
SIMMUT=$(CTXDIR)/libs/bioinf-perl/sim_mutations/sim_mutations.pl
READSIM=$(CTXDIR)/libs/readsim/readsim
SIMCHECK=$(CTXDIR)/libs/bioinf-perl/sim_mutations/sim_corrected.pl
K=31

REF=$(CTXDIR)/results/data/chr22/chr22_17M_18M.fa
PHIX=$(CTXDIR)/results/data/PhiX/PhiX.1.fq.gz

# simluating a diploid, actual depth 2*$(HAPDEPTH)
HAPDEPTH=25
READLEN=250

READS=reads/chrom0.$(HAPDEPTH)X.fa.gz reads/chrom1.$(HAPDEPTH)X.fa.gz
CORREADS=correct/chrom0.$(HAPDEPTH)X.fa.gz correct/chrom1.$(HAPDEPTH)X.fa.gz
CORREADS_PATHS=correct/chrom0.$(HAPDEPTH)X.paths.fa.gz correct/chrom1.$(HAPDEPTH)X.paths.fa.gz

TGTS=diploid/genome0.fa diploid/genome1.fa \
     diploid/chrom0.fa diploid/chrom1.fa \
     $(READS) $(CORREADS) $(CORREADS_PATHS) \
     k$(K)/graph.k$(K).ctx k$(K)/cleaned.k$(K).ctx

DIRS=k$(K) correct reads diploid logs

all: $(TGTS) test

# Generate a diploid genome
diploid/genome1.fa: diploid/genome0.fa
diploid/genome0.fa: $(REF) | $(DIRS)
	$(SIMMUT) --snps 1000 --indels 10 --invs 0 diploid 2 $<

# Remove deletion marks (-) and convert to uppercase
diploid/chrom%.fa: diploid/genome%.fa
	cat $< | tr -d '-' | $(DNACAT) -u -F - > $@

# Simulate PE reads of each chrom each 50X
reads/chrom%.$(HAPDEPTH)X.fa.gz: diploid/chrom%.fa | $(DIRS)
	$(READSIM) -l $(READLEN) -r $< -d $(HAPDEPTH) -p $(PHIX) -s reads/chrom$*.$(HAPDEPTH)X

# Cortex build k=$(K)
k$(K)/graph.k$(K).ctx: $(READS) | $(DIRS)
	$(CTX) build -m 100M -k $(K) --sample chr22_17M_18M --seq reads/chrom0.$(HAPDEPTH)X.fa.gz --seq reads/chrom1.$(HAPDEPTH)X.fa.gz $@

# Cortex clean
k$(K)/cleaned.k$(K).ctx: k$(K)/graph.k$(K).ctx
	$(CTX) clean -m 100M --covg-before logs/cleaning-covg-before.csv --covg-after logs/cleaning-covg-after.csv --len-before logs/cleaning-len-before.csv --len-after logs/cleaning-len-after.csv --out $@ $<

# Cortex thread reads
k$(K)/cleaned.k$(K).ctp.gz: k$(K)/cleaned.k$(K).ctx $(READS) | $(DIRS)
	$(CTX) thread -m 100M --seq reads/chrom0.$(HAPDEPTH)X.fa.gz --seq reads/chrom1.$(HAPDEPTH)X.fa.gz -o $@ $<

# Cortex correct
correct/chrom1.$(HAPDEPTH)X.fa.gz: correct/chrom0.$(HAPDEPTH)X.fa.gz
correct/chrom0.$(HAPDEPTH)X.fa.gz: k$(K)/cleaned.k$(K).ctx $(READS) | $(DIRS)
	$(CTX) correct -m 100M --format FASTA --seq reads/chrom0.$(HAPDEPTH)X.fa.gz:correct/chrom0.$(HAPDEPTH)X --seq reads/chrom1.$(HAPDEPTH)X.fa.gz:correct/chrom1.$(HAPDEPTH)X k$(K)/cleaned.k$(K).ctx

correct/chrom1.$(HAPDEPTH)X.paths.fa.gz: correct/chrom0.$(HAPDEPTH)X.fa.gz
correct/chrom0.$(HAPDEPTH)X.paths.fa.gz: k$(K)/cleaned.k$(K).ctx k$(K)/cleaned.k$(K).ctp.gz $(READS) | $(DIRS)
	$(CTX) correct -m 100M --format FASTA --seq reads/chrom0.$(HAPDEPTH)X.fa.gz:correct/chrom0.$(HAPDEPTH)X.paths --seq reads/chrom1.$(HAPDEPTH)X.fa.gz:correct/chrom1.$(HAPDEPTH)X.paths -p k$(K)/cleaned.k$(K).ctp.gz k$(K)/cleaned.k$(K).ctx

$(DIRS):
	mkdir -p $@

test: $(READS) $(CORREADS) $(CORREADS_PATHS)
	gzip -dc $(READS)          | $(SIMCHECK) - diploid/chrom{0,1}.fa
	gzip -dc $(CORREADS)       | $(SIMCHECK) - diploid/chrom{0,1}.fa
	gzip -dc $(CORREADS_PATHS) | $(SIMCHECK) - diploid/chrom{0,1}.fa

clean:
	rm -rf $(TGTS) $(DIRS)

.PHONY: all clean
