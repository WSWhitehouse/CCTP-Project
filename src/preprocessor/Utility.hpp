#ifndef SNOWFLAKE_UTILITY_HPP
#define SNOWFLAKE_UTILITY_HPP

/** @brief Stringify the passed in argument */
#define STRINGIFY(arg) #arg

/** @brief Glue two variables together. Needed for macro expansion. */
#define GLUE(X,Y) GLUE_I(X,Y)
#define GLUE_I(X,Y) X##Y

/** @brief Trim arguments to 32 */
#define TRIM_ARGS_32(X1,  X2,  X3,  X4,  X5,  X6,  X7,  X8,  X9,  X10, X11, X12, X13, X14, X15, X16,      \
	                   X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32, ...) \
	                   X1,  X2,  X3,  X4,  X5,  X6,  X7,  X8,  X9,  X10, X11, X12, X13, X14, X15, X16,      \
                     X17, X18, X19, X20, X21, X22, X23, X24, X25, X26, X27, X28, X29, X30, X31, X32

/**
 * @brief Expands out the VA Arguments. Needed on MSVC compilers
 * as it handles the `__VA_ARGS__` macro very weirdly...
 */
#define VA_ARGS_EXPAND(...) __VA_ARGS__

/** @brief Make an initaliser list from a set of variadic arguments */
#define MAKE_INITIALISER_LIST(...) { __VA_ARGS__ }

/**
 * @brief Count of arguments in `__VA_ARGS__`.
 * Counts up to 60 arguments.
 */
#define VA_ARGS_COUNT(...) VA_ARGS_EXPAND(VA_ARGS_COUNT_I(__VA_ARGS__,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,))
#define VA_ARGS_COUNT_I(_,_59,_58,_57,_56,_55,_54,_53,_52,_51,_50,_49,_48,_47,_46,_45,_44,_43,_42,_41,_40,_39,_38,_37,_36,_35,_34,_33,_32,_31,_30,_29,_28,_27,_26,_25,_24,_23,_22,_21,_20,_19,_18,_17,_16,_15,_14,_13,_12,_11,_10,_9,_8,_7,_6,_5,_4,_3,_2,X_,...) X_

/**
* @brief Get the first argument in a list of variadic arguments.
*/
#define FIRST_ARG(...) VA_ARGS_EXPAND(FIRST_ARG_I(__VA_ARGS__,))
#define FIRST_ARG_I(X,...) X

/**
* @brief Removes the first argument from the `__VA_ARGS__` list.
* This is the opposite of the `FIRST_ARG` macro above.
*/
#define TUPLE_TAIL_ARGS(...) VA_ARGS_EXPAND(TUPLE_TAIL_ARGS_I(__VA_ARGS__))
#define TUPLE_TAIL_ARGS_I(X,...) __VA_ARGS__

#endif //SNOWFLAKE_UTILITY_HPP
