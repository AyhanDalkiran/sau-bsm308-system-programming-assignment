#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <inttypes.h>

/* Custom error codes */
// Upper 8 bits for category, lower 8 bits for specific error
typedef enum {
    ERROR_NONE = 0x0000,

    // I/O errors
    ERROR_IO = 0x0100,
    ERROR_IO_INPUT_OVERFLOW = 0x0101,

    // Command-line argument parsing errors
    ERROR_ARGV = 0x0200,
    ERROR_ARGV_INVALID = 0x0201,
    ERROR_ARGV_FILENAME_MISSING = 0x0202,
    ERROR_ARGV_FILENAME_DUPLICATE = 0x0203,
    ERROR_ARGV_DIMENSION_MISSING = 0x0204,
    ERROR_ARGV_DIMENSION_DUPLICATE = 0x0205,
    ERROR_ARGV_DIMENSION_INVALID = 0x0206,
    ERROR_ARGV_DIMENSION_OUT_OF_RANGE = 0x0207,

    // Conversion errors
    ERROR_CONVERSION = 0x0300,
    ERROR_CONVERSION_INVALID = 0x0301,
    ERROR_CONVERSION_OUT_OF_RANGE = 0x0302,
    ERROR_CONVERSION_INVALID_BASE = 0x0303,

    // Memory allocation errors
    ERROR_ALLOCATION = 0x0400,

    // Math errors
    ERROR_MATH = 0x0500,
    ERROR_MUL_OVERFLOW = 0x0501
} error_code_t;
/* End of custom error codes */

static int INT_STRING_DIGITS_MAX = 0; // Not defined until main function is called, due to C standard reasons

/* 
Converts `n_str` null terminated string to int with the specified `base` and writes the result to `n_out`.
If `base` is 0, the base is auto-detected by `strtol`.
Returns `error_code_t` indicating success (`ERROR_NONE`) or the type of error that occurred.
`n_out` is only modified if the conversion is successful.

Error codes:
`ERROR_CONVERSION_INVALID`: No digits were found
`ERROR_CONVERSION_OUT_OF_RANGE`: The number is out of range for `int`
`ERROR_CONVERSION_INVALID_BASE`: `base` is not 0 nor between 2 and 36
*/
error_code_t string_to_int(char* n_str, int* n_out, int base) {
    if (base != 0 && (base < 2 || base > 36)) return ERROR_CONVERSION_INVALID_BASE; // Invalid base error
    
    // Convert string to long
    char* n_str_end = n_str;
    errno = 0;
    const long n_long = strtol(n_str, &n_str_end, base);

    if (errno == ERANGE || n_long < INT_MIN || n_long > INT_MAX) return ERROR_CONVERSION_OUT_OF_RANGE; // Number is out of range
    if (n_long == 0 && n_str_end == n_str) return ERROR_CONVERSION_INVALID; // No digits were found

    *n_out = (int)n_long; // Cast to int and store in output variable

    return ERROR_NONE; // Success
}

/* 
Multiplies two `size_t` values and stores the result in `result_out`.
Returns `error_code_t` indicating success (`ERROR_NONE`) or the type of error that occurred.

Error codes:
`ERROR_MUL_OVERFLOW`: The multiplication of `a` and `b` would overflow the `size_t` type.
*/
error_code_t mul_size_t(size_t a, size_t b, size_t* result_out) {
    if (a > SIZE_MAX / b) return ERROR_MUL_OVERFLOW; // Multiplication overflow

    *result_out = a * b;
    return ERROR_NONE; // Success
}
/* Stream utilities */

