/*
 *	
 * Copyright (c) 2016 Cisco Systems, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * 
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * tls.c
 *
 * contains the functionality for TLS awareness
 * 
 */

#include <stdio.h>    /* for fprintf(), etc */
#include <pcap.h>     /* for pcap_hdr       */
#include <ctype.h>    /* for isprint()      */
#include <string.h>   /* for memcpy()       */
#include <stdlib.h>
#include <netinet/in.h>
#include "tls.h"


inline unsigned int timer_gt_tls(const struct timeval *a, const struct timeval *b) {
  return (a->tv_sec == b->tv_sec) ? (a->tv_usec > b->tv_usec) : (a->tv_sec > b->tv_sec);
}

inline unsigned int timer_lt_tls(const struct timeval *a, const struct timeval *b) {
  return (a->tv_sec == b->tv_sec) ? (a->tv_usec < b->tv_usec) : (a->tv_sec < b->tv_sec);
}

inline void timer_sub_tls(const struct timeval *a, const struct timeval *b, struct timeval *result)  {  
  result->tv_sec = a->tv_sec - b->tv_sec;        
  result->tv_usec = a->tv_usec - b->tv_usec;     
  if (result->tv_usec < 0) {                         
    --result->tv_sec;                                
    result->tv_usec += 1000000;                      
  }                                                    
}

inline void timer_clear_tls(struct timeval *a) { 
  a->tv_sec = a->tv_usec = 0; 
}

unsigned int timeval_to_milliseconds_tls(struct timeval ts) {
  unsigned int result = ts.tv_usec / 1000 + ts.tv_sec * 1000;
  return result;
}

/* initialize data associated with TLS */
void tls_record_init(struct tls_information *r) {
  r->tls_op = 0;
  r->num_ciphersuites = 0;
  r->num_tls_extensions = 0;
  r->num_server_tls_extensions = 0;
  r->tls_sid_len = 0;
  r->tls_v = 0;
  r->tls_client_key_length = 0;
  r->certificate_buffer = NULL;
  r->certificate_offset = 0;
  r->start_cert = 0;

  memset(r->tls_len, 0, sizeof(r->tls_len));
  memset(r->tls_time, 0, sizeof(r->tls_time));
  memset(r->tls_type, 0, sizeof(r->tls_type));
  memset(r->ciphersuites, 0, sizeof(r->ciphersuites));
  memset(r->tls_extensions, 0, sizeof(r->tls_extensions));
  memset(r->server_tls_extensions, 0, sizeof(r->server_tls_extensions));
  memset(r->tls_sid, 0, sizeof(r->tls_sid));
  memset(r->tls_random, 0, sizeof(r->tls_random));

  r->num_certificates = 0;
  int i, j;
  for (i = 0; i < MAX_CERTIFICATES; i++) {
    r->certificates[i].signature = NULL;
    r->certificates[i].subject_public_key_size = 0;
    r->certificates[i].signature_key_size = 0;

    for (j = 0; j < MAX_SAN; j++) {
      r->certificates[i].san[j] = NULL;
    }
  }
}

/* free data associated with TLS */
void tls_record_delete(struct tls_information *r) {
  int i,j;
  if (r->certificate_buffer) {
    free(r->certificate_buffer);
  }
  for (i=0; i<r->num_tls_extensions; i++) {
    if (r->tls_extensions[i].data) {
      free(r->tls_extensions[i].data);
    }
  }
  for (i=0; i<r->num_server_tls_extensions; i++) {
    if (r->server_tls_extensions[i].data) {
      free(r->server_tls_extensions[i].data);
    }
  }
  for (i = 0; i < r->num_certificates; i++) {
    if (r->certificates[i].signature) {
      free(r->certificates[i].signature);
    }
    if (r->certificates[i].serial_number) {
      free(r->certificates[i].serial_number);
    }
    for (j = 0; j < MAX_RDN; j++) {
      if (r->certificates[i].issuer_id[j]) {
	free(r->certificates[i].issuer_id[j]);
      }
    }
    for (j = 0; j < MAX_RDN; j++) {
      if (r->certificates[i].issuer_string[j]) {
	free(r->certificates[i].issuer_string[j]);
      }
    }
    if (r->certificates[i].validity_not_before) {
      free(r->certificates[i].validity_not_before);
    }
    if (r->certificates[i].validity_not_after) {
      free(r->certificates[i].validity_not_after);
    }
    for (j = 0; j < MAX_RDN; j++) {
      if (r->certificates[i].subject_id[j]) {
	free(r->certificates[i].subject_id[j]);
      }
    }
    for (j = 0; j < MAX_RDN; j++) {
      if (r->certificates[i].subject_string[j]) {
	free(r->certificates[i].subject_string[j]);
      }
    }
    if (r->certificates[i].subject_public_key_algorithm) {
      free(r->certificates[i].subject_public_key_algorithm);
    }
    for (j = 0; j < MAX_EXTENSIONS; j++) {
      if (r->certificates[i].ext_id[j]) {
	free(r->certificates[i].ext_id[j]);
      }
    }
    for (j = 0; j < MAX_EXTENSIONS; j++) {
      if (r->certificates[i].ext_data[j]) {
	free(r->certificates[i].ext_data[j]);
      }
    }
    for (j = 0; j < MAX_SAN; j++) {
      if (r->certificates[i].san[j]) {
	free(r->certificates[i].san[j]);
      }
    }
  }
}


unsigned short raw_to_unsigned_short(const void *x) {
  unsigned short int y;
  const unsigned char *z = x;

  y = z[0];
  y *= 256;
  y += z[1];
  return y;
}

