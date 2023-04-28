/**
* @file bitflag.h
*
* @brief Common utilities for working with bitflags.
*/

#ifndef BITFLAG_H
#define BITFLAG_H

/**
* @brief Check if a bitflag contains a flag.
* @param bits bitflag to check
* @param flag flag to check for
*/
#define BITFLAG_HAS_FLAG(bits, flag) (((bits) & (flag)) != 0)

/**
* @brief Check if a bitflag contains all flags.
* @param bits bitflag to check
* @param flags flags to check for
*/
#define BITFLAG_HAS_ALL(bits, flags) (((bits) & (flags)) == (flags))

/**
* @brief Add a flag to the bitflag
* @param bits bitflag to add too
* @param flags flags to add
*/
#define BITFLAG_ADD(bits, flag) ((bits) | (flag))

/**
* @brief Remove a flag from the bitflag
* @param bits bitflag to remove from
* @param flags flags to remove
*/
#define BITFLAG_REMOVE(bits, flag) ((bits) & ~(flag))

#endif //BITFLAG_H