/*
Reads int from `stream` into `n_out`.
Returns `error_code_t` indicating success (`ERROR_NONE`) or the type of error that occurred.
`n_out` is only modified if the conversion is successful.

Error codes:
`ERROR_IO`: An I/O error occurred while doing I/O operations on the stream
`ERROR_IO_INPUT_OVERFLOW`: The input size is too large to be represented in the buffer
`ERROR_ALLOCATION`: Memory allocation for the buffer failed
`ERROR_CONVERSION_INVALID`: No digits were found in the input
`ERROR_CONVERSION_OUT_OF_RANGE`: The number is out of range for `int`
*/
error_code_t stream_read_int(FILE* stream, int* n_out) {
    int c;
    // Skip until the first non-whitespace character
    while (isspace(c = fgetc(stream))) continue;
    if (feof(stream)) return ERROR_CONVERSION_INVALID; // End of file reached before finding a non-whitespace character
    if (ferror(stream)) return ERROR_IO; // An I/O error occurred while reading from the stream
    if (ungetc(c, stream) == EOF) return ERROR_IO; // An I/O error occurred while ungetting the character

    // Write input to buffer
    char* buffer = malloc((INT_STRING_DIGITS_MAX + 1) * sizeof(char));
    if (buffer == NULL) return ERROR_ALLOCATION; // Memory allocation error

    int str_i = 0;
    while (isgraph(c = fgetc(stream)) && str_i < INT_STRING_DIGITS_MAX) {
        *(buffer + str_i) = (char)c; // Store character in buffer
        str_i++;
    }

    if (ferror(stream)) {
        free(buffer); // Free allocated memory on error
        return ERROR_IO; // An I/O error occurred while reading from the stream
    }
    if (isgraph(c)) {
        free(buffer); // Free allocated memory on error
        return ERROR_IO_INPUT_OVERFLOW; // Input size is too large to be represented in the buffer
    }
    if (!feof(stream) && ungetc(c, stream) == EOF) {
        free(buffer); // Free allocated memory on error
        return ERROR_IO; // An I/O error occurred while ungetting the character
    }

    *(buffer + str_i) = '\0'; // Null-terminate the buffer

    int n;
    const error_code_t error_result = string_to_int(buffer, &n, 0); // Convert buffer to int
    free(buffer);
    if (error_result != ERROR_NONE) return error_result; // Propagate conversion error

    *n_out = n; // Store the converted integer in the output variable

    return ERROR_NONE; // Success
}

/*
Discards characters from `stream` until the specified `delimiter` is found or end of file is reached.
Returns `error_code_t` indicating success (`ERROR_NONE`) or the type of error that occurred.

Error codes:
`ERROR_IO`: An I/O error occurred while reading from the stream
*/
error_code_t stream_skip_until(FILE* stream, int delimiter) {
    int c = fgetc(stream);
    while (c != EOF && c != delimiter) c = fgetc(stream);

    if (ferror(stream)) return ERROR_IO; // An I/O error occurred while reading from the stream
    else return ERROR_NONE; // End of file reached without finding the specified character, but no I/O error occurred
}

/* Argv parameter handling functions */

/* 
Reads a positive integer from standard input and stores it in `dimension_out`.
Returns `error_code_t` indicating success (`ERROR_NONE`) or the type of error that occurred.
`dimension_out` is only modified if the input is successfully read and valid.
Error codes:
`ERROR_IO`: An I/O error occurred while skipping to end of input.
*/
error_code_t user_read_dimension(int* dimension_out) {
    int dimension = 0;
    error_code_t error_result = ERROR_NONE;

    while (dimension <= 0) {
        fprintf(stdout, "Enter the dimension of the matrix (positive integer): ");
        error_result = stream_read_int(stdin, &dimension);

        // Error handling for reading dimension from user input
        if (error_result != ERROR_NONE) {
            if (error_result == ERROR_IO) fprintf(stderr, "error: an I/O error occurred while reading input\n");
            else if (error_result == ERROR_IO_INPUT_OVERFLOW) fprintf(stderr, "error: potential number is too long\n");
            else if (error_result == ERROR_ALLOCATION) fprintf(stderr, "error: failed to allocate memory for input string while reading input\n");
            else if (error_result == ERROR_CONVERSION_INVALID) fprintf(stderr, "error: dimension is not a valid integer\n");
            else if (error_result == ERROR_CONVERSION_OUT_OF_RANGE) fprintf(stderr, "error: dimension is not within range for int type\n");
            else fprintf(stderr, "error: unhandled error code: 0x%08" PRIxLEAST32 "\n", error_result);
        }
        // Additional check for dimension being a positive integer
        else if (dimension <= 0) fprintf(stderr, "error: dimension is not within range [1, %d]\n", INT_MAX);

        error_result = stream_skip_until(stdin, '\n');
        if (error_result != ERROR_NONE) {
            if (error_result == ERROR_IO) fprintf(stderr, "fatal error: an I/O error occurred while skipping to end of the stream\n");
            else fprintf(stderr, "fatal error: unhandled error code: 0x%08" PRIxLEAST32 "\n", error_result);

            return error_result; // Not being able to skip input breaks console input a lot so it is better to terminate
        }
    }

    *dimension_out = dimension;
    return ERROR_NONE; // Success
}