void TLSClientHello_get_ciphersuites(const void *x, int len, 
				     struct tls_information *r) {
  unsigned int session_id_len;
  const unsigned char *y = x;
  unsigned short int cipher_suites_len;
  unsigned int i = 0;

  //  mem_print(x, len);
  //  fprintf(stderr, "TLS version %0x%0x\n", y[0], y[1]);

  if ((y[0] != 3) || (y[1] > 3)) {
    // fprintf(stderr, "warning: TLS version %0x%0x\n", y[0], y[1]);
    return;  
  }

  /* record the 32-byte Random field */
  memcpy(r->tls_random, y+2, 32); 

  y += 34;  /* skip over ProtocolVersion and Random */
  session_id_len = *y;

  len -= (session_id_len + 3);
  if (len < 0) {
    //fprintf(info, "error: TLS session ID too long\n"); 
    return;   /* error: session ID too long */
  }

  /* record the session id, if there is one */
  if (session_id_len) {
    r->tls_sid_len = session_id_len;
    memcpy(r->tls_sid, y+1, session_id_len); 
  }

  y += (session_id_len + 1);   /* skip over SessionID and SessionIDLen */
  // mem_print(y, 2);
  cipher_suites_len = raw_to_unsigned_short(y);
  if (len < cipher_suites_len) {
    //fprintf(info, "error: TLS ciphersuite list too long\n"); 
    return;   /* error: session ID too long */
  }
  y += 2;

  r->num_ciphersuites = cipher_suites_len/2;
  r->num_ciphersuites = r->num_ciphersuites > MAX_CS ? MAX_CS : r->num_ciphersuites;
  for (i=0; i < r->num_ciphersuites; i++) {
    unsigned short int cs;
    
    cs = raw_to_unsigned_short(y);
    r->ciphersuites[i] = cs;
    y += 2;
  }
}

void TLSClientHello_get_extensions(const void *x, int len, 
				     struct tls_information *r) {
  unsigned int session_id_len, compression_method_len;
  const unsigned char *y = x;
  unsigned short int cipher_suites_len, extensions_len;
  unsigned int i = 0;


  len -= 4; // get handshake message length
  if ((y[0] != 3) || (y[1] > 3)) {
    return;  
  }

  y += 34;  /* skip over ProtocolVersion and Random */
  len -= 34;
  session_id_len = *y;

  len -= (session_id_len + 3);
  if (len < 0) {
    //fprintf(info, "error: TLS session ID too long\n"); 
    return;   /* error: session ID too long */
  }

  y += (session_id_len + 1);   /* skip over SessionID and SessionIDLen */

  cipher_suites_len = raw_to_unsigned_short(y);
  if (len < cipher_suites_len) {
    //fprintf(info, "error: TLS ciphersuite list too long\n"); 
    return;   /* error: session ID too long */
  }
  y += 2;
  len -= 2;

  // skip over ciphersuites
  y += cipher_suites_len;
  len -= cipher_suites_len;

  // skip over compression methods
  compression_method_len = *y;
  y += 1+compression_method_len;
  len -= 1+compression_method_len;

  // extensions length
  extensions_len = raw_to_unsigned_short(y);
  if (len < extensions_len) {
    //fprintf(info, "error: TLS extensions too long\n"); 
    return;   /* error: session ID too long */
  }
  y += 2;
  len -= 2;

  i = 0;
  while (len > 0) {
    r->tls_extensions[i].type = raw_to_unsigned_short(y);
    r->tls_extensions[i].length = raw_to_unsigned_short(y+2);
    // should check if length is reasonable?
    r->tls_extensions[i].data = malloc(r->tls_extensions[i].length);
    memcpy(r->tls_extensions[i].data, y+4, r->tls_extensions[i].length);

    r->num_tls_extensions += 1;
    i += 1;

    len -= 4;
    len -= raw_to_unsigned_short(y+2);
    y += 4 + raw_to_unsigned_short(y+2);
  }
}


