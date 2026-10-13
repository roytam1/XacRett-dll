#ifndef _RAR_COMPRESS_
#define _RAR_COMPRESS_

// Combine pack and unpack constants to class to avoid polluting global
// namespace with numerous short names.
class PackDef
{
  public:
    // Maximum LZ match length we can encode even for short distances.
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { MAX_LZ_MATCH = 0x1001 };
        #else
        static const uint MAX_LZ_MATCH = 0x1001;
        #endif /* VC6 */

    // We increment LZ match length for longer distances, because shortest
    // matches are not allowed for them. Maximum length increment is 3
    // for distances larger than 256KB (0x40000). Here we define the maximum
    // incremented LZ match. Normally packer does not use it, but we must be
    // ready to process it in corrupt archives.
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { MAX_INC_LZ_MATCH = MAX_LZ_MATCH + 3 };
        #else
        static const uint MAX_INC_LZ_MATCH = MAX_LZ_MATCH + 3;
        #endif /* VC6 */

    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { MAX3_LZ_MATCH = 0x101 };// Maximum match length for RAR v3.
        #else
        static const uint MAX3_LZ_MATCH = 0x101;// Maximum match length for RAR v3.
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { MAX3_INC_LZ_MATCH = MAX3_LZ_MATCH + 3 };
        #else
        static const uint MAX3_INC_LZ_MATCH = MAX3_LZ_MATCH + 3;
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { LOW_DIST_REP_COUNT = 16 };
        #else
        static const uint LOW_DIST_REP_COUNT = 16;
        #endif /* VC6 */

    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { NC = 306 };/* alphabet = {0, 1, 2, ..., NC - 1} */
        #else
        static const uint NC = 306;/* alphabet = {0, 1, 2, ..., NC - 1} */
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { DCB = 64 };// Base distance codes up to 4 GB.
        #else
        static const uint DCB = 64;// Base distance codes up to 4 GB.
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { DCX = 80 };// Extended distance codes up to 1 TB.
        #else
        static const uint DCX = 80;// Extended distance codes up to 1 TB.
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { LDC = 16 };
        #else
        static const uint LDC = 16;
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { RC = 44 };
        #else
        static const uint RC = 44;
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { HUFF_TABLE_SIZEB = NC + DCB + RC + LDC };
        #else
        static const uint HUFF_TABLE_SIZEB = NC + DCB + RC + LDC;
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { HUFF_TABLE_SIZEX = NC + DCX + RC + LDC };
        #else
        static const uint HUFF_TABLE_SIZEX = NC + DCX + RC + LDC;
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { BC = 20 };
        #else
        static const uint BC = 20;
        #endif /* VC6 */

    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { NC30 = 299 };/* alphabet = {0, 1, 2, ..., NC - 1} */
        #else
        static const uint NC30 = 299;/* alphabet = {0, 1, 2, ..., NC - 1} */
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { DC30 = 60 };
        #else
        static const uint DC30 = 60;
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { LDC30 = 17 };
        #else
        static const uint LDC30 = 17;
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { RC30 = 28 };
        #else
        static const uint RC30 = 28;
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { BC30 = 20 };
        #else
        static const uint BC30 = 20;
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { HUFF_TABLE_SIZE30 = NC30 + DC30 + RC30 + LDC30 };
        #else
        static const uint HUFF_TABLE_SIZE30 = NC30 + DC30 + RC30 + LDC30;
        #endif /* VC6 */

    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { NC20 = 298 };/* alphabet = {0, 1, 2, ..., NC - 1} */
        #else
        static const uint NC20 = 298;/* alphabet = {0, 1, 2, ..., NC - 1} */
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { DC20 = 48 };
        #else
        static const uint DC20 = 48;
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { RC20 = 28 };
        #else
        static const uint RC20 = 28;
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { BC20 = 19 };
        #else
        static const uint BC20 = 19;
        #endif /* VC6 */
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { MC20 = 257 };
        #else
        static const uint MC20 = 257;
        #endif /* VC6 */

    // Largest alphabet size among all values listed above.
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
        enum { LARGEST_TABLE_SIZE = 306 };
        #else
        static const uint LARGEST_TABLE_SIZE = 306;
        #endif /* VC6 */

};


enum FilterType {
  // These values must not be changed, because we use them directly
  // in RAR5 compression and decompression code.
  FILTER_DELTA=0, FILTER_E8, FILTER_E8E9, FILTER_ARM, 
  FILTER_AUDIO, FILTER_RGB, FILTER_ITANIUM, FILTER_TEXT, 
  
  // These values can be changed.
  FILTER_LONGRANGE,FILTER_EXHAUSTIVE,FILTER_NONE
};

#endif

/* VC6_ENUM2 */
