CC ?= cc
CFLAGS ?= -O2
CSV_RANK_BIN ?= /tmp/v2speed-csv-rank

.PHONY: test demo

test:
	./test.sh

demo:
	$(CC) -std=c11 -Wall -Wextra -Werror -pedantic $(CFLAGS) -I include \
	  src/smart_connect.c src/smart_connect_measurements.c \
	  src/smart_connect_probe_plan.c tools/v2speed_csv_rank.c -lm -o $(CSV_RANK_BIN)
	@printf 'Built experimental CSV ranker: %s\n' '$(CSV_RANK_BIN)'
