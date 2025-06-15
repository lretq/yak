#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <yak/ipl.h>
#include <yak/cpudata.h>

void softint_dispatch(ipl_t ipl);
void softint_issue(ipl_t ipl);
void softint_issue_other(struct cpu *cpu, ipl_t ipl);

#ifdef __cplusplus
}
#endif
