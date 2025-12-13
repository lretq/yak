#pragma once

#include <yak/status.h>
#include <yak/process.h>

status_t jobctl_setsid(struct kprocess *process);
status_t jobctl_setpgid(struct kprocess *proc, pid_t pgid);

void pgrp_insert(struct pgrp *pgrp, struct kprocess *process);
void session_insert(struct session *session, struct kprocess *process);

void session_remove(struct kprocess *process);
void pgrp_remove(struct kprocess *process);
