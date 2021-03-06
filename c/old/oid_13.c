/*###################################################################
###                                                               ###
### Object ID converter. Matthias Gaertner, 06/1999               ###
### Converted to plain 'C' 07/2001                                ###
###                                                               ###
### Enhanced version by Daniel Marschall, ViaThinkSoft 06/2011    ###
### -- NEW 1.2: 2.48 can also be encoded!                         ###
### -- NEW 1.2: UUIDs (128-bit) are now supported!                ###
###             (requires GMPLib)                                 ###
### -- NEW 1.3: Length can now have more than 1 byte              ###
### -- AS WELL AS SEVERAL BUG FIXES                               ###
###                                                               ###
### To compile with gcc simply use:                               ###
###   gcc -O2 -o oid oid.c -lgmp -lm                              ###
###                                                               ###
### To compile using cl, use:                                     ###
###   cl -DWIN32 -O1 oid.c (+ include gmp library)                ###
###                                                               ###
### Freeware - do with it whatever you want.                      ###
### Use at your own risk. No warranty of any kind.                ###
###                                                               ###
###################################################################*/
/* $Version: 1.3$ */

// MAJOR PROBLEMS:
// - Long OIDS: Buffer overflow in abBinary[] without any segfault?

// MINOR PROBLEMS:
// - A wrong error message is shown when trying to encode "-0.0" or "x"

// NICE TO HAVE:
// - makefile / linuxpackage
// - better make functions instead of putting everything in main() with fprintf...

// NICE TO HAVE (INFINITY-IDEA - NOT IMPORTANT):
// - Remove command line limitation (to allow unlimited long OIDs)
// - Is it possible to detect overflows and therefore output errors?

// -------------------------------------------------------

// Allows OIDs which are bigger than "long"
// Compile with "gcc oid.c -lgmp -lm"
#define is_gmp

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef is_gmp
#include <gmp.h>
#endif

#include <stdbool.h>

#ifndef __STRNICMP_LOCAL
#ifdef WIN32
#define __STRNICMP_LOCAL strnicmp
#else
#define __STRNICMP_LOCAL strncasecmp
#endif
#endif

char			abCommandLine[4096]; // TODO: Supersize me
unsigned char	abBinary[128]; // TODO: Supersize me (?)
unsigned int	nBinary = 0;


#ifdef is_gmp
static void MakeBase128( mpz_t l, int first ) {
	if ( mpz_cmp_si(l, 127) > 0 ) {
		mpz_t l2;
		mpz_init(l2);
		mpz_div_ui(l2, l, 128);
		MakeBase128( l2 , 0 );
	}
	mpz_mod_ui(l, l, 128);
	if ( first ) {
		abBinary[nBinary++] = mpz_get_ui(l);
	} else {
		abBinary[nBinary++] = 0x80 | mpz_get_ui(l);
	}
}
#else
static void MakeBase128( unsigned long l, int first ) {
	if ( l > 127 ) {
		MakeBase128( l / 128, 0 );
	}
	l %= 128;
	if ( first ) {
		abBinary[nBinary++] = (unsigned char)l;
	} else {
		abBinary[nBinary++] = 0x80 | (unsigned char)l;
	}
}
#endif

