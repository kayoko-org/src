S=	/usr/src/sys

KMOD=	kat
SRCS=	kmod.c

# Ensure we can find the header in current directory
CPPFLAGS+=	-I${.CURDIR}

.include <bsd.kmodule.mk>
