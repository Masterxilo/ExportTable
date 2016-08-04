#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

#ifndef _DEBUG
#define NDEBUG
#endif
#include <assert.h>

#include <WolframLibrary.h>

EXTERN_C DLLEXPORT int WolframLibrary_initialize(WolframLibraryData libData) {return 0;}
EXTERN_C DLLEXPORT mint WolframLibrary_getVersion() {return WolframLibraryVersion;}

#define verify(x, err) if (!(x)) return err;

/*
Load with
LibraryFunctionLoad[dll, "ExportTableG", {"UTF8String", {Real, 2, "Constant"}, Integer}, "Void"]

Args[0] : filename
Args[1] : MTensor
Args[2] : DIGITS (17 is enough to represent every double bit by bit http://stackoverflow.com/a/9999374/524504 less use less storage)
*/
EXTERN_C DLLEXPORT int ExportTableG(WolframLibraryData libData, mint Argc, MArgument * Args, MArgument Res) {
    const int DIGITS = MArgument_getInteger(Args[2]);

    // open file
    char* filename = MArgument_getUTF8String(Args[0]);
    FILE* file = fopen(filename, "wb");
    libData->UTF8String_disown(filename);

    verify(file, LIBRARY_FUNCTION_ERROR);

    MTensor T_arg = MArgument_getMTensor(Args[1]);
    verify(libData->MTensor_getRank(T_arg) == 2, LIBRARY_RANK_ERROR);
    verify(libData->MTensor_getType(T_arg) == MType_Real, LIBRARY_TYPE_ERROR);

    mint const* const dims = libData->MTensor_getDimensions(T_arg); // same result as Dimensions, same order
    auto data = libData->MTensor_getRealData(T_arg); // Flatten[] result linearization (i.e. row major, interleaved etc.)

    // write
    for (mint i = 0; i < dims[0]; i++) {
        for (mint j = 0; j < dims[1] - 1; j++) {
            fprintf(file, "%.*g\t", DIGITS, *data++);
        }
        fprintf(file, "%.*g\n", DIGITS, *data++);
    }

    // close
    verify(fclose(file) == 0, LIBRARY_FUNCTION_ERROR);

    return LIBRARY_NO_ERROR;
}


const int dtostr_fast_extra_chars = 1 + 1 + 2 + 3; //  7;
/* Writes exactly
1 (+-) + digits + 1 (.) + 2 (e +/-) + 3 (exponent)
i.e. digits + 7 characters in the format
%e, that is
to str

returns pointer to string after final position
*/
void dtostr_fast(
    double value,
    int digits,
    char *str,
    char** endptr
    );


// writes digits + 1 characters (adds +- sign) with the last character being at last
// pads left with 0
void itoa_backwards_signed(int digits, int i, char* last) {
    bool negative = false;
    if (i < 0) {
        negative = true; i = -i;
    }

    for (; digits; digits--) {
        *last-- = '0' + i % 10;
        i /= 10;
    }
    if (negative) *last = '-';
    else *last = '+';
}

// writes digits + 1 characters, inserting a dot before the final digit
void ulltoa_backwards_dotted(int digits, long long i, char* last) {
    for (; digits>1; digits--) {
        *last-- = '0' + i % 10;
        i /= 10;
    }

    *last-- = '.';

    *last = '0' + i % 10;
}

// ---

template<typename T>
void dftostr_fast(
    T value,
    int digits,
    char *str,
    char** endptr
    ) {
    // special case (log undefined)
    if (value == 0.) {
    give0:
        *str++ = '+';
        *str++ = '0'; digits--;
        *str++ = '.';
        for (; digits; digits--) *str++ = '0';

        *str++ = 'e';
        *str++ = '+';
        *str++ = '0';
        *str++ = '0';
        *str++ = '0';

        // Point past end - don't forget!
        *endptr = str;
        return;
    }

    if (value < 0) {
        value = -value; *str++ = '-';
    }
    else *str++ = '+';


    int EXPONENT = (int)floor(log10(value));
    // TODO handle special cases where pow() cannot be computed because the exponent is too large
    if ((digits - 1) - EXPONENT > 308) goto give0;
    if ((digits - 1) - EXPONENT < -308) goto give0;

    // digits are obtained by making value an integer with digits many digits
    long long MANTISSA = value*pow(10., (digits - 1) - EXPONENT); // is at 0 based position 'EXPONENT', want to get it to 0-based position digits-1

    str += digits;
    ulltoa_backwards_dotted(digits, MANTISSA, str);
    str++;

    // +-EXPONENT
    *str++ = 'e';
    str += 3;
    itoa_backwards_signed(3, EXPONENT, str);
    str++;

    *endptr = str;
}

void dtostr_fast(
    double value,
    int digits,
    char *str,
    char** endptr
    ) {
    dftostr_fast<double>(value, digits, str, endptr);
}

