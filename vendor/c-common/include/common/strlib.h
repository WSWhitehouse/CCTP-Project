/**
 * @file strlib.h
 * 
 * @brief Interface to manipulate strings (char*). None of the strlib functions
 * check for a valid string, the caller should check for null before invoking a 
 * strlib function.
 */

#ifndef STRLIB_H
#define STRLIB_H

#include "common/types.h"
#include "common/defines.h"

/**
 * @brief Gets the length of the string
 * @param str the string whose length to obtain
 * @return u64 length of the given string
 */
u64 str_length(const char* str);

/**
 * @brief Duplicates given string. Warning: this allocates new
 * memory that should be freed by the caller.
 * @param str the string to duplicate
 * @return char* the duplicated string
 */
char* str_duplicate(const char* str);

/**
 * @brief Returns -1 if string str0 comes before str1 in
 * lexicographic order, 0 if they are equal, and +1 if str0
 * comes after str1.
 * @param str0 first string to be compared
 * @param str1 second string to be compared
 * @return i32 returns -1 if string str0 comes before str1 in
 * lexicographic order, 0 if they are equal, and +1 if str0
 * comes after str1
 */
i32 str_compare(const char* str0, const char* str1);

/**
 * @brief Case-sensitive string compare.
 * @param str0 first string to be compared
 * @param str1 second string to be compared
 * @return b8 true if strings are the same, otherwise false
 */
b8 str_equal(const char* str0, const char* str1);

/**
 * @brief Case-insensitive string compare.
 * @param str0 first string to be compared
 * @param str1 second string to be compared
 * @return b8 true if strings are the same, otherwise false
 */
b8 str_equal_ignore_case(const char* str0, const char* str1);

/**
 * @brief Empties the given string.
 * @param str string to empty
 * @return char* pointer to emptied string
 */
static INLINE char* str_empty(char* str) { str[0] = 0; return str; }

/**
 * @brief Copies the src string to the dest.
 * @param dest destination pointer of copy
 * @param src source pointer of copy
 */
void str_copy(char* dest, const char* src);

/**
 * @brief In-place trim of provided string. Removes all
 * whitespace from start and end of the string. Done by 
 * placing zeros in the string at relevant points. 
 * @param str string to be trimmed
 * @return char* pointer to the trimmed string
 */
char* str_trim(char* str);

/**
 * @brief Get the index of the first occuring char in 
 * the string. Returns -1 if the char doesnt exist.
 * @param str the string to be scanned
 * @param c the char to search for
 * @return i32 index of char in str; -1 if not found
 */
i32 str_index_of(char* str, char c);

/**
 * @brief Get the index of the last occuring char in 
 * the string. Returns -1 if the char doesnt exist.
 * @param str the string to be scanned
 * @param c the char to search for
 * @return i32 index of char in str; -1 if not found
 */
i32 str_index_of_last(char* str, char c);

/**
 * @brief Converts string to all uppercase
 * @param str string to convert
 */
void str_to_upper(char* str);

/**
 * @brief Converts string to all lowercase
 * @param str string to convert
 */
void str_to_lower(char* str);

#endif // STRLIB_H
