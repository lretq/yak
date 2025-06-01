#pragma once

#include <yak/ipl.h>
#include <yak/cpudata.h>

void softint_dispatch(ipl_t ipl);
void softint_issue(ipl_t ipl);
void softint_issue_other(struct cpu *cpu, ipl_t ipl);