void TLSServerCertificate_parse(const void *x, unsigned int len,
				    struct tls_information *r) {

  const unsigned char *y = x;
  short certs_len, cert_len, tmp_len, tmp_len2, cur_cert, cur_rdn, issuer_len, subject_len,
    rdn_seq_len, ext_len, cur_ext;
  unsigned char lo, mid, hi;
  certs_len = raw_to_unsigned_short(y+1);
  //printf("certificates_length: %i\n",(unsigned int)certs_len);
  y += 3;
  certs_len -= 3;
  
  while (certs_len > 0) {
    cur_cert = r->num_certificates;
    r->num_certificates += 1;
    cert_len = raw_to_unsigned_short(y+1);
    //printf("\tcert_length: %i\n",(unsigned int)cert_len);
    r->certificates[cur_cert].length = (unsigned short)cert_len;
    
    y += 3; // skip over single certificate length
    certs_len -= 3;
    
    y += 14; // skip over lengths
    certs_len -= 14;
    
    // parse serial number
    tmp_len = (*y);
    r->certificates[cur_cert].serial_number = malloc(tmp_len);
    memcpy(r->certificates[cur_cert].serial_number, y+1, tmp_len);
    r->certificates[cur_cert].serial_number_length = tmp_len;
    //printf("\tserial_number: ");
    //printf_raw_as_hex_tls(r->certificates[cur_cert].serial_number, tmp_len);
    //printf("\n");
    y += tmp_len+1;
    certs_len -= tmp_len+1;
    y += 2;
    certs_len -= 2;

    // parse signature
    tmp_len = *(y+1);
    y += 2;
    certs_len -= 2;
    r->certificates[cur_cert].signature = malloc(tmp_len);
    memcpy(r->certificates[cur_cert].signature, y, tmp_len); 
    r->certificates[cur_cert].signature_length = tmp_len;
    //printf("\tsignature_algorithm: ");
    //printf_raw_as_hex_tls(r->certificates[cur_cert].signature, tmp_len);
    //printf("\n");
    y += tmp_len;
    certs_len -= tmp_len;
    y += 2;
    certs_len -= 2;

    // parse issuer
    cur_rdn = 0;
    issuer_len = *(y+1);
    if (issuer_len == 129) {
      issuer_len = *(y+2);
      y += 5;
      certs_len -= 5;
    } else if (issuer_len == 130) {
      issuer_len = raw_to_unsigned_short(y+2);
      y += 6;
      certs_len -= 6;
    } else {
      y += 4;
      certs_len -= 4;
    }
    while (issuer_len > 0) {
      if (cur_rdn >= MAX_RDN) {
	break;
      }
      rdn_seq_len = *(y+1);
      y += 2;
      certs_len -= 2;
      issuer_len -= 2;
      
      tmp_len = *(y+1);
      r->certificates[cur_cert].issuer_id[cur_rdn] = malloc(tmp_len);
      memcpy(r->certificates[cur_cert].issuer_id[cur_rdn], y+2, tmp_len);
      r->certificates[cur_cert].issuer_id_length[cur_rdn] = tmp_len;
      //printf("\tissuer_id: ");
      //printf_raw_as_hex_tls(r->certificates[cur_cert].issuer_id[cur_rdn], tmp_len);
      //printf("\n");
      
      tmp_len2 = *(y+tmp_len+2+1);
      r->certificates[cur_cert].issuer_string[cur_rdn] = malloc(tmp_len2+1);
      memset(r->certificates[cur_cert].issuer_string[cur_rdn], 0, tmp_len2+1);
      memcpy(r->certificates[cur_cert].issuer_string[cur_rdn], y+tmp_len+2+2, tmp_len2);
      //r->certificates[cur_cert].issuer_string_length[cur_rdn] = tmp_len2;
      //printf("\tissuer_string: \"%s\"\n", (char*)r->certificates[cur_cert].issuer_string[cur_rdn]);

      y += 2;
      certs_len -= 2;
      issuer_len -= 2;
      y += rdn_seq_len;
      certs_len -= rdn_seq_len;
      issuer_len -= rdn_seq_len;
      cur_rdn++;
      r->certificates[cur_cert].num_issuer = cur_rdn;
    }
    
    // validity_not_before
    //	  tmp_len = *(y+1);
    
    //y += 2;
    //certs_len -= 2;
    tmp_len = *(y+1);
    y += 2;
    certs_len -= 2;
    r->certificates[cur_cert].validity_not_before = malloc(tmp_len+1);
    memset(r->certificates[cur_cert].validity_not_before, 0, tmp_len+1);
    memcpy(r->certificates[cur_cert].validity_not_before, y, tmp_len); 
    //printf("\tvalidity_not_before: \"%s\"\n", (char *)r->certificates[cur_cert].validity_not_before);
    y += tmp_len;
    certs_len -= tmp_len;
    // validity_not_after
    tmp_len = *(y+1);
    y += 2;
    certs_len -= 2;
    r->certificates[cur_cert].validity_not_after = malloc(tmp_len+1);
    memset(r->certificates[cur_cert].validity_not_after, 0, tmp_len+1);
    memcpy(r->certificates[cur_cert].validity_not_after, y, tmp_len); 
    //printf("\tvalidity_not_after: \"%s\"\n", (char *)r->certificates[cur_cert].validity_not_after);
    y += tmp_len;
    certs_len -= tmp_len;

    // parse subject
    cur_rdn = 0;
    subject_len = *(y+1);
    if (subject_len == 129) {
      subject_len = *(y+2);
      y += 5;
      certs_len -= 5;
    } else if (subject_len == 130) {
      subject_len = raw_to_unsigned_short(y+2);
      y += 6;
      certs_len -= 6;
    } else {
      y += 4;
      certs_len -= 4;
    }
    
    while (subject_len > 0) {
      if (cur_rdn >= MAX_RDN) {
	break;
      }
      rdn_seq_len = *(y+1);
      y += 2;
      certs_len -= 2;
      subject_len -= 2;
      
      tmp_len = *(y+1);
      r->certificates[cur_cert].subject_id[cur_rdn] = malloc(tmp_len);
      memcpy(r->certificates[cur_cert].subject_id[cur_rdn], y+2, tmp_len);
      r->certificates[cur_cert].subject_id_length[cur_rdn] = tmp_len;
      //printf("\tsubject_id: ");
      //printf_raw_as_hex_tls(r->certificates[cur_cert].subject_id[cur_rdn], tmp_len);
      //printf("\n");
      
      tmp_len2 = *(y+tmp_len+2+1);
      r->certificates[cur_cert].subject_string[cur_rdn] = malloc(tmp_len2+1);
      memset(r->certificates[cur_cert].subject_string[cur_rdn], 0, tmp_len2+1);
      memcpy(r->certificates[cur_cert].subject_string[cur_rdn], y+tmp_len+2+2, tmp_len2);
      //printf("\tsubject_string: \"%s\"\n", (char*)r->certificates[cur_cert].subject_string[cur_rdn]);

      y += 2;
      certs_len -= 2;
      subject_len -= 2;
      y += rdn_seq_len;
      certs_len -= rdn_seq_len;
      subject_len -= rdn_seq_len;
      cur_rdn++;
      r->certificates[cur_cert].num_subject = cur_rdn;
    }
    
    //printf("\tNext Three Bytes: ");
    //printf_raw_as_hex_tls(y, 3);
    //printf("\n");
    
    // parse subject public key info
    if (*(y+1) == 48) {
      y += 3;
      certs_len -= 3;
    } else {
      y += 4;
      certs_len -= 4;
    }
    tmp_len = *(y+1);
    y += 2;
    certs_len -= 2;
    r->certificates[cur_cert].subject_public_key_algorithm = malloc(tmp_len);
    memcpy(r->certificates[cur_cert].subject_public_key_algorithm, y, tmp_len); 
    r->certificates[cur_cert].subject_public_key_algorithm_length = tmp_len;
    //printf("\tsubject_public_key_algorithm: ");
    //printf_raw_as_hex_tls(r->certificates[cur_cert].subject_public_key_algorithm, tmp_len);
    //printf("\n");
    y += tmp_len;
    certs_len -= tmp_len;
    y += 2;
    certs_len -= 2;
    
    if (*(y+1) == 129) {
      tmp_len = *(y+2);
      r->certificates[cur_cert].subject_public_key_size = (tmp_len-13)*8;
      //printf("\tsubject_public_key_size: %i\n", (tmp_len-13)*8);
      //tmp_len -= 13;
      y += tmp_len+3;
      certs_len -= tmp_len+3;
    } else if (*(y+1) == 130) {
      tmp_len = raw_to_unsigned_short(y+2);
      r->certificates[cur_cert].subject_public_key_size = (tmp_len-15)*8;
      //printf("\tsubject_public_key_size: %i\n", (tmp_len-15)*8);
      //tmp_len -= 15;
      y += tmp_len+4;
      certs_len -= tmp_len+4;	    
    } else {
      break ;
    }
    
    
    // optional: parse extensions
    if (*y == 163 && *(y+1) == 130) {
      y += 5;
      certs_len -= 5;
      
      if (*y == 130) {
	ext_len = raw_to_unsigned_short(y+1);
	y += 3;
	certs_len -= 3;
      } else {
	ext_len = *y;
	y += 2;
	certs_len -= 2;
      }
      cur_ext = 0;
      while (ext_len > 0) {
	tmp_len2 = *(y+1);
	if (tmp_len2 == 130) {
	  tmp_len2 = raw_to_unsigned_short(y+2);
	  y += 4;
	  certs_len -= 4;
	  ext_len -= 4;		
	} else {
	  y += 2;
	  certs_len -= 2;
	  ext_len -= 2;
	}
	
	// check for extension-specific parsing
	hi = *(y+2);
	mid = *(y+3);
	lo = *(y+4);
	if ((hi == 85) && (mid == 29) && (lo == 17)) { // parse SAN
	  tmp_len = *(y+1);
	  tmp_len2 = tmp_len2-tmp_len-2;
	  
	  parse_san(y+tmp_len+2+4, tmp_len2-4, &r->certificates[cur_cert]);
	  
	  y += tmp_len2+tmp_len+2;
	  certs_len -= tmp_len2+tmp_len+2;
	  ext_len -= tmp_len2+tmp_len+2;
	} else { // general purpose ext parsing
	  tmp_len = *(y+1);
	  r->certificates[cur_cert].ext_id[cur_ext] = malloc(tmp_len);
	  memcpy(r->certificates[cur_cert].ext_id[cur_ext], y+2, tmp_len);
	  r->certificates[cur_cert].ext_id_length[cur_ext] = tmp_len;
	  //printf("\text_id: ");
	  //printf_raw_as_hex_tls(r->certificates[cur_cert].ext_id[cur_ext], tmp_len);
	  //printf("\n");
	  
	  tmp_len2 = tmp_len2-tmp_len-2;
	  r->certificates[cur_cert].ext_data[cur_ext] = malloc(tmp_len2);
	  //memset(r->certificates[cur_cert].ext_data[cur_ext], 0, tmp_len2);
	  memcpy(r->certificates[cur_cert].ext_data[cur_ext], y+tmp_len+2, tmp_len2);
	  r->certificates[cur_cert].ext_data_length[cur_ext] = tmp_len2;
	  //printf("\text_data: ");
	  //printf_raw_as_hex_tls(r->certificates[cur_cert].ext_data[cur_ext], tmp_len2);
	  //printf("\n");
	  
	  cur_ext++;
	  r->certificates[cur_cert].num_ext = cur_ext;
	  y += tmp_len2+tmp_len+2;
	  certs_len -= tmp_len2+tmp_len+2;
	  ext_len -= tmp_len2+tmp_len+2;
	}
      }	    
    }
    
    // parse signature key size
    tmp_len = *(y+1);
    y += tmp_len+2;
    certs_len -= tmp_len+2;
    
    if (*(y+1) == 129) {
      tmp_len = *(y+2);
      r->certificates[cur_cert].signature_key_size = (tmp_len-1)*8;
      //printf("\tsignature_key_size: %i\n", (tmp_len-1)*8);
      y += tmp_len+3;
      certs_len -= tmp_len+3;
    } else if (*(y+1) == 130) {
      tmp_len = raw_to_unsigned_short(y+2);
      r->certificates[cur_cert].signature_key_size = (tmp_len-1)*8;
      //printf("\tsignature_key_size: %i\n", (tmp_len-1)*8);
      y += tmp_len+4;
      certs_len -= tmp_len+4;	    
    } else {
      break ;
    }
    
    //certs_len -= cert_len;
    //printf("\n");
    //break;
  }
  
  
  //printf("\n");
}

