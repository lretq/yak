#pragma once

typedef enum status {
	YAK_SUCCESS = 0,
} status_t;

#define IS_OK(x) ((x) == YAK_SUCCESS)
#define IS_ERR(x) ((x) != YAK_SUCCESS)

#define IF_OK(expr) if (IS_OK((expr)))
#define IF_ERR(expr) if (IS_ERR((expr)))

const char *status_str(unsigned int status);
