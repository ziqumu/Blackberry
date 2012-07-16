/*
 * sha.h
 *
 * Secure Hash Algorithm - 1
 */
 

#ifndef SHA_H
#define SHA_H

typedef unsigned char byte;

#define SHA_HASH_LENGTH 20

extern "C" 
int 
sha_hash( const void * data_ptr,          /* data to be hashed */
          int    data_length,
          void * buffer_ptr,              /* hash buffer ( 20 bytes ) */
          int    buffer_length = SHA_HASH_LENGTH,
          void * prefix_ptr = NULL,       /* for hmac */
          int    prefix_length = 0 );
          
          
extern "C" 
int
hmac_sha( const void * data_ptr,          /* data to be hashed */
          int    data_length,            
          const void * key_ptr,           /* secret key ( up to 64 bytes ) */
          int    key_length,
          void * buffer_ptr,              /* hash buffer ( 20 bytes ) */
          int    buffer_length = SHA_HASH_LENGTH );

#endif

