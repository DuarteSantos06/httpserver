#pragma once

#include "client.h"

void log_audit(struct client *c, const char *reason, const char *bad_path);