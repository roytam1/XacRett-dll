/* MD5.H - header file for MD5C.C
 */

/* Copyright (C) 1991, RSA Data Security, Inc. All rights reserved.

   License to copy and use this software is granted provided that it
   is identified as the "RSA Data Security, Inc. MD5 Message-Digest
   Algorithm" in all material mentioning or referencing this software
   or this function.

   License is also granted to make and use derivative works provided
   that such works are identified as "derived from the RSA Data
   Security, Inc. MD5 Message-Digest Algorithm" in all material
   mentioning or referencing the derived work.  
                                                                    
   RSA Data Security, Inc. makes no representations concerning either
   the merchantability of this software or the suitability of this
   software for any particular purpose. It is provided "as is"
   without express or implied warranty of any kind.  
                                                                    
   These notices must be retained in any copies of any part of this
   documentation and/or software.  
 */

#if !defined(__RMD5_H)   //- include this file only once per compilation
#define __RMD5_H

/* MD5 context. */
typedef struct {
  unsigned int state[4];	//- state (ABCD)
  unsigned int count[2];    //- number of bits, modulo 2^64 (lsb first) 
  unsigned char buffer[64]; //- input buffer 
} MD5CTX;

void MD5Init(MD5CTX *context);
void MD5Update(MD5CTX *context, unsigned char *input, unsigned int inputLen);
void MD5Final(unsigned char digest[16], MD5CTX *context);
void GenerateMD5Key(char *szUserKey,char *szMD5Key16);

#endif //- #if !defined(__RMD5_H)