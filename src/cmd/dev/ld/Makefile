PROG=	ld
.PATH:	${.CURDIR}/src

SRCS=	main.cc writer.cc symbols.cc reader.cc

CPPFLAGS+=	-I${.CURDIR}/../../../include

CXXFLAGS+=	-Wall -Wextra -std=c++17

LDADD+=		-lstdc++

# This ensures the build system treats the final link stage correctly
.if defined(HAVE_GCC) || defined(HAVE_LLVM)
CXXFLAGS+=	-fPIE
LDFLAGS+=	-pie
.endif

# NOMAN=yes skips building a manual page if ld.1 doesn't exist yet
NOMAN=		yes

.include <bsd.prog.mk>
