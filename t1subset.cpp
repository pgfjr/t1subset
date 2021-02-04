

#include "t1subset.h"


int main(int argc, char* argv[])
{
	t1subset t1;
	string error;
	const byte_t sub_chars[] = { 33, 34, 35 }; // replace these with the characters you want to subset


	// Make sure these fonts are in the current directory or replace them with your desired fonts

	if (!t1.subset_font("cmr10.pfb", sub_chars, sizeof(sub_chars), "cmr10-sub.pfb", error))
	{
		printf("Error subsetting 'cmr10.pfb': %s\n", error.c_str());
	}
	else
	{
		puts("Success subsetting 'cmr10.pfb'");
	}
	if (!t1.subset_font("d:\\lmroman9.pfb", sub_chars, 3, "lmroman9-sub.pfb", error))
	{
		printf("Error subsetting 'lmroman9.pfb': %s\n", error.c_str());
	}
	else
	{
		puts("Success subsetting 'lmroman9.pfb'");
	}
	return 0;
}