// ---

#define NOMINMAX
#define NOGDI
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// using WINAPI and dtostr_fast
// using ppl optionally
#include <ppl.h>
using namespace concurrency;

/*
Load with
LibraryFunctionLoad[dll, "ExportTableFixedE", {"UTF8String", {Real, 2, "Constant"}, Integer, Integer}, "Void"]

Args[0] : filename
Args[1] : MTensor
Args[2] : DIGITS (17 is enough to represent every double bit by bit http://stackoverflow.com/a/9999374/524504 less use less storage)
Args[3] : usePPL (parallelize) 0 == false, 1 == true
*/
EXTERN_C DLLEXPORT int ExportTableFixedE(WolframLibraryData libData, mint Argc, MArgument * Args, MArgument Res) {
    // Options
    const int DIGITS = MArgument_getInteger(Args[2]);
    verify(DIGITS > 0 && DIGITS <= 17, LIBRARY_TYPE_ERROR);
    verify(MArgument_getInteger(Args[3]) == 0 || MArgument_getInteger(Args[3]) == 1, LIBRARY_TYPE_ERROR);
    const bool usePPL = MArgument_getInteger(Args[3]) == 1;

    const MTensor T_arg = MArgument_getMTensor(Args[1]);
    verify(libData->MTensor_getRank(T_arg) == 2, LIBRARY_RANK_ERROR);
    verify(libData->MTensor_getType(T_arg) == MType_Real, LIBRARY_TYPE_ERROR);

    mint const* const dims = libData->MTensor_getDimensions(T_arg); // same result as Dimensions, same order
    const mreal* const allData = libData->MTensor_getRealData(T_arg); // Flatten[] result linearization (i.e. row major, interleaved etc.)

    // open memory mapped file 

    const char* const filename = MArgument_getUTF8String(Args[0]);

#define FILE_SHARE_NONE 0
    auto hFile = CreateFileA(
        filename,
        (GENERIC_READ | GENERIC_WRITE),
        FILE_SHARE_NONE,
        0,
        OPEN_ALWAYS, /* OPEN_ALWAYS can be faster than CREATE_ALWAYS (storage reused?), but external programs seem to get confused with the modified data sometimes: 0.02859 s vs 0.04 s*/
        0,
        0);

    libData->UTF8String_disown((char*)filename);

    verify(hFile != INVALID_HANDLE_VALUE, LIBRARY_FUNCTION_ERROR);

    // compute and set exact necessary size
    const int CHARS_PER_DOUBLE = DIGITS + dtostr_fast_extra_chars + 1; // dtostr_fast 17 DIGITS + dtostr_fast_extra_chars and 1 \t or \n after each one
    const LONG size = dims[0] * dims[1] * CHARS_PER_DOUBLE;

    auto hMapping = CreateFileMappingA(
        hFile,
        0,
        PAGE_READWRITE,
        0,
        size,
        0
        );

    verify(hMapping != INVALID_HANDLE_VALUE, LIBRARY_FUNCTION_ERROR);

    char *const mappedData = (char*)MapViewOfFile(hMapping,
        FILE_MAP_WRITE,
        0, 0,
        0 //  the mapping extends from the specified offset to the end of the file mapping.
        );

    verify(mappedData, LIBRARY_FUNCTION_ERROR);

    // write

    // write current double pointed by data to file starting at e
#define WRITE_DATA() dtostr_fast(*data++, DIGITS, e, &e) /* exactly DIGITS+dtostr_fast_extra_chars characters */

    // write current row starting from e and data
#define WRITE_ROW() \
    const char* const olde = e;\
    for (mint j = 0; j < dims[1] - 1; j++) {\
        WRITE_DATA(); *e++ = '\t';\
    }\
    WRITE_DATA(); *e++ = '\n'; \
    assert(e == olde+dims[1]*CHARS_PER_DOUBLE);


    if (usePPL) {
        parallel_for((mint)0, dims[0], [&](const mint i) { // equivalently: for (mint i = 0; i < dims[0]; i++) {
            // compute pointers for row i
            char* e = mappedData + dims[1] * CHARS_PER_DOUBLE * i;
            const double* data = allData + dims[1] * i;

            WRITE_ROW();
        }
        );
    } else {
        const double* data = allData;
        char* e = mappedData;
        for (mint i = 0; i < dims[0]; i++) {
            WRITE_ROW(); // e and data are carried along
        }

        assert(e == mappedData + size);
    }

    // close
    verify(UnmapViewOfFile(mappedData), LIBRARY_FUNCTION_ERROR);
    verify(CloseHandle(hMapping), LIBRARY_FUNCTION_ERROR);
    verify(CloseHandle(hFile), LIBRARY_FUNCTION_ERROR);

    return LIBRARY_NO_ERROR;
}