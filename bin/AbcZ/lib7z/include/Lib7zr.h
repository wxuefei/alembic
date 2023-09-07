// Lib7zr.h
extern int lib7z_compress(const char* filename, const char* basefile, const char* patchfile);
extern int lib7z_list(char* filename);
extern int lib7z_decompress(char* filename);
extern int lib7z_list_num();
extern void lib7z_list_get(int i, char* path, unsigned __int64& filesize, unsigned __int64& packsize);