void parse_san(const void *x, int len, struct tls_certificate *r) {
  unsigned short num_san = 0;
  unsigned short tmp_len;
  const unsigned char *y = x;

  while (len > 0) {
    tmp_len = *(y+1);
    r->san[num_san] = malloc(tmp_len+1);
    memset(r->san[num_san], 0, tmp_len+1);
    memcpy(r->san[num_san], y+2, tmp_len);

    num_san += 1;
    y += tmp_len+2;
    len -= tmp_len+2;
  }
  r->num_san = num_san;
}

void TLSServerHello_get_ciphersuite(const void *x, unsigned int len,
				    struct tls_information *r) {
  unsigned int session_id_len;
  const unsigned char *y = x;
  unsigned short int cs; 

  //  mem_print(x, len);

  if ((y[0] != 3) || (y[1] > 3)) {
    // fprintf(stderr, "warning: TLS version %0x%0x\n", y[0], y[1]);
    return;  
  }

  /* record the 32-byte Random field */
  memcpy(r->tls_random, y+2, 32); 

  y += 34;  /* skip over ProtocolVersion and Random */
  session_id_len = *y;
  if (session_id_len + 3 > len) {
    //fprintf(info, "error: TLS session ID too long\n"); 
    return;   /* error: session ID too long */
  }

  /* record the session id, if there is one */
  if (session_id_len) {
    r->tls_sid_len = session_id_len;
    memcpy(r->tls_sid, y+1, session_id_len); 
  }

  y += (session_id_len + 1);   /* skip over SessionID and SessionIDLen */
  // mem_print(y, 2);
  cs = raw_to_unsigned_short(y);

  r->num_ciphersuites = 1;
  r->ciphersuites[0] = cs;
}

