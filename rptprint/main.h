typedef struct node_value{
	char arr[4096];			 // 4k of character space
	struct node_value *next; // next value in linked list
} NODE_VALUE;

std::vector<struct node_value*> nodes;
//NODE_VALUE *root;
const char inifile[] = "settings.ini";
char watchDir[4097] = "\0";
char watchFiles[5047] = "\0";
char watchExt[51] = "\0";
char watchCmd[4097] = "\0";

bool verbose = false;
void dprintf(char *b, ...)
{
	va_list arglist;
    va_start(arglist,b);

	if( verbose )
		vprintf(b,arglist);

    va_end(arglist);
}