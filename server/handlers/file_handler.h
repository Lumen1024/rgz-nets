#pragma once

#include "../../shared/protocol.h"

Response handle_file_request(const char *to, Request *req, const char *from);
Response handle_file_approve(const char *to, const char *file_id);
Response handle_file_decline(const char *to, const char *file_id);