void TLSServerHello_get_extensions(const void *x, int len,
				    struct tls_information *r) {
  unsigned int session_id_len, compression_method_len;
  const unsigned char *y = x;
  unsigned short int extensions_len;
  unsigned int i = 0;

  //  mem_print(x, len);
  len -= 4;
  if ((y[0] != 3) || (y[1] < 3)) {
    //printf("warning: TLS version %0x%0x\n", y[0], y[1]);
    return;  
  }

  y += 34;  /* skip over ProtocolVersion and Random */
  len -= 34;
  session_id_len = *y;

  len -= (session_id_len + 1);
  y += (session_id_len + 1);   /* skip over SessionID and SessionIDLen */

  len -= 2; /* skip over scs */
  y += 2;

  // skip over compression methods
  compression_method_len = *y;
  y += 1+compression_method_len;
  len -= 1+compression_method_len;

  // extensions length
  extensions_len = raw_to_unsigned_short(y);
  if (len < extensions_len) {
    //fprintf(info, "error: TLS extensions too long\n"); 
    return;   /* error: session ID too long */
  }
  y += 2;
  len -= 2;

  i = 0;
  while (len > 0) {
    if (raw_to_unsigned_short(y+2) > 64) {
      break;
    }
    r->server_tls_extensions[i].type = raw_to_unsigned_short(y);
    r->server_tls_extensions[i].length = raw_to_unsigned_short(y+2);
    // should check if length is reasonable?
    r->server_tls_extensions[i].data = malloc(r->server_tls_extensions[i].length);
    memcpy(r->server_tls_extensions[i].data, y+4, r->server_tls_extensions[i].length);

    r->num_server_tls_extensions += 1;
    i += 1;

    len -= 4;
    len -= raw_to_unsigned_short(y+2);
    y += 4 + raw_to_unsigned_short(y+2);
  }
}

unsigned int TLSHandshake_get_length(const struct TLSHandshake *H) {
  return H->lengthLo + ((unsigned int) H->lengthMid) * 0x100 
    + ((unsigned int) H->lengthHi) * 0x10000;
}

unsigned int tls_header_get_length(const struct tls_header *H) {
  return H->lengthLo + ((unsigned int) H->lengthMid) * 0x100;
}


char *tls_version_get_string(enum tls_version v) {
  switch(v) {
  case 1:
    return "sslv2";
    break;
  case 2:
    return "sslv3";
    break;
  case 3:
    return "tls1.0";
    break;
  case 4:
    return "tls1.1";
    break;
  case 5:
    return "tls1.2";
    break;
  case 0:
    ;
    break;
  }
  return "unknown";
}

unsigned char tls_version(const void *x) {
  const unsigned char *z = x;

  // printf("tls_version: ");  mem_print(x, 2);

  switch(z[0]) {
  case 3:
    switch(z[1]) {
    case 0:
      return tls_sslv3;
      break;
    case 1:
      return tls_tls1_0;
      break;
    case 2:
      return tls_tls1_1;
      break;
    case 3:
      return tls_tls1_2;
      break;
    }
    break;
  case 2:
    return tls_sslv2;
    break;
  default:
    ;
  } 
  return tls_unknown;
}

unsigned int packet_is_sslv2_hello(const void *data) {
  const unsigned char *d = data;
  unsigned char b[3];
  
  b[0] = d[0];
  b[1] = d[1];
  b[2] = d[2];

  if (b[0] & 0x80) {
    b[0] &= 0x7F;
    if (raw_to_unsigned_short(b) > 9) {
      if (b[2] == 0x01) {
	return tls_sslv2;
      }
    }    
  }

  return tls_unknown;
}

struct tls_information *
process_tls(const struct pcap_pkthdr *h, const void *start, int len, struct tls_information *r) {
  const struct tls_header *tls;
  unsigned int tls_len;
  unsigned int levels = 0;
  //unsigned char end_cert = 0;

  /* currently skipping SSLv2 */

  tls = start;
  if (tls->ContentType == handshake && tls->Handshake.HandshakeType == server_hello) {
    if (r->start_cert == 0) {
      // create buffer to store the server certificate
      r->certificate_buffer = malloc(MAX_CERTIFICATE_BUFFER);
    
      //memcpy(r->certificate_buffer+r->certificate_offset, &tls->Handshake.body, tls_len);
      memcpy(r->certificate_buffer, tls, len);
      //r->certificate_offset += tls_len;
      r->certificate_offset += len;
      
      r->start_cert = 1;
    } else {
      if (r->certificate_offset + len > MAX_CERTIFICATE_BUFFER) {
      } else {
	//memcpy(r->certificate_buffer+r->certificate_offset, &tls->Handshake.body, tls_len);
	memcpy(r->certificate_buffer+r->certificate_offset, tls, len);
	//r->certificate_offset += tls_len;
	r->certificate_offset += len;
      }
    }

  } else if (r->start_cert) {
    if (r->certificate_offset + len > MAX_CERTIFICATE_BUFFER) {
    } else {
      //memcpy(r->certificate_buffer+r->certificate_offset, &tls->Handshake.body, tls_len);
      memcpy(r->certificate_buffer+r->certificate_offset, tls, len);
      //r->certificate_offset += tls_len;
      r->certificate_offset += len;
    }
  }

  while (len > 0) {
    tls = start;
    tls_len = tls_header_get_length(tls);

    //if (start_cert) {
    //  memcpy(r->certificate_buffer+r->certificate_offset, &tls->Handshake.body, tls_len);
    //  r->certificate_offset += tls_len;
    //}

    // process certificate
    if (r->start_cert && ((tls->ContentType == application_data) ||
		       (r->certificate_offset >= 4000) ||
		       (tls->Handshake.HandshakeType == server_hello_done))) {
      //TLSServerCertificate_parse(r->certificate_buffer, tls_len, r);
      process_certificate(r->certificate_buffer, r->certificate_offset, r);
      if (r->certificate_buffer) {
	free(r->certificate_buffer);
	r->certificate_buffer = NULL;
      }
      r->start_cert = 0;
    }

    if (tls->ContentType == application_data) {
      levels++;

      /* sanity check version number */
      if ((tls->ProtocolVersionMajor != 3) || (tls->ProtocolVersionMinor > 3)) {
	return NULL;
      }
      r->tls_v = tls_version(&tls->ProtocolVersionMajor);

    } else if (tls->ContentType == handshake) {
      if (tls->Handshake.HandshakeType == client_hello) {
	
	TLSClientHello_get_ciphersuites(&tls->Handshake.body, tls_len, r);
	TLSClientHello_get_extensions(&tls->Handshake.body, tls_len, r);

      } else if (tls->Handshake.HandshakeType == server_hello) {

	TLSServerHello_get_ciphersuite(&tls->Handshake.body, tls_len, r);
	TLSServerHello_get_extensions(&tls->Handshake.body, (int)tls_len, r);

      } else if (tls->Handshake.HandshakeType == client_key_exchange) {

	//	TLSClientKeyExchange_get_key_length(&tls->Handshake.body, tls_len, tls_version(&tls->ProtocolVersionMajor), r);
	if (r->tls_client_key_length == 0) {
	  r->tls_client_key_length = (unsigned int)tls->Handshake.lengthLo*8 + 
	    (unsigned int)tls->Handshake.lengthMid*8*256 + 
	    (unsigned int)tls->Handshake.lengthHi*8*256*256;
	  if (r->tls_client_key_length > 8193) {
	    r->tls_client_key_length = 0;
	  }
	}

      } if (((tls->Handshake.HandshakeType > 2) & 
	     (tls->Handshake.HandshakeType < 11)) ||
	    ((tls->Handshake.HandshakeType > 16) & 
	     (tls->Handshake.HandshakeType < 20)) ||
	    (tls->Handshake.HandshakeType > 20)) {
	
	/*
	 * we encountered an unknown handshaketype, so this packet is
	 * not actually a TLS handshake, so we bail on decoding it
	 */
	return NULL;
      }

      if (r->tls_op < MAX_NUM_RCD_LEN) {
	r->tls_type[r->tls_op].handshake = tls->Handshake.HandshakeType;
      }      
    } else if (tls->ContentType != change_cipher_spec || 
	       tls->ContentType != alert) {
      
      /* 
       * we encountered an unknown contenttype, so this is not
       * actually a TLS record, so we bail on decoding it
       */      
      return NULL;
    }

    /* record TLS record lengths and arrival times */
    if (r->tls_op < MAX_NUM_RCD_LEN) {
      r->tls_type[r->tls_op].content = tls->ContentType;
      r->tls_len[r->tls_op] = tls_len;
      r->tls_time[r->tls_op] = h->ts;
    }

    /* increment TLS record count in tls_information */
    r->tls_op++;

    tls_len += 5; /* advance over header */
    start += tls_len;
    len -= tls_len;
  }

  return NULL;
}

