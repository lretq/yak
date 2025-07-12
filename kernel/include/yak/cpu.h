#pragma once

#include <stddef.h>
#include <limits.h>
#include <yak/macro.h>

#define MAX_NR_CPUS 1024ULL

typedef unsigned long cpumask_word_t;

#define CPUMASK_BITS_SIZE \
	DIV_ROUNDUP(MAX_NR_CPUS, sizeof(cpumask_word_t) * CHAR_BIT)

#define CPUMASK_BITS_PER_IDX (sizeof(cpumask_word_t) * CHAR_BIT)

struct cpumask {
	cpumask_word_t bits[CPUMASK_BITS_SIZE];
};

extern struct cpumask cpumask_active;

void cpu_init();
void cpu_up(size_t id);

size_t cpus_online();

#define for_each_cpu(index, mask)                                              \
	for (size_t bits_i_ = 0; bits_i_ < CPUMASK_BITS_SIZE; bits_i_++)       \
		for (cpumask_word_t tmp_var_ = __atomic_load_n(                \
			     &((mask)->bits[bits_i_]), __ATOMIC_RELAXED);      \
		     tmp_var_ != 0 &&                                          \
		     (((index) =                                               \
			       bits_i_ * (sizeof(cpumask_word_t) * CHAR_BIT) + \
			       __builtin_ctzl(tmp_var_)),                      \
		      1);                                                      \
		     tmp_var_ &= (tmp_var_ - 1))