/*
Reads command-line arguments from `argv` with length `argc` and writes the filename and dimension to `filename_out` and `dimension_out`, respectively.
Returns `error_code_t` indicating success (`ERROR_NONE`) or the type of error that occurred
`filename_out` and `dimension_out` may only be modified if the arguments are successfully parsed and present in `argv`.

Error codes:
`ERROR_ARGV_INVALID`: Unrecognized command-line argument
`ERROR_ARGV_FILENAME_MISSING`: Missing filename after -f argument
`ERROR_ARGV_FILENAME_DUPLICATE`: Multiple -f arguments provided
`ERROR_ARGV_DIMENSION_MISSING`: Missing dimension after -d argument
`ERROR_ARGV_DIMENSION_DUPLICATE`: Multiple -d arguments provided
`ERROR_ARGV_DIMENSION_INVALID`: Dimension is not a valid integer
`ERROR_ARGV_DIMENSION_OUT_OF_RANGE`: Dimension is not within range [1, INT_MAX]
*/
error_code_t read_args(int argc, char** argv, char** filename_out, int* dimension_out) {
    char* filename = NULL;
    int dimension = 0;

    // Processing command-line arguments
    for (int argc_i = 1; argc_i < argc; argc_i++) {
        char* arg = *(argv + argc_i);

        if (strcmp(arg, "-f") == 0) { // File argument
            if (filename != NULL) return ERROR_ARGV_FILENAME_DUPLICATE; // Multiple -f arguments provided

            argc_i++;
            filename = *(argv + argc_i);
            if (filename == NULL) return ERROR_ARGV_FILENAME_MISSING; // Missing filename after -f argument
        }
        else if (strcmp(arg, "-d") == 0) { // Dimension argument
            if (dimension != 0) return ERROR_ARGV_DIMENSION_DUPLICATE; // Multiple -d arguments provided

            argc_i++;
            char* dimension_str = *(argv + argc_i);
            if (dimension_str == NULL) return ERROR_ARGV_DIMENSION_MISSING; // Missing dimension after -d argument
            const error_code_t error_result = string_to_int(dimension_str, &dimension, 0);

            if (error_result == ERROR_CONVERSION_INVALID) return ERROR_ARGV_DIMENSION_INVALID;
            if (error_result == ERROR_CONVERSION_OUT_OF_RANGE) return ERROR_ARGV_DIMENSION_OUT_OF_RANGE;
            if (dimension <= 0) return ERROR_ARGV_DIMENSION_OUT_OF_RANGE; // Dimension argument must be a positive integer
        }
        else {
            return ERROR_ARGV_INVALID; // Unrecognized argument
        }
    }

    *filename_out = filename;
    *dimension_out = dimension;

    return ERROR_NONE; // Success
}

/*
Computer the sum of primary and secondary diagonals of the square matrix `matrix` with dimensions `rows` x `cols`.
Returns the computed sum.
*/
int special_sum(int* matrix, int rows, int cols) {
    int sum = 0;

    for (int r = 0; r < rows; r++) {
        int* left_ptr = matrix + (r * cols) + r; // Primary diagonal element
        int* right_ptr = matrix + (r * cols) + (cols - 1 - r); // Secondary diagonal element

        sum += *left_ptr;
        // If not the center element, add the secondary diagonal element
        if (left_ptr != right_ptr) sum += *right_ptr;
    }

    return sum;
}

int main(int argc, char** argv) {
    // Define maximum buffer size for reading input strings based on the number of digits in INT_MIN
    INT_STRING_DIGITS_MAX = snprintf(NULL, 0, "%d", INT_MIN);

    error_code_t error_result = ERROR_NONE;

    // Read command-line arguments
    int dimension = 0;
    char* filename = NULL;
    error_result = read_args(argc, argv, &filename, &dimension);
    if (error_result != ERROR_NONE) { // Error handling for command-line argument parsing
        if (error_result == ERROR_ARGV_INVALID) fprintf(stderr, "fatal error: unrecognized command-line argument\n");
        else if (error_result == ERROR_ARGV_FILENAME_MISSING) fprintf(stderr, "fatal error: missing filename after -f argument\n");
        else if (error_result == ERROR_ARGV_FILENAME_DUPLICATE) fprintf(stderr, "fatal error: multiple -f arguments provided\n");
        else if (error_result == ERROR_ARGV_DIMENSION_MISSING) fprintf(stderr, "fatal error: missing dimension after -d argument\n");
        else if (error_result == ERROR_ARGV_DIMENSION_DUPLICATE) fprintf(stderr, "fatal error: multiple -d arguments provided\n");
        else if (error_result == ERROR_ARGV_DIMENSION_INVALID) fprintf(stderr, "fatal error: dimension is not a valid integer\n");
        else if (error_result == ERROR_ARGV_DIMENSION_OUT_OF_RANGE) fprintf(stderr, "fatal error: dimension is not within range [1, %d]\n", INT_MAX);
        else fprintf(stderr, "fatal error: unhandled error code: 0x%08" PRIxLEAST32 "\n", error_result);

        return EXIT_FAILURE;
    }

    // If dimension is not provided through command-line arguments, prompt the user for it
    if (dimension == 0) {
        error_result = user_read_dimension(&dimension);
        if (error_result != ERROR_NONE) { // Error handling for reading dimension from user input
            if (error_result == ERROR_IO) fprintf(stderr, "fatal error: an I/O error occurred while skipping to end of input\n");
            else fprintf(stderr, "fatal error: unhandled error code: 0x%08" PRIxLEAST32 "\n", error_result);

            return EXIT_FAILURE;
        }
    }

    // Multiplication overflow checks for allocation
    size_t element_count = 0;
    size_t matrix_size = 0;
    if (mul_size_t((size_t)dimension, (size_t)dimension, &element_count) != ERROR_NONE) {
        fprintf(stderr, " fatal error: dimension is too large for matrix to be stored in memory\n");
        return EXIT_FAILURE;
    }
    else if (mul_size_t(element_count, sizeof(int), &matrix_size) != ERROR_NONE) {
        fprintf(stderr, "fatal error: dimension is too large for matrix to be stored in memory\n");
        return EXIT_FAILURE;
    }

    // Allocating matrix
    int* matrix = (int*)malloc(matrix_size);
    if (matrix == NULL) {
        fprintf(stderr, "fatal error: failed to allocate memory for matrix\n");
        return EXIT_FAILURE;
    }

    // If filename is provided, read the matrix from the file
    if (filename != NULL) {
        FILE* file = fopen(filename, "r");
        if (file == NULL) {
            fprintf(stderr, "error: failed to open file '%s'\n", filename);
            free(matrix);
            return EXIT_FAILURE;
        }

        // Reading elements from file
        for (size_t i = 0; i < element_count; i++) {
            error_result = stream_read_int(file, (matrix + i));
            if (error_result != ERROR_NONE) {
                const size_t element = i + 1, row = (i / dimension) + 1, column = (i % dimension) + 1;

                if (error_result == ERROR_IO) fprintf(stderr, "fatal error: an I/O error occurred while reading from the file (element %zu, row %zu, column %zu)\n", element, row, column);
                else if (error_result == ERROR_IO_INPUT_OVERFLOW) fprintf(stderr, "fatal error: potential number in file is too long (element %zu, row %zu, column %zu)\n", element, row, column);
                else if (error_result == ERROR_ALLOCATION) fprintf(stderr, "fatal error: failed to allocate memory for input string while reading from file (element %zu, row %zu, column %zu)\n", element, row, column);
                else if (error_result == ERROR_CONVERSION_INVALID) fprintf(stderr, "fatal error: matrix element in file is not a valid integer (element %zu, row %zu, column %zu)\n", element, row, column);
                else if (error_result == ERROR_CONVERSION_OUT_OF_RANGE) fprintf(stderr, "fatal error: matrix element in file is out of range for int type (element %zu, row %zu, column %zu)\n", element, row, column);
                else fprintf(stderr, "fatal error: unhandled error code: 0x%08" PRIxLEAST32 " (element %zu, row %zu, column %zu)\n", error_result, element, row, column);

                free(matrix);
                fclose(file);
                return EXIT_FAILURE;
            }
        }

        fclose(file);
    }
    else { // Read elements from user
        size_t i = 0;

        do {
            const size_t element = i + 1, row = (i / dimension) + 1, column = (i % dimension) + 1;
            fprintf(stdout, "Enter element %zu (row %zu, column %zu): ", element, row, column);

            error_result = stream_read_int(stdin, (matrix + i));
            if (error_result != ERROR_NONE) {
                if (error_result == ERROR_IO) fprintf(stderr, "error: an I/O error occurred while reading input\n");
                else if (error_result == ERROR_IO_INPUT_OVERFLOW) fprintf(stderr, "error: input size is too large\n");
                else if (error_result == ERROR_ALLOCATION) fprintf(stderr, "error: failed to allocate memory for input string while reading input\n");
                else if (error_result == ERROR_CONVERSION_INVALID) fprintf(stderr, "error: input is not a valid integer\n");
                else if (error_result == ERROR_CONVERSION_OUT_OF_RANGE) fprintf(stderr, "error: input is out of range for int type\n");
                else fprintf(stderr, "error: unhandled error code: 0x%08" PRIxLEAST32 "\n", error_result);
            }
            else i++;

            error_result = stream_skip_until(stdin, '\n');
            if (error_result != ERROR_NONE) {
                if (error_result == ERROR_IO) fprintf(stderr, "fatal error: an I/O error occurred while skipping to end of the stream\n");
                else fprintf(stderr, "fatal error: unhandled error code: 0x%08" PRIxLEAST32 "\n", error_result);

                return EXIT_FAILURE; // Not being able to skip input breaks console input a lot so it is better to terminate
            }
        } while (i < element_count);
    }

    // Print special sum of the matrix
    const int sum = special_sum(matrix, dimension, dimension);
    fprintf(stdout,"Special sum of the matrix: %d\n", sum);

    // Free allocated memory for the matrix
    free(matrix);

    return EXIT_SUCCESS;
}