struct tls_information *
process_certificate(const void *start, int len, struct tls_information *r) {
  const struct tls_header *tls;
  unsigned int tls_len;

  while (len > 0) {
    tls = start;
    tls_len = tls_header_get_length(tls);

    //printf("%i\n",tls->ContentType);
    //printf("%i\n",tls->Handshake.HandshakeType);
    if (tls->ContentType == handshake) {
      if (tls->Handshake.HandshakeType == certificate) {

	TLSServerCertificate_parse(&tls->Handshake.body, tls_len, r);

      }
    }
    tls_len += 5; /* advance over header */
    start += tls_len;
    len -= tls_len;
    //printf("%i\n",len);
  }

  return NULL;
}

void printf_raw_as_hex_tls(const void *data, unsigned int len) {
  const unsigned char *x = data;
  const unsigned char *end = data + len;

  if (data == NULL) { /* special case for nfv9 TLS export */
    printf("\"");   /* quotes needed for JSON */
    printf("\"");
    return ;
  }
  
  printf("\"");   /* quotes needed for JSON */
  while (x < end) {
    printf("%02x", *x++);
  }
  printf("\"");

}

void zprintf_raw_as_hex_tls(zfile f, const void *data, unsigned int len) {
  const unsigned char *x = data;
  const unsigned char *end = data + len;

  if (data == NULL) { /* special case for nfv9 TLS export */
    zprintf(f, "\"");   /* quotes needed for JSON */
    zprintf(f, "\"");
    return ;
  }
  
  zprintf(f, "\"");   /* quotes needed for JSON */
  while (x < end) {
    zprintf(f, "%02x", *x++);
  }
  zprintf(f, "\"");

}

void print_bytes_dir_time_tls(unsigned short int pkt_len, char *dir, struct timeval ts, struct tls_type_code type, char *term, zfile f) {

  zprintf(f, "{\"b\":%u,\"dir\":\"%s\",\"ipt\":%u,\"tp\":\"%u:%u\"}%s", 
	  pkt_len, dir, timeval_to_milliseconds_tls(ts), type.content, type.handshake, term);

}

unsigned int num_pkt_len_tls = NUM_PKT_LEN_TLS;

void len_time_print_interleaved_tls(unsigned int op, const unsigned short *len, const struct timeval *time, const struct tls_type_code *type,
				    unsigned int op2, const unsigned short *len2, const struct timeval *time2, const struct tls_type_code *type2, zfile f) {
  unsigned int i, j, imax, jmax;
  struct timeval ts, ts_last, ts_start, tmp;
  unsigned int pkt_len;
  char *dir;
  struct tls_type_code typecode;

  zprintf(f, ",\"srlt\":[");

  if (len2 == NULL) {
    
    ts_start = *time;

    imax = op > num_pkt_len_tls ? num_pkt_len_tls : op;
    if (imax == 0) { 
      ; /* no packets had data, so we print out nothing */
    } else {
      for (i = 0; i < imax-1; i++) {
	if (i > 0) {
	  timer_sub_tls(&time[i], &time[i-1], &ts);
	} else {
	  timer_clear_tls(&ts);
	}
	print_bytes_dir_time_tls(len[i], OUT, ts, type[i], ",", f);
      }
      if (i == 0) {        /* this code could be simplified */ 	
	timer_clear_tls(&ts);  
      } else {
	timer_sub_tls(&time[i], &time[i-1], &ts);
      }
      print_bytes_dir_time_tls(len[i], OUT, ts, type[i], "", f);
    }
    zprintf(f, "]"); 
  } else {

    if (timer_lt_tls(time, time2)) {
      ts_start = *time;
    } else {
      ts_start = *time2;
    }

    imax = op > num_pkt_len_tls ? num_pkt_len_tls : op;
    jmax = op2 > num_pkt_len_tls ? num_pkt_len_tls : op2;
    i = j = 0;
    ts_last = ts_start;
    while ((i < imax) || (j < jmax)) {      

      if (i >= imax) {  /* record list is exhausted, so use twin */
	dir = OUT;
	ts = time2[j];
	pkt_len = len2[j];
	typecode = type2[j];
	j++;
      } else if (j >= jmax) {  /* twin list is exhausted, so use record */
	dir = IN;
	ts = time[i];
	pkt_len = len[i];
	typecode = type[i];
	i++;
      } else { /* neither list is exhausted, so use list with lowest time */     

	if (timer_lt_tls(&time[i], &time2[j])) {
	  ts = time[i];
	  pkt_len = len[i];
	  typecode = type[i];
	  dir = IN;
	  if (i < imax) {
	    i++;
	  }
	} else {
	  ts = time2[j];
	  pkt_len = len2[j];
	  typecode = type2[j];
	  dir = OUT;
	  if (j < jmax) {
	    j++;
	  }
	}
      }
      timer_sub_tls(&ts, &ts_last, &tmp);
      print_bytes_dir_time_tls(pkt_len, dir, tmp, typecode, "", f);
      ts_last = ts;
      if (!((i == imax) & (j == jmax))) { /* we are done */
	zprintf(f, ",");
      }
    }
    zprintf(f, "]");
  }

}

