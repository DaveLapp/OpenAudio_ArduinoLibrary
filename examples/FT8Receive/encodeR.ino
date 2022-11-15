/*
 * encodeR.ino
 * Basically the Goba encode.h and .c with minor changes for Teensy
 * Arduino use along with the floating point OpenAudio_ArduinoLibrary.
 * Bob Larkin W7PUA, September 2022.
 *
 */

/* Thank you to Kārlis Goba, YL3JG, https://github.com/kgoba/ft8_lib
 * and to Charley Hill, W5BAA, https://github.com/Rotron/Pocket-FT8
 * as well as the all the contributors to the Joe Taylor WSJT project.
 * See "The FT4 and FT8 Communication Protocols," Steve Franks, K9AN,
 * Bill Somerville, G4WJS and Joe Taylor, K1JT, QEX July/August 2020
 * pp 7-17 as well as https://www.physics.princeton.edu/pulsar/K1JT
 */

/*  *****  MIT License  ***

Copyright (c) 2018 Kārlis Goba

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// This ino is needed for transmit FT8, but in order to get crc(),
// it is needed for receive also.

#if 0
// REMOVE FOR RECEIVE ONLY FILE

// Returns 1 if an odd number of bits are set in x, zero otherwise
uint8_t parity8(uint8_t x) {
    x ^= x >> 4;    // a b c d ae bf cg dh
    x ^= x >> 2;    // a b ac bd cae dbf aecg bfdh
    x ^= x >> 1;    // a ab bac acbd bdcae caedbf aecgbfdh
    return (x) & 1;
}


// Encode a 91-bit message and return a 174-bit codeword.
// The generator matrix has dimensions (87,87).
// The code is a (174,91) regular ldpc code with column weight 3.
// The code was generated using the PEG algorithm.
// Arguments:
// [IN] message   - array of 91 bits stored as 12 bytes (MSB first)
// [OUT] codeword - array of 174 bits stored as 22 bytes (MSB first)
void encode174(const uint8_t *message, uint8_t *codeword) {
    // Here we don't generate the generator bit matrix as in WSJT-X implementation
    // Instead we access the generator bits straight from the binary representation in kGenerator

    // For reference:
    // codeword(1:K)=message
    // codeword(K+1:N)=pchecks

    // printf("Encode ");
    // for (int i = 0; i < K_BYTES; ++i) {
    //     printf("%02x ", message[i]);
    // }
    // printf("\n");

    // Fill the codeword with message and zeros, as we will only update binary ones later
    for (int j = 0; j < (7 + N) / 8; ++j) {
        codeword[j] = (j < K_BYTES) ? message[j] : 0;
    }

    uint8_t col_mask = (0x80 >> (K % 8));   // bitmask of current byte
    uint8_t col_idx = K_BYTES - 1;          // index into byte array

    // Compute the first part of itmp (1:M) and store the result in codeword
    for (int i = 0; i < M; ++i) { // do i=1,M
        // Fast implementation of bitwise multiplication and parity checking
        // Normally nsum would contain the result of dot product between message and kGenerator[i],
        // but we only compute the sum modulo 2.
        uint8_t nsum = 0;
        for (int j = 0; j < K_BYTES; ++j) {
            uint8_t bits = message[j] & kGenerator[i][j];    // bitwise AND (bitwise multiplication)
            nsum ^= parity8(bits);                  // bitwise XOR (addition modulo 2)
        }
        // Check if we need to set a bit in codeword
        if (nsum % 2) { // pchecks(i)=mod(nsum,2)
            codeword[col_idx] |= col_mask;
        }

        col_mask >>= 1;
        if (col_mask == 0) {
            col_mask = 0x80;
            ++col_idx;
        }
    }

    // printf("Result ");
    // for (int i = 0; i < (N + 7) / 8; ++i) {
    //     printf("%02x ", codeword[i]);
    // }
    // printf("\n");
}
#endif


// Compute 14-bit CRC for a sequence of given number of bits
// [IN] message  - byte sequence (MSB first)
// [IN] num_bits - number of bits in the sequence
uint16_t crc(uint8_t *message, int num_bits) {
    // Adapted from https://barrgroup.com/Embedded-Systems/How-To/CRC-Calculation-C-Code
	
	// Define CRC parameters
    uint16_t CRC_POLYNOMIAL = 0X2757;  // CRC-14 polynomial without the leading (MSB) 1
    int      CRC_WIDTH = 14;
    //constexpr uint16_t  TOPBIT = (1 << (CRC_WIDTH - 1));
    uint16_t  TOPBIT = (1 << (CRC_WIDTH - 1));
    // printf("CRC, %d bits: ", num_bits);
    // for (int i = 0; i < (num_bits + 7) / 8; ++i) {
    //     printf("%02x ", message[i]);
    // }
    // printf("\n");

    uint16_t remainder = 0;
    int idx_byte = 0;

    // Perform modulo-2 division, a bit at a time.
    for (int idx_bit = 0; idx_bit < num_bits; ++idx_bit) {
        if (idx_bit % 8 == 0) {
            // Bring the next byte into the remainder.
            remainder ^= (message[idx_byte] << (CRC_WIDTH - 8));
            ++idx_byte;
        }

        // Try to divide the current data bit.
        if (remainder & TOPBIT) {
            remainder = (remainder << 1) ^ CRC_POLYNOMIAL;
        }
        else {
            remainder = (remainder << 1);
        }
    }
    // printf("CRC = %04xh\n", remainder & ((1 << CRC_WIDTH) - 1));
    return remainder & ((1 << CRC_WIDTH) - 1);
}

#if 0
// REMOVE FOR RECEIVE ONLY FILE

// Generate FT8 tone sequence from payload data
// [IN] payload - 10 byte array consisting of 77 bit payload (MSB first)
// [OUT] itone  - array of NN (79) bytes to store the generated tones (encoded as 0..7)
void genft8(const uint8_t *payload, uint8_t *itone) {
    uint8_t a91[12];    // Store 77 bits of payload + 14 bits CRC

    // Copy 77 bits of payload data
    for (int i = 0; i < 10; i++)
        a91[i] = payload[i];

    // Clear 3 bits after the payload to make 80 bits
    a91[9] &= 0xF8;
    a91[10] = 0;
    a91[11] = 0;

    // Calculate CRC of 12 bytes = 96 bits, see WSJT-X code
    uint16_t checksum = crc(a91, 96 - 14);

    // Store the CRC at the end of 77 bit message
    a91[9] |= (uint8_t)(checksum >> 11);
    a91[10] = (uint8_t)(checksum >> 3);
    a91[11] = (uint8_t)(checksum << 5);

    // a87 contains 77 bits of payload + 14 bits of CRC
    uint8_t codeword[22];
    encode174(a91, codeword);

    // Message structure: S7 D29 S7 D29 S7
    for (int i = 0; i < 7; ++i) {
        itone[i]      = kCostas_map[i];
        itone[36 + i] = kCostas_map[i];
        itone[72 + i] = kCostas_map[i];
    }

    int k = 7;          // Skip over the first set of Costas symbols

    uint8_t mask = 0x80;
    int i_byte = 0;
    for (int j = 0; j < ND; ++j) { // do j=1,ND
        if (j == 29) {
            k += 7;     // Skip over the second set of Costas symbols
        }

        // Extract 3 bits from codeword at i-th position
        uint8_t bits3 = 0;

        if (codeword[i_byte] & mask) bits3 |= 4;
        if (0 == (mask >>= 1)) { mask = 0x80; i_byte++; }
        if (codeword[i_byte] & mask) bits3 |= 2;
        if (0 == (mask >>= 1)) { mask = 0x80; i_byte++; }
        if (codeword[i_byte] & mask) bits3 |= 1;
        if (0 == (mask >>= 1)) { mask = 0x80; i_byte++; }

        itone[k] = kGray_map[bits3];
        ++k;
    }
}
#endif
