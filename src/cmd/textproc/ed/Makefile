# Define the binary name
PROG=	ed

# Point to the source directory so make can find the .c files
.PATH:	${.CURDIR}/src

# List the source files (bsd.prog.mk automatically derives .o names)
SRCS=	main.c \
	buffer.c \
	commands.c \
	parser.c

# Compiler flags
# -Iinclude: Look for your headers in the include directory
# -D_POSIX_C_SOURCE: Required for getline() and strtok_r()
CFLAGS+= -I../../../include/ -Wall -Wextra -std=c99
CFLAGS+= -D_POSIX_C_SOURCE=200809L

MAN=	ed.1

# Optional: Define 'red' as a hard link to 'ed' upon installation
LINKS=	${BINDIR}/ed ${BINDIR}/red

.include <bsd.prog.mk>
