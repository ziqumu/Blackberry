/*
 * sha.cpp
 *
 * SHA-1
 * Secure Hash Algorithm - 1
 *
 * taken from Applied Cryptography v2 p442
 */

//Commented out by jlau, defined in project settings instead
//#define _CRYPTO_BLOCK_


#include "stdafx.h"
#include <stdio.h>
#include <string.h>

#include "sha.h"

 
#define f1(x,y,z)    ( ( x & y ) | ( ~x & z ) )                /* Rounds  0-19 */
#define f2(x,y,z)    ( x ^ y ^ z )                                 /* Rounds 20-39 */
#define f3(x,y,z)    ( ( x & y ) | ( x & z ) | ( y & z ) ) /* Rounds 40-59 */
#define f4(x,y,z)    ( x ^ y ^ z )                                 /* Rounds 60-79 */

/* Constants */

#define K1  0x5A827999L            /* Rounds  0-19 */
#define K2  0x6ED9EBA1L            /* Rounds 20-39 */
#define K3  0x8F1BBCDCL            /* Rounds 40-59 */
#define K4  0xCA62C1D6L            /* Rounds 60-79 */

/* Initial values */

#define init_h0  0x67452301L
#define init_h1  0xEFCDAB89L
#define init_h2  0x98BADCFEL
#define init_h3  0x10325476L
#define init_h4  0xC3D2E1F0L

/* 32-bit rotate - kludged with shifts */

#define rotate(x,n)  ( ( (x) << (n) ) | ( (x) >> (32 - (n) ) ) )

#define min(a,b) ( (a)<(b) ? (a) : (b) )


struct SHAinfo {
    
    unsigned long h0;
    unsigned long h1;
    unsigned long h2;
    unsigned long h3;
    unsigned long h4;
     
    SHAinfo () 
    {
        h0 = init_h0;
        h1 = init_h1;
        h2 = init_h2;
        h3 = init_h3;
        h4 = init_h4;
    }
};
    


static
void
hash( unsigned long * data_buffer,
        SHAinfo * sha_info_ptr )
{
    unsigned long w[16];
    unsigned long A, B, C, D, E;
    unsigned long temp;
    int t;
    
  /*
    * initialize w
    */
    for( t = 0; t<16; t++ ) {
        w[t] = data_buffer[t];
    }
        
    A = sha_info_ptr->h0;
    B = sha_info_ptr->h1;
    C = sha_info_ptr->h2;
    D = sha_info_ptr->h3;
    E = sha_info_ptr->h4;
        
   /*
    * hash
    */
    for( t=0; t<20; t++ ) {
        
        if( t >= 16 ) {
            temp = w[ (t-3)&0xf ] ^ w[ (t-8)&0xf ] ^ w[ (t-14)&0xf ] ^ w[ (t-16)&0xf ];
            w[t&0xf] = rotate( temp, 1 );
            }
        
        temp = rotate( A,5 ) + f1( B, C, D ) + E + w[t&0xf] + K1; 
        E = D; 
        D = C; 
        C = rotate( B, 30 ); 
        B = A; 
        A = temp; 
    }
        
    for( ; t<40; t++ ) {
    
        temp = w[ (t-3)&0xf ] ^ w[ (t-8)&0xf ] ^ w[ (t-14)&0xf ] ^ w[ (t-16)&0xf ];
        w[t&0xf] = rotate( temp, 1 );
            
        temp = rotate( A,5 ) + f2( B, C, D ) + E + w[t&0xf] + K2; 
        E = D; 
        D = C; 
        C = rotate( B, 30 ); 
        B = A; 
        A = temp; 
    }
        
    for( ; t<60; t++ ) {
    
        temp = w[ (t-3)&0xf ] ^ w[ (t-8)&0xf ] ^ w[ (t-14)&0xf ] ^ w[ (t-16)&0xf ];
        w[t&0xf] = rotate( temp, 1 );
            
        temp = rotate( A,5 ) + f3( B, C, D ) + E + w[t&0xf] + K3; 
        E = D; 
        D = C; 
        C = rotate( B, 30 ); 
        B = A; 
        A = temp; 
    }
        
    for( ; t<80; t++ ) {
    
        temp = w[ (t-3)&0xf ] ^ w[ (t-8)&0xf ] ^ w[ (t-14)&0xf ] ^ w[ (t-16)&0xf ];
        w[t&0xf] = rotate( temp, 1 );
            
        temp = rotate( A,5 ) + f4( B, C, D ) + E + w[t&0xf] + K4; 
        E = D; 
        D = C; 
        C = rotate( B, 30 ); 
        B = A; 
        A = temp; 
    }
        
    sha_info_ptr->h0 += A;
    sha_info_ptr->h1 += B;
    sha_info_ptr->h2 += C;
    sha_info_ptr->h3 += D;
    sha_info_ptr->h4 += E;
}
  
 
static
inline
void
convert_to_big_endian( void * ptr,
                              int num_bytes )
{
    byte * data_ptr = (byte *) ptr;
    byte t;
    
    while( num_bytes > 0 ) {
    
        t = * ( data_ptr + 0 );
        *( data_ptr + 0 ) = * ( data_ptr + 3 );
        *( data_ptr + 3 ) = t;
    
        t = * ( data_ptr + 1 );
        *( data_ptr + 1 ) = * ( data_ptr + 2 );
        *( data_ptr + 2 ) = t;
        
        data_ptr  += 4;
        num_bytes -= 4;
    }
}


static 
int
output( void * buffer_ptr, 
          int buffer_length, 
          SHAinfo * sha_info )
{
    unsigned long data_buffer[ 5 ];

    memset( data_buffer, 0, sizeof( data_buffer ) );
    
    data_buffer[0] = sha_info->h0;
    data_buffer[1] = sha_info->h1;
    data_buffer[2] = sha_info->h2;
    data_buffer[3] = sha_info->h3;
    data_buffer[4] = sha_info->h4;
    
    convert_to_big_endian( data_buffer, sizeof( data_buffer ) );
    
    if( buffer_length > SHA_HASH_LENGTH ) {
        buffer_length = SHA_HASH_LENGTH;
    }
    
    memcpy( buffer_ptr, data_buffer, buffer_length );
    
    return( buffer_length );
}
    
//#pragma warning ( push )
//#pragma warning ( disable:4711 )

int 
sha_hash( const void * init_data_ptr,
             int     data_length,
             void * buffer_ptr,
             int     buffer_length,
             void * prefix_ptr,
             int     prefix_length )
{
    byte * data_ptr = (byte *) init_data_ptr;
    unsigned long data_buffer[ 16 ];
    SHAinfo sha_info;
    
    int original_data_length = data_length;
    
    if( prefix_ptr != NULL ) {
    
        memset( data_buffer, 0, sizeof( data_buffer ) );
        memcpy( data_buffer, prefix_ptr, min( prefix_length, (int) sizeof( data_buffer ) ) );
        convert_to_big_endian( data_buffer, sizeof( data_buffer ) );
        
        hash( data_buffer, &sha_info );
    }
        
    while( data_length >= 64 ) {
        
        memcpy( data_buffer, data_ptr, sizeof( data_buffer ) );
        convert_to_big_endian( data_buffer, sizeof( data_buffer ) );
        
        data_ptr     += sizeof( data_buffer );
        data_length -= sizeof( data_buffer );
        
        hash( data_buffer, &sha_info );
    }
        
   /*
    * append 1 bit to message
    * pad message to 64 bits short of multiple of 512 bits
    * append 64 bit length
    */ 
    memset( data_buffer, 0, sizeof( data_buffer ) );
    
    memcpy( data_buffer, data_ptr, data_length );
    *( ( (byte *) data_buffer ) + data_length ) = 0x80; /* append 1 bit to message */
    
    convert_to_big_endian( data_buffer, sizeof( data_buffer ) );
    
    if( data_length + 1 > 56 ) {
        hash( data_buffer, &sha_info );
        memset( data_buffer, 0, sizeof( data_buffer ) );
    }
        
    data_buffer[15] = (original_data_length + prefix_length) << 3; /* bit length of message */
    
    hash( data_buffer, &sha_info );
    
   /*
    * output hash value
    */
    return( output( buffer_ptr, buffer_length, &sha_info ) );
}

//#pragma warning ( pop )

int
hmac_sha( const void * data_ptr,
             int     data_length,
             const void * key_ptr,
             int     key_length,
             void * buffer_ptr,
             int     buffer_length )
{
    byte key_buffer[64];
    byte hash_buffer[64];
    int i;
    int key_buffer_length;
    int hash_length;
    
    memset( key_buffer, 0, sizeof( key_buffer ) );
    
    if( key_length > 64 ) {

         key_buffer_length = sha_hash( key_ptr,
                                                 key_length,
                                                 key_buffer,
                                                 sizeof( key_buffer ),
                                                 NULL,
                                                 0 );
    } else {
        memcpy( key_buffer, key_ptr, key_length );
    }

    for( i=0; i<64; i++ ) {
        key_buffer[i] ^= 0x36;
    }
        
    hash_length = sha_hash( data_ptr,
                                    data_length,
                                    hash_buffer,
                                    sizeof( hash_buffer ),
                                    key_buffer,
                                    sizeof( key_buffer ) );
                 
    for( i=0; i<64; i++ ) {
        key_buffer[i] ^= 0x36;
        key_buffer[i] ^= 0x5c;
    }
        
    return( sha_hash( hash_buffer,
                            hash_length,
                            buffer_ptr,
                            buffer_length,
                            key_buffer,
                            sizeof( key_buffer ) ) );
}
