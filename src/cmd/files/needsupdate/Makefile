# Define the binary name
PROG=	needsupdate

# Point to the source directory so make can find the .c files
.PATH:	${.CURDIR}/src

# List the source files (bsd.prog.mk automatically derives .o names)
SRCS=	needsupdate.c

# Compiler flags
# -Iinclude: Look for your headers in the include directory
# -D_POSIX_C_SOURCE: Required for getline() and strtok_r()
CFLAGS+= -I../../../include/ -Wall -Wextra -std=c99
CFLAGS+= -D_POSIX_C_SOURCE=200809L

MAN=	needsupdate.1

.include <bsd.prog.mk>