void tls_printf(const struct tls_information *data, const struct tls_information *data_twin, zfile f) {
  int i;

  if (!data->tls_v && (data_twin == NULL || !data_twin->tls_v)) { // no reliable TLS information
    return ;
  }
  zprintf(f, ",\"tls\":{");

  if (data->tls_v) {
    zprintf(f, "\"tls_ov\":%u", data->tls_v);
  }
  if (data_twin && data_twin->tls_v) {
    if (data->tls_v) {
      zprintf(f, ",\"tls_iv\":%u", data_twin->tls_v);
    } else {
      zprintf(f, "\"tls_iv\":%u", data_twin->tls_v);
    }
  }

  if (data->tls_client_key_length) {
    zprintf(f, ",\"tls_client_key_length\":%u", data->tls_client_key_length);
  }
  if (data_twin && data_twin->tls_client_key_length) {
    zprintf(f, ",\"tls_client_key_length\":%u", data_twin->tls_client_key_length);
  }

  /*
   * print out TLS random, using the ciphersuite count as a way to
   * determine whether or not we have seen a clientHello or a
   * serverHello
   */

  if (data->num_ciphersuites) {
    zprintf(f, ",\"tls_orandom\":");
    zprintf_raw_as_hex_tls(f, data->tls_random, 32);
  }
  if (data_twin && data_twin->num_ciphersuites) {
    zprintf(f, ",\"tls_irandom\":");
    zprintf_raw_as_hex_tls(f, data_twin->tls_random, 32);
  }

  if (data->tls_sid_len) {
    zprintf(f, ",\"tls_osid\":");
    zprintf_raw_as_hex_tls(f, data->tls_sid, data->tls_sid_len);
  }
  if (data_twin && data_twin->tls_sid_len) {
    zprintf(f, ",\"tls_isid\":");
    zprintf_raw_as_hex_tls(f, data_twin->tls_sid, data_twin->tls_sid_len);
  }

  if (data->num_ciphersuites) {
    if (data->num_ciphersuites == 1) {
      zprintf(f, ",\"scs\":\"%04x\"", data->ciphersuites[0]);
    } else {
      zprintf(f, ",\"cs\":[");
      for (i = 0; i < data->num_ciphersuites-1; i++) {
	zprintf(f, "\"%04x\",", data->ciphersuites[i]);
      }
      zprintf(f, "\"%04x\"]", data->ciphersuites[i]);
    }
  }  
  if (data_twin && data_twin->num_ciphersuites) {
    if (data_twin->num_ciphersuites == 1) {
      zprintf(f, ",\"scs\":\"%04x\"", data_twin->ciphersuites[0]);
    } else {
      zprintf(f, ",\"cs\":[");
      for (i = 0; i < data_twin->num_ciphersuites-1; i++) {
	zprintf(f, "\"%04x\",", data_twin->ciphersuites[i]);
      }
      zprintf(f, "\"%04x\"]", data_twin->ciphersuites[i]);
    }
  }    
  
  if (data->num_tls_extensions) {
    zprintf(f, ",\"tls_ext\":[");
    for (i = 0; i < data->num_tls_extensions-1; i++) {
      zprintf(f, "{\"type\":\"%04x\",", data->tls_extensions[i].type);
      zprintf(f, "\"length\":%i,\"data\":", data->tls_extensions[i].length);
      zprintf_raw_as_hex_tls(f, data->tls_extensions[i].data, data->tls_extensions[i].length);
      zprintf(f, "},");
    }
    zprintf(f, "{\"type\":\"%04x\",", data->tls_extensions[i].type);
    zprintf(f, "\"length\":%i,\"data\":", data->tls_extensions[i].length);
    zprintf_raw_as_hex_tls(f, data->tls_extensions[i].data, data->tls_extensions[i].length);
    zprintf(f, "}]");
  }  
  if (data_twin && data_twin->num_tls_extensions) {
    zprintf(f, ",\"tls_ext\":[");
    for (i = 0; i < data_twin->num_tls_extensions-1; i++) {
      zprintf(f, "{\"type\":\"%04x\",", data_twin->tls_extensions[i].type);
      zprintf(f, "\"length\":%i,\"data\":", data_twin->tls_extensions[i].length);
      zprintf_raw_as_hex_tls(f, data_twin->tls_extensions[i].data, data_twin->tls_extensions[i].length);
      zprintf(f, "},");
    }
    zprintf(f, "{\"type\":\"%04x\",", data_twin->tls_extensions[i].type);
    zprintf(f, "\"length\":%i,\"data\":", data_twin->tls_extensions[i].length);
    zprintf_raw_as_hex_tls(f, data_twin->tls_extensions[i].data, data_twin->tls_extensions[i].length);
    zprintf(f, "}]");
  }
  
  if (data->num_server_tls_extensions) {
    zprintf(f, ",\"s_tls_ext\":[");
    for (i = 0; i < data->num_server_tls_extensions-1; i++) {
      zprintf(f, "{\"type\":\"%04x\",", data->server_tls_extensions[i].type);
      zprintf(f, "\"length\":%i,\"data\":", data->server_tls_extensions[i].length);
      zprintf_raw_as_hex_tls(f, data->server_tls_extensions[i].data, data->server_tls_extensions[i].length);
      zprintf(f, "},");
    }
    zprintf(f, "{\"type\":\"%04x\",", data->server_tls_extensions[i].type);
    zprintf(f, "\"length\":%i,\"data\":", data->server_tls_extensions[i].length);
    zprintf_raw_as_hex_tls(f, data->server_tls_extensions[i].data, data->server_tls_extensions[i].length);
    zprintf(f, "}]");
  }  
  if (data_twin && data_twin->num_server_tls_extensions) {
    zprintf(f, ",\"s_tls_ext\":[");
    for (i = 0; i < data_twin->num_server_tls_extensions-1; i++) {
      zprintf(f, "{\"type\":\"%04x\",", data_twin->server_tls_extensions[i].type);
      zprintf(f, "\"length\":%i,\"data\":", data_twin->server_tls_extensions[i].length);
      zprintf_raw_as_hex_tls(f, data_twin->server_tls_extensions[i].data, data_twin->server_tls_extensions[i].length);
      zprintf(f, "},");
    }
    zprintf(f, "{\"type\":\"%04x\",", data_twin->server_tls_extensions[i].type);
    zprintf(f, "\"length\":%i,\"data\":", data_twin->server_tls_extensions[i].length);
    zprintf_raw_as_hex_tls(f, data_twin->server_tls_extensions[i].data, data_twin->server_tls_extensions[i].length);
    zprintf(f, "}]");
  }

  if (data->num_certificates) {
    zprintf(f, ",\"server_cert\":[");
    for (i = 0; i < data->num_certificates-1; i++) {
      certificate_printf(&data->certificates[i], f);
      zprintf(f, "},");
    }
    certificate_printf(&data->certificates[i], f);    
    zprintf(f, "}]");
  }
  if (data_twin && data_twin->num_certificates) {
    zprintf(f, ",\"server_cert\":[");
    for (i = 0; i < data_twin->num_certificates-1; i++) {
      certificate_printf(&data_twin->certificates[i], f);
      zprintf(f, "},");
    }
    certificate_printf(&data_twin->certificates[i], f);    
    zprintf(f, "}]");
  }  
    /* print out TLS application data lengths and times, if any */

    if (data->tls_op) {
      if (data_twin) {
	len_time_print_interleaved_tls(data->tls_op, data->tls_len, data->tls_time, data->tls_type,
				       data_twin->tls_op, data_twin->tls_len, data_twin->tls_time, data_twin->tls_type, f);
      } else {
	/*
	 * unidirectional TLS does not typically happen, but if it
	 * does, we need to pass in zero/NULLs, since there is no twin
	 */
	len_time_print_interleaved_tls(data->tls_op, data->tls_len, data->tls_time, data->tls_type, 0, NULL, NULL, NULL, f);
      }
    }
 
  zprintf(f, "}");
}

