#pragma once

extern "C" {

	int mz_compress2(unsigned char* pDest, unsigned long* pDest_len, const unsigned char* pSource, unsigned long source_len, int level);

	int mz_uncompress(unsigned char* pDest, unsigned long* pDest_len, const unsigned char* pSource, unsigned long source_len);

	unsigned long mz_compressBound(unsigned long source_len);

}
#define _MZ_OK 0