int main( int argc, char **argv ) {
	char *fOutName = NULL;
	char *fInName = NULL;
	FILE *fOut = NULL;

	int n = 1;
	int nMode = 0;	/* dotted->hex */
	int nCHex = 0;
	int nAfterOption = 0;

	if ( argc == 1 ) {
		fprintf( stderr,
		"OID encoder/decoder 1.3 - Matthias Gaertner 1999/2001, Daniel Marschall 2011 - Freeware\n"
		#ifdef is_gmp
		"GMP Edition\n"
		#endif
		"\nUsage:\n"
		" OID [-C] [-o<outfile>] {-i<infile>|2.999.1}\n"
		"   converts dotted form to ASCII HEX DER output.\n"
		" OID -x [-o<outfile>] {-i<infile>|hex-digits}\n"
		"   decodes ASCII HEX DER and gives dotted form.\n" );
		return 1;
	}

	while ( n < argc ) {
		if ( !nAfterOption && argv[n][0] == '-' ) {
			if ( argv[n][1] == 'x' ) {
				nMode = 1;	/* hex->dotted */
				if ( argv[n][2] != '\0' ) {
					argv[n--] += 2;
					nAfterOption = 1;
				}
			} else if ( argv[n][1] == 'C' ) {
				nMode = 0;
				nCHex = 1;

				if ( argv[n][2] != '\0' ) {
					argv[n--] += 2;
					nAfterOption = 1;
				}
			} else if ( argv[n][1] == 'o' ) {
				if ( argv[n][2] != '\0' ) {
					fOutName = &argv[n][2];
				} else if ( n < argc-1 ) {
					fOutName = argv[++n];
				} else {
					fprintf(stderr,"Incomplete command line.\n");
				}
			} else if ( argv[n][1] == 'i' ) {
				if ( argv[n][2] != '\0' ) {
					fInName = &argv[n][2];
				} else if ( n < argc-1 ) {
					fInName = argv[++n];
				} else {
					fprintf(stderr,"Incomplete command line.\n");
				}
			}
		} else {
			if ( fInName != NULL ) {
				break;
			}

			nAfterOption = 1;
			if ( strlen( argv[n] ) + strlen( abCommandLine ) >= sizeof(abCommandLine)-2 ) {
				fprintf(stderr,"Command line too long.\n");
				return 2;
			}
			strcat( abCommandLine, argv[n] );
			if ( n != argc - 1 && nMode != 1 ) {
				strcat( abCommandLine, "." );
			}
		}
		n++;
	}

	if ( fInName != NULL && nMode == 1 ) {
		FILE *fIn = fopen( fInName, "rb" );
		size_t nRead = 0;
		if ( fIn == NULL ) {
			fprintf(stderr,"Unable to open input file %s.\n", fInName );
			return 11;
		}
		nRead = fread( abCommandLine, 1, sizeof(abCommandLine), fIn );
		abCommandLine[nRead] = '\0';
		fclose( fIn );
	} else if ( fInName != NULL && nMode == 0 ) {
		FILE *fIn = fopen( fInName, "rt" );
		if ( fIn == NULL ) {
			fprintf(stderr,"Unable to open input file %s.\n", fInName );
			return 11;
		}
		fgets( abCommandLine, sizeof(abCommandLine), fIn );
		fclose( fIn );
	}

	while ( nMode == 1 )	/* better if */
	{
		/* hex->dotted */
		/*printf("Hex-In: %s\n", abCommandLine );*/

		char *p = abCommandLine;
		char *q = p;

		unsigned char *pb = NULL;
		unsigned int nn = 0;
		#ifdef is_gmp
		mpz_t ll;
		mpz_init(ll);
		#else
		unsigned long ll = 0;
		#endif
		bool fOK = false;
		int fSub = 0; // Subtract value from next number output. Used when encoding {2 48} and up

		while ( *p ) {
			if ( *p != '.' && *p != '\r' && *p != '\n' && *p != '\x20' && *p != '\t') {
				*q++ = *p;
			}
			p++;
		}
		*q = '\0';

		if ( strlen( abCommandLine ) % 2 != 0 ) {
			fprintf(stderr, "Encoded OID must have even number of hex digits!\n" );
			return 2;
		}

		if ( strlen( abCommandLine ) < 3 ) {
			fprintf(stderr, "Encoded OID must have at least three bytes!\n" );
			return 2;
		}

		nBinary = 0;
		p = abCommandLine;

		while ( *p ) {
			unsigned char b;

			// Interpret upper nibble
			if ( p[0] >= 'A' && p[0] <= 'F' ) {
				b = (p[0] - 'A' + 10) * 16;
			} else if ( p[0] >= 'a' && p[0] <= 'f' ) {
				b = (p[0] - 'a' + 10) * 16;
			} else if ( p[0] >= '0' && p[0] <= '9' ) {
				b = (p[0] - '0') * 16;
			} else {
				fprintf(stderr, "Must have hex digits only!\n" );
				return 2;
			}

			// Interpret lower nibble
			if ( p[1] >= 'A' && p[1] <= 'F' ) {
				b += (p[1] - 'A' + 10);
			} else if ( p[1] >= 'a' && p[1] <= 'f' ) {
				b += (p[1] - 'a' + 10);
			} else if ( p[1] >= '0' && p[1] <= '9' ) {
				b += (p[1] - '0');
			} else {
				fprintf(stderr, "Must have hex digits only!\n" );
				return 2;
			}

			abBinary[nBinary++] = b;
			p += 2;
		}

		/*printf("Hex-In: %s\n", abCommandLine );*/

		if ( fOutName != NULL ) {
			fOut = fopen( fOutName, "wt" );
			if ( fOut == NULL ) {
				fprintf(stderr,"Unable to open output file %s\n", fOutName );
				return 33;
			}
		} else {
			fOut = stdout;
		}

		pb = abBinary;
		nn = 0;
		#ifdef is_gmp
		mpz_init(ll);
		#else
		ll = 0;
		#endif
		fOK = false;
		fSub = 0;

		// 0 = Universal Class Identifier Tag
		// 1 = Length part (may have more than 1 byte!)
		// 2 = First two arc encoding
		// 3 = Encoding of arc three and higher
		unsigned char part = 0;

		unsigned char lengthbyte_count = 0;
		unsigned char lengthbyte_pos = 0;
		bool lengthfinished = false;

		while ( nn < nBinary ) {
			if ( part == 0 ) { // Class Tag
				unsigned char cl = ((*pb & 0xC0) >> 6) & 0x03;
				switch ( cl ) {
					default:
					case 0: fprintf(fOut,"UNIVERSAL"); break;
					case 1: fprintf(fOut,"APPLICATION"); break;
					case 2: fprintf(fOut,"CONTEXT"); break;
					case 3: fprintf(fOut,"PRIVATE"); break;
				}
				fprintf(fOut," OID");
				part++;
			} else if ( part == 1 ) { // Length

				// Find out the length and save it into ll

				// 2nd Byte
				// 0x00 .. 0x7F => The actual length
				// 0x80 + n => The length is spread over the following 'n' bytes
				// (Unknown: Is 'n' limited or can it be until 0xFF is reached?)
				// (Unknown: How is length=0x80 (=0 following bytes define the length) defined? Is it the same as 0x00?)

				if (nn == 1) { // The first length byte
					lengthbyte_pos = 0;
					if ( (*pb & 0x80) != 0 ) {
						// 0x80 + n => The length is spread over the following 'n' bytes
						lengthfinished = false;
						lengthbyte_count = *pb & 0x7F;
						fOK = false;
					} else {
						// 0x00 .. 0x7F => The actual length
						#ifdef is_gmp
						mpz_set_ui(ll, *pb);
						#else
						ll = *pb;
						#endif
						lengthfinished = true;
						lengthbyte_count = 0;
						fOK = true;
					}
				} else {
					if (lengthbyte_count > lengthbyte_pos) {
						#ifdef is_gmp
						mpz_mul_ui(ll, ll, 0x100);
						mpz_add_ui(ll, ll, *pb);
						#else
						ll *= 0x100;
						ll += *pb;
						#endif
						lengthbyte_pos++;
					}

					if (lengthbyte_count == lengthbyte_pos) {
						lengthfinished = true;
						fOK = true;
					}
				}

				if (lengthfinished) { // The length is now in ll
					#ifdef is_gmp
					if ( mpz_cmp_ui(ll,  nBinary - 2 - lengthbyte_count) != 0 ) {
						fprintf(fOut,"\n");
						if ( fOut != stdout ) {
							fclose( fOut );
						}
						fprintf(stderr,"\nInvalid length (%d entered, but %s expected)\n", nBinary - 2, mpz_get_str(NULL, 10, ll) );
						return 3;
					}
					mpz_set_ui(ll, 0); // reset for later usage
					#else
					if ( ll != nBinary - 2 - lengthbyte_count ) {
						fprintf(fOut,"\n");
						if ( fOut != stdout ) {
							fclose( fOut );
						}
						fprintf(stderr,"\nInvalid length (%d entered, but %d expected)\n", nBinary - 2, ll );
						return 3;
					}
					ll = 0; // reset for later usage
					#endif
					fOK = true;
					part++;
				}
			} else if ( part == 2 ) { // First two arcs
				int first = *pb / 40;
				int second = *pb % 40;
				if (first > 2) {
					first = 2;
					fprintf(fOut,".%d", first );

					if ( (*pb & 0x80) != 0 ) {
						// 2.48 and up => 2+ octets
						#ifdef is_gmp
						mpz_add_ui(ll, ll, (*pb & 0x7F));
						#else
						ll += (*pb & 0x7F);
						#endif
						fSub = 80;
						fOK = false;
					} else {
						// 2.0 till 2.47 => 1 octet
						second = *pb - 80;
						fprintf(fOut,".%d",second);
						fOK = true;
						#ifdef is_gmp
						mpz_set_ui(ll, 0);
						#else
						ll = 0;
						#endif
					}
				} else {
					// 0.0 till 0.37 => 1 octet
					// 1.0 till 1.37 => 1 octet
					fprintf(fOut,".%d.%d", first, second );
					fOK = true;
					#ifdef is_gmp
					mpz_set_ui(ll, 0);
					#else
					ll = 0;
					#endif
				}
				part++;
			} else if ( part == 3 ) { // Arc three and higher
				if ( (*pb & 0x80) != 0 ) {
					#ifdef is_gmp
					mpz_mul_ui(ll, ll, 128);
					mpz_add_ui(ll, ll, (*pb & 0x7F));
					#else
					ll *= 128;
					ll += (*pb & 0x7F);
					#endif
					fOK = false;
				} else {
					#ifdef is_gmp
					mpz_mul_ui(ll, ll, 128);
					mpz_add_ui(ll, ll, *pb);
					mpz_sub_ui(ll, ll, fSub);
					fprintf(fOut,".%s", mpz_get_str(NULL, 10, ll) );
					mpz_set_ui(ll, 0);
					#else
					ll *= 128;
					ll += *pb;
					ll -= fSub;
					fprintf(fOut,".%lu", ll );
					ll = 0;
					#endif
					fSub = 0;
					fOK = true;
				}
			}

			pb++;
			nn++;
		}

		if ( !fOK ) {
			fprintf(fOut,"\n");
			if ( fOut != stdout ) {
				fclose( fOut );
			}
			fprintf(stderr,"\nEncoding error. The OID is not constructed properly.\n");
			return 4;
		} else {
			fprintf(fOut,"\n");
		}

		if ( fOut != stdout ) {
			fclose( fOut );
		}
		break;
	};

	while ( nMode == 0 )	/* better if */
	{
		/* dotted->hex */
		/* printf("OID.%s\n", abCommandLine ); */

		char *p = abCommandLine;
		unsigned char cl = 0x00;
		char *q = NULL;
		int nPieces = 1;
		int n = 0;
		unsigned char b = 0;
		unsigned int nn = 0;
		#ifdef is_gmp
		mpz_t l;
		#else
		unsigned long l = 0;
		#endif
		bool isjoint = false;

		if ( __STRNICMP_LOCAL( p, "UNIVERSAL.", 10 ) == 0 ) {
			p+=10;
		} else if ( __STRNICMP_LOCAL( p, "APPLICATION.", 12 ) == 0 ) {
			cl = 0x40;
			p+=12;
		} else if ( __STRNICMP_LOCAL( p, "CONTEXT.", 8 ) == 0 ) {
			cl = 0x80;
			p+=8;
		} else if ( __STRNICMP_LOCAL( p, "PRIVATE.", 8 ) == 0 ) {
			cl = 0xC0;
			p+=8;
		}

		if ( __STRNICMP_LOCAL( p, "OID.", 4 ) == 0 ) {
			p+=4;
		}

		q = p;
		nPieces = 1;
		while ( *p ) {
			if ( *p == '.' ) {
				nPieces++;
			}
			p++;
		}

		n = 0;
		b = 0;
		p = q;
		while ( n < nPieces ) {
			q = p;
			while ( *p ) {
				if ( *p == '.' ) {
					break;
				}
				p++;
			}

			#ifdef is_gmp
			mpz_init(l);
			#else
			l = 0;
			#endif
			if ( *p == '.' ) {
				*p = 0;
				#ifdef is_gmp
				mpz_set_str(l, q, 10);
				#else
				l = (unsigned long) atoi( q );
				#endif
				q = p+1;
				p = q;
			} else {
				#ifdef is_gmp
				mpz_set_str(l, q, 10);
				#else
				l = (unsigned long) atoi( q );
				#endif
				q = p;
			}

			/* Digit is in l. */
			if ( n == 0 ) {
				#ifdef is_gmp
				if (mpz_cmp_ui(l, 2) > 0) {
				#else
				if (l > 2) {
				#endif
					fprintf(stderr,"\nEncoding error. The top arc is limited to 0, 1 and 2.\n");
					return 5;
				}
				#ifdef is_gmp
				b += 40 * mpz_get_ui(l);
				isjoint = mpz_cmp_ui(l, 2) == 0;
				#else
				b = 40 * ((unsigned char)l);
				isjoint = l == 2;
				#endif
			} else if ( n == 1 ) {
				#ifdef is_gmp
				if ((mpz_cmp_ui(l, 39) > 0) && (!isjoint)) {
				#else
				if ((l > 39) && (!isjoint)) {
				#endif
					fprintf(stderr,"\nEncoding error. The second level is limited to 0..39 for top level arcs 0 and 1.\n");
					return 5;
				}

				#ifdef is_gmp
				if (mpz_cmp_ui(l, 47) > 0) {
					mpz_add_ui(l, l, 80);
					MakeBase128( l, 1 );
				} else {
					b += mpz_get_ui(l);
					abBinary[nBinary++] = b;
				}
				#else
				if (l > 47) {
					l += 80;
					MakeBase128( l, 1 );
				} else {
					b += ((unsigned char) l);
					abBinary[nBinary++] = b;
				}
				#endif
			} else {
				MakeBase128( l, 1 );
			}
			n++;
		}

		if (n < 2) {
			fprintf(stderr,"\nEncoding error. The minimum depth of an encodeable OID is 2. (e.g. 2.999)\n");
			return 5;
		}

		if ( fOutName != NULL ) {
			fOut = fopen( fOutName, "wt" );
			if ( fOut == NULL ) {
				fprintf(stderr,"Unable to open output file %s\n", fOutName );
				return 33;
			}
		} else {
			fOut = stdout;
		}

		if ( nCHex ) {
			fprintf(fOut,"\"\\x%02X\\x%02X", cl | 6, nBinary );
		} else {
			fprintf(fOut,"%02X %02X ", cl | 6, nBinary );
		}
		nn = 0;
		while ( nn < nBinary ) {
			unsigned char b = abBinary[nn++];
			if ( nn == nBinary ) {
				if ( nCHex ) {
					fprintf(fOut,"\\x%02X\"\n", b );
				} else {
					fprintf(fOut,"%02X\n", b );
				}
			} else {
				if ( nCHex ) {
					fprintf(fOut,"\\x%02X", b );
				} else {
					fprintf(fOut,"%02X ", b );
				}
			}
		}
		if ( fOut != stdout ) {
			fclose( fOut );
		}
		break;
	}

	return 0;
}

/* */