void certificate_printf(const struct tls_certificate *data, zfile f) {
  int j;

    zprintf(f, "{\"length\":%i,", data->length);
    zprintf(f, "\"serial_number\":");
    zprintf_raw_as_hex_tls(f, data->serial_number, data->serial_number_length);
    
    if (data->signature_length) {
      zprintf(f, ",\"signature\":");
      zprintf_raw_as_hex_tls(f, data->signature, data->signature_length);
    }
    if (data->signature_key_size) {
      zprintf(f, ",\"signature_key_size\":%i", data->signature_key_size);
    }
    
    if (data->num_issuer) {
      zprintf(f, ",\"issuer\":[");
      for (j = 0; j < data->num_issuer-1; j++) {
	zprintf(f, "{\"issuer_id\":");
	zprintf_raw_as_hex_tls(f, data->issuer_id[j], data->issuer_id_length[j]);
	zprintf(f, ",\"issuer_string\":\"%s\"},", data->issuer_string[j]);
      }
      zprintf(f, "{\"issuer_id\":");
      zprintf_raw_as_hex_tls(f, data->issuer_id[j], data->issuer_id_length[j]);
      zprintf(f, ",\"issuer_string\":\"%s\"}]", data->issuer_string[j]);
    }
    
    if (data->validity_not_before) {
      zprintf(f, ",\"validity_not_before\":\"%s\"", data->validity_not_before);
    }
    if (data->validity_not_after) {
      zprintf(f, ",\"validity_not_after\":\"%s\"", data->validity_not_after);
    }
    
    if (data->num_subject) {
      zprintf(f, ",\"subject\":[");
      for (j = 0; j < data->num_subject-1; j++) {
	zprintf(f, "{\"subject_id\":");
	zprintf_raw_as_hex_tls(f, data->subject_id[j], data->subject_id_length[j]);
	zprintf(f, ",\"subject_string\":\"%s\"},", data->subject_string[j]);
      }
      zprintf(f, "{\"subject_id\":");
      zprintf_raw_as_hex_tls(f, data->subject_id[j], data->subject_id_length[j]);
      zprintf(f, ",\"subject_string\":\"%s\"}]", data->subject_string[j]);
    }
    
    if (data->subject_public_key_algorithm_length) {
      zprintf(f, ",\"subject_public_key_algorithm\":");
      zprintf_raw_as_hex_tls(f, data->subject_public_key_algorithm, data->subject_public_key_algorithm_length);
    }
    
    if (data->subject_public_key_size) {
      zprintf(f, ",\"subject_public_key_size\":%i", data->subject_public_key_size);
    }
    
    if (data->num_san) {
      zprintf(f, ",\"SAN\":[");
      for (j = 0; j < data->num_san-1; j++) {
	zprintf(f, "\"%s\",", data->san[j]);
      }
      zprintf(f, "\"%s\"]", data->san[j]);
    }
    
    if (data->num_ext) {
      zprintf(f, ",\"extensions\":[");
      for (j = 0; j < data->num_ext-1; j++) {
	zprintf(f, "{\"ext_id\":");
	zprintf_raw_as_hex_tls(f, data->ext_id[j], data->ext_id_length[j]);
	zprintf(f, ",\"ext_data\":");
	zprintf_raw_as_hex_tls(f, data->ext_data[j], data->ext_data_length[j]);
	zprintf(f, "},");
      }
      zprintf(f, "{\"ext_id\":");
      zprintf_raw_as_hex_tls(f, data->ext_id[j], data->ext_id_length[j]);
      zprintf(f, ",\"ext_data\":");
      zprintf_raw_as_hex_tls(f, data->ext_data[j], data->ext_data_length[j]);
      zprintf(f, "}]");
    } 
}
