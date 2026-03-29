#ifndef PARSER_H
#define PARSER_H

#include <kayoko/textproc/ed.h>

/* * Returns 1 if an address was found, 0 otherwise. 
 * Updates the string pointer 's' to the next character after the address.
 */
int get_addr(Ed *e, char **s, int *addr);

#endif
