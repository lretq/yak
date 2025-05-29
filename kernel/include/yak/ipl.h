#pragma once

#include <yak/arch-ipl.h>

typedef int ipl_t;

// raise ipl
ipl_t ripl(ipl_t ipl);
// restore ipl
void xipl(ipl_t ipl);

ipl_t curipl();
void setipl(ipl_t ipl);